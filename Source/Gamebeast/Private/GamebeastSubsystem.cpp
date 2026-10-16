// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "GamebeastSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GamebeastCohorts.h"
#include "GamebeastConfigs.h"
#include "GamebeastExperiments.h"
#include "GamebeastMarkers.h"
#include "GamebeastSettings.h"
#include "HAL/PlatformTime.h"
#include "Internal/GamebeastApiClient.h"
#include "Internal/GamebeastContext.h"
#include "Internal/GamebeastEnvironment.h"
#include "Internal/GamebeastLog.h"
#include "Internal/GamebeastRates.h"
#include "Misc/CoreDelegates.h"

/** `DISTINCT_ID_MAX_LENGTH` in @gamebeast/sdk-contract. */
static constexpr int32 MaxDistinctIdLength = 256;

UGamebeastSubsystem* UGamebeastSubsystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr || GEngine == nullptr)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UGamebeastSubsystem>() : nullptr;
}

void UGamebeastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Services exist before Setup so Blueprint getters are never null; they no-op until started.
	Markers = NewObject<UGamebeastMarkers>(this);
	Configs = NewObject<UGamebeastConfigs>(this);
	Experiments = NewObject<UGamebeastExperiments>(this);
	Cohorts = NewObject<UGamebeastCohorts>(this);

	const UGamebeastSettings* Settings = GetDefault<UGamebeastSettings>();
	bAutoInitializePending = Settings->bAutoInitialize && !Settings->ApiKey.TrimStartAndEnd().IsEmpty()
		&& (!IsRunningDedicatedServer() || Settings->bAutoInitializeOnDedicatedServer);

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UGamebeastSubsystem::HandleTick));

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
		this, &UGamebeastSubsystem::HandleWillEnterBackground);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
		this, &UGamebeastSubsystem::HandleHasEnteredForeground);
}

void UGamebeastSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	Shutdown();
	Super::Deinitialize();
}

bool UGamebeastSubsystem::Setup(const FString& ApiKey)
{
	FGamebeastSetupOptions Options = GetDefault<UGamebeastSettings>()->ToSetupOptions();
	Options.ApiKey = ApiKey;
	return SetupWithOptions(Options);
}

bool UGamebeastSubsystem::SetupWithOptions(const FGamebeastSetupOptions& Options)
{
	bAutoInitializePending = false;

	if (Context.IsValid())
	{
		GB_WARN(TEXT("Setup called while the SDK is already running; call Shutdown first. Ignored."));
		return false;
	}
	const FString ApiKey = Options.ApiKey.TrimStartAndEnd();
	if (ApiKey.IsEmpty())
	{
		GB_ERROR(TEXT("Setup needs an API key (Project Settings > Plugins > Gamebeast, or FGamebeastSetupOptions::ApiKey)."));
		return false;
	}

	Gamebeast::Private::GDebugLogging = Options.bDebugLogging;

	const TSharedRef<FGamebeastContext> NewContext = MakeShared<FGamebeastContext>();
	NewContext->Options = Options;
	NewContext->Options.ApiKey = ApiKey;
	NewContext->Rates = Gamebeast::Rates::Resolve(Options, GIsEditor);
	NewContext->EnvironmentAlias =
		Gamebeast::Environment::Resolve(Options.Environment, Options.CustomEnvironment, GIsEditor);
	NewContext->AnonymousId = FGamebeastContext::LoadOrCreateAnonymousId();
	const FString SuppliedId = Options.DistinctId.TrimStartAndEnd();
	if (SuppliedId.Len() > MaxDistinctIdLength)
	{
		GB_WARN(TEXT("DistinctId is longer than %d characters; using the anonymous id."), MaxDistinctIdLength);
	}
	NewContext->DistinctId =
		!SuppliedId.IsEmpty() && SuppliedId.Len() <= MaxDistinctIdLength ? SuppliedId : NewContext->AnonymousId;
	NewContext->SessionId = FGamebeastContext::NewSessionId();
	NewContext->Api = MakeShared<FGamebeastApiClient>(ApiKey, Options.ApiUrl, NewContext->EnvironmentAlias);
	NewContext->Api->SessionId = NewContext->SessionId;
	Context = NewContext;

	Experiments->Start(NewContext);
	Configs->Start(NewContext, Experiments);
	Cohorts->Start(NewContext);
	Markers->Start(NewContext);

	UE_LOG(LogGamebeast, Log, TEXT("Gamebeast SDK %s started (environment: %s)."),
		*FGamebeastApiClient::UserAgentVersion(), *NewContext->EnvironmentAlias);
	GB_INFO(TEXT("Rates: markers every %.0fs, status every %.0fs, assignments every %.0fs%s."),
		NewContext->Rates.MarkerFlushRate, NewContext->Rates.StatusPollRate, NewContext->Rates.AssignmentRefreshRate,
		GIsEditor && Options.bFastRatesInEditor ? TEXT(" (editor minimums; see bFastRatesInEditor)") : TEXT(""));
	return true;
}

void UGamebeastSubsystem::UpdateRates(const FGamebeastRates& Rates)
{
	if (!Context.IsValid())
	{
		GB_WARN(TEXT("UpdateRates called before Setup; set Rates in FGamebeastSetupOptions or Project Settings instead."));
		return;
	}
	// An explicit call wins over the editor minimums.
	Context->Options.Rates = Rates;
	Context->Rates = Gamebeast::Rates::Validate(Rates);
	Markers->OnRatesChanged();
	Configs->OnRatesChanged();
}

FGamebeastRates UGamebeastSubsystem::GetRates() const
{
	return Context.IsValid() ? Context->Rates : FGamebeastRates();
}

void UGamebeastSubsystem::Shutdown()
{
	if (!Context.IsValid())
	{
		return;
	}
	Markers->Stop();
	Cohorts->Stop();
	Configs->Stop();
	Experiments->Stop();
	Context.Reset();
	GB_INFO(TEXT("Gamebeast SDK shut down."));
}

bool UGamebeastSubsystem::IsInitialized() const
{
	return Context.IsValid();
}

FString UGamebeastSubsystem::GetEnvironment() const
{
	return Context.IsValid() ? Context->EnvironmentAlias : FString();
}

FString UGamebeastSubsystem::GetDistinctId() const
{
	return Context.IsValid() ? Context->DistinctId : FString();
}

FString UGamebeastSubsystem::GetSessionId() const
{
	return Context.IsValid() ? Context->SessionId : FString();
}

void UGamebeastSubsystem::Identify(const FString& DistinctId)
{
	const FString Id = DistinctId.TrimStartAndEnd();
	if (Id.IsEmpty() || Id.Len() > MaxDistinctIdLength)
	{
		GB_ERROR(TEXT("Identify needs a distinct id of 1-%d characters."), MaxDistinctIdLength);
		return;
	}
	SetIdentity(Id);
}

void UGamebeastSubsystem::ResetIdentity()
{
	if (Context.IsValid())
	{
		SetIdentity(Context->AnonymousId);
	}
}

void UGamebeastSubsystem::SetIdentity(const FString& DistinctId)
{
	if (!Context.IsValid())
	{
		GB_WARN(TEXT("Identify called before Setup; pass DistinctId in FGamebeastSetupOptions instead."));
		return;
	}
	if (Context->DistinctId == DistinctId)
	{
		return;
	}
	Context->DistinctId = DistinctId;
	++Context->IdentityGeneration;
	GB_INFO(TEXT("Identity changed; re-evaluating configurations."));

	// Markers stamp the id when recorded, so they need nothing here.
	Experiments->OnIdentityChanged();
	Cohorts->OnIdentityChanged();
	Configs->OnIdentityChanged();
}

bool UGamebeastSubsystem::HandleTick(float DeltaSeconds)
{
	if (bAutoInitializePending)
	{
		// Deferred to the first frame so a Setup call in GameInstance::Init wins.
		bAutoInitializePending = false;
		if (!Context.IsValid())
		{
			SetupWithOptions(GetDefault<UGamebeastSettings>()->ToSetupOptions());
		}
	}

	if (Context.IsValid() && !bInBackground)
	{
		Markers->Tick(DeltaSeconds);
		Configs->Tick(DeltaSeconds);
	}
	return true;
}

void UGamebeastSubsystem::HandleWillEnterBackground()
{
	bInBackground = true;
	BackgroundedAt = FPlatformTime::Seconds();
	if (Context.IsValid())
	{
		// The OS may kill a backgrounded game without warning: keep what's buffered on disk.
		Markers->PersistUnsent();
	}
}

void UGamebeastSubsystem::HandleHasEnteredForeground()
{
	bInBackground = false;
	if (!Context.IsValid())
	{
		return;
	}

	const float Timeout = Context->Options.SessionTimeoutSeconds;
	if (Timeout > 0.f && FPlatformTime::Seconds() - BackgroundedAt >= Timeout)
	{
		Context->SessionId = FGamebeastContext::NewSessionId();
		Context->Api->SessionId = Context->SessionId;
		GB_INFO(TEXT("Started a new session after %.0fs in the background."), FPlatformTime::Seconds() - BackgroundedAt);
	}

	Markers->RestorePersisted();
	Configs->OnForeground();
}
