// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastConfigs.h"

#include "GamebeastExperiments.h"
#include "Gamebeast.h"
#include "Internal/GamebeastContext.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastLog.h"
#include "Internal/GamebeastStorage.h"

namespace
{
	/** Configurations that have never loaded retry this often, even when refresh is off. */
	constexpr float UnloadedRetrySeconds = 10.f;
	constexpr int32 CacheFormatVersion = 1;
}

// --- Lifecycle ---------------------------------------------------------------------------------

void UGamebeastConfigs::Start(const TSharedRef<FGamebeastContext>& InContext, UGamebeastExperiments* InExperiments)
{
	Context = InContext;
	Experiments = InExperiments;

	for (const FString& Alias : InContext->Options.Configurations)
	{
		const FString Trimmed = Alias.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			EnsureConfig(Trimmed, /*bRequiredForReady*/ true);
		}
	}
	CheckReady();
	Refresh();
}

void UGamebeastConfigs::Stop()
{
	Context.Reset();
	Configs.Reset();
	Listeners.Reset();
	ReadyCallbacks.Reset();
	bReady = false;
}

void UGamebeastConfigs::Tick(float DeltaSeconds)
{
	if (!Context.IsValid() || Configs.Num() == 0)
	{
		return;
	}

	SecondsSinceUnloadedRetry += DeltaSeconds;
	if (SecondsSinceUnloadedRetry >= UnloadedRetrySeconds)
	{
		SecondsSinceUnloadedRetry = 0.f;
		TArray<FString> Unloaded;
		for (const TPair<FString, FConfigState>& Pair : Configs)
		{
			if (!Pair.Value.bLoaded && !Pair.Value.bInFlight)
			{
				Unloaded.Add(Pair.Key);
			}
		}
		for (const FString& Alias : Unloaded)
		{
			RefreshConfig(Alias);
		}
	}

	const float Interval = Context->Options.ConfigRefreshIntervalSeconds;
	if (Interval <= 0.f)
	{
		return;
	}
	SecondsSinceRefresh += DeltaSeconds;
	if (SecondsSinceRefresh >= Interval)
	{
		Refresh();
	}
}

void UGamebeastConfigs::OnForeground()
{
	// Configurations may have changed while the game was suspended.
	if (Configs.Num() > 0)
	{
		Refresh();
	}
}

void UGamebeastConfigs::OnIdentityChanged()
{
	// Evaluated content is per user. Drop hashes so everything refetches in full; responses
	// already in flight are discarded by the identity generation check. Current values keep
	// serving until the new user's land.
	for (TPair<FString, FConfigState>& Pair : Configs)
	{
		Pair.Value.Hash.Reset();
	}
	Refresh();
}

// --- Fetching ----------------------------------------------------------------------------------

UGamebeastConfigs::FConfigState* UGamebeastConfigs::EnsureConfig(const FString& Alias, bool bRequiredForReady)
{
	if (FConfigState* Existing = Configs.Find(Alias))
	{
		Existing->bRequiredForReady |= bRequiredForReady;
		return Existing;
	}

	FConfigState& State = Configs.Add(Alias);
	State.Alias = Alias;
	State.bRequiredForReady = bRequiredForReady;

	if (const TSharedPtr<FJsonObject> Cached = LoadCache(Alias))
	{
		ApplyResponse(State, Cached, /*bPersist*/ false);
		GB_INFO(TEXT("Loaded cached configuration '%s'."), *Alias);
	}

	if (!bRequiredForReady && Context.IsValid())
	{
		GB_INFO(TEXT("Configuration '%s' was not listed in Setup; loading it on demand."), *Alias);
		RefreshConfig(Alias);
	}
	// ApplyResponse may have added entries; re-find in case the map reallocated.
	return Configs.Find(Alias);
}

void UGamebeastConfigs::Preload(const FString& ConfigurationAlias)
{
	const FString Alias = ConfigurationAlias.TrimStartAndEnd();
	if (!Alias.IsEmpty())
	{
		EnsureConfig(Alias, /*bRequiredForReady*/ false);
	}
}

void UGamebeastConfigs::Refresh()
{
	SecondsSinceRefresh = 0.f;
	TArray<FString> Aliases;
	Configs.GetKeys(Aliases);
	for (const FString& Alias : Aliases)
	{
		RefreshConfig(Alias);
	}
}

void UGamebeastConfigs::RefreshConfig(const FString& Alias)
{
	FConfigState* State = Configs.Find(Alias);
	if (State == nullptr || !Context.IsValid() || !Context->Api.IsValid())
	{
		return;
	}
	if (State->bInFlight)
	{
		State->bRefreshQueued = true;
		return;
	}
	State->bInFlight = true;
	State->bRefreshQueued = false;

	const int32 Generation = Context->IdentityGeneration;
	const TWeakPtr<FGamebeastContext> WeakContext = Context;
	TWeakObjectPtr<UGamebeastConfigs> WeakThis(this);
	Context->Api->EvaluateConfiguration(
		Alias, State->Hash, Context->DistinctId, Context->BuildEvaluationProperties(),
		[WeakThis, WeakContext, Alias, Generation](const FGamebeastFetchResult& Result)
		{
			UGamebeastConfigs* This = WeakThis.Get();
			// Gone, or shut down (and maybe set up again) since this request started.
			if (This == nullptr || !This->Context.IsValid() || This->Context != WeakContext.Pin())
			{
				return;
			}
			FConfigState* State = This->Configs.Find(Alias);
			if (State == nullptr)
			{
				return;
			}
			State->bInFlight = false;

			if (Generation != This->Context->IdentityGeneration)
			{
				// Evaluated for the previous user: drop it and fetch for the current one.
				This->RefreshConfig(Alias);
				return;
			}

			switch (Result.Status)
			{
			case EGamebeastFetchStatus::Updated:
				This->ApplyResponse(*State, Result.Body, /*bPersist*/ true);
				GB_INFO(TEXT("Configuration '%s' updated."), *Alias);
				break;
			case EGamebeastFetchStatus::NotModified:
				GB_INFO(TEXT("Configuration '%s' unchanged."), *Alias);
				break;
			case EGamebeastFetchStatus::NotFound:
				GB_ERROR(TEXT("Configuration '%s' not found (%s). Check the alias against your dashboard."),
					*Alias, *Result.Error);
				break;
			case EGamebeastFetchStatus::Failed:
				GB_WARN(TEXT("Refreshing configuration '%s' failed (%s); will retry."), *Alias, *Result.Error);
				break;
			}

			State = This->Configs.Find(Alias);
			if (State != nullptr && State->bRefreshQueued)
			{
				This->RefreshConfig(Alias);
			}
		});
}

void UGamebeastConfigs::ApplyResponse(FConfigState& State, const TSharedPtr<FJsonObject>& Response, bool bPersist)
{
	if (!Response.IsValid())
	{
		return;
	}

	TSharedPtr<FJsonValue> NewValues = Response->TryGetField(TEXT("configuration"));
	if (!NewValues.IsValid() || NewValues->Type != EJson::Object)
	{
		GB_WARN(TEXT("Configuration '%s' is not a JSON object; no values applied."), *State.Alias);
		NewValues = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
	}

	const TSharedPtr<FJsonValue> OldValues = State.Values;
	const bool bWasLoaded = State.bLoaded;
	const FString Alias = State.Alias;

	State.Values = NewValues;
	Response->TryGetStringField(TEXT("hash"), State.Hash);
	State.bLoaded = true;

	if (bPersist)
	{
		SaveCache(Alias, Response);
	}

	if (UGamebeastExperiments* ExperimentsService = Experiments.Get())
	{
		const TArray<TSharedPtr<FJsonValue>>* Assignments = nullptr;
		static const TArray<TSharedPtr<FJsonValue>> NoAssignments;
		ExperimentsService->ReportAssignments(
			Alias, Response->TryGetArrayField(TEXT("experiments"), Assignments) ? *Assignments : NoAssignments);
	}

	// Copy: callbacks may subscribe or unsubscribe while we iterate.
	const TArray<FListener> Snapshot = Listeners;
	for (const FListener& Listener : Snapshot)
	{
		if (!Listener.Alias.Equals(Alias, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const TSharedPtr<FJsonValue> NewValue = Gamebeast::Json::Resolve(NewValues, Listener.Segments);
		const bool bFire = bWasLoaded
			? !Gamebeast::Json::DeepEquals(Gamebeast::Json::Resolve(OldValues, Listener.Segments), NewValue)
			: Listener.bIncludeInitial;
		if (bFire && Listeners.ContainsByPredicate([&](const FListener& Live) { return Live.Id == Listener.Id; }))
		{
			Listener.Callback.ExecuteIfBound(NewValue);
		}
	}

	OnConfigurationUpdated.Broadcast(Alias);
	CheckReady();
}

void UGamebeastConfigs::CheckReady()
{
	if (bReady)
	{
		return;
	}
	for (const TPair<FString, FConfigState>& Pair : Configs)
	{
		if (Pair.Value.bRequiredForReady && !Pair.Value.bLoaded)
		{
			return;
		}
	}
	// Nothing to wait for until Setup has run.
	if (!Context.IsValid())
	{
		return;
	}

	bReady = true;
	TArray<TFunction<void()>> Callbacks = MoveTemp(ReadyCallbacks);
	ReadyCallbacks.Reset();
	for (TFunction<void()>& Callback : Callbacks)
	{
		Callback();
	}
	OnReadyEvent.Broadcast();
}

// --- Reading -----------------------------------------------------------------------------------

TSharedPtr<FJsonValue> UGamebeastConfigs::Lookup(const FString& Path, bool bLogMissing)
{
	FString Alias;
	TArray<FString> Segments;
	if (!Gamebeast::Json::SplitConfigPath(Path, Alias, Segments))
	{
		GB_ERROR(TEXT("'%s' is not a configuration path (expected \"ConfigName.SomeKey\")."), *Path);
		return nullptr;
	}

	const FConfigState* State = EnsureConfig(Alias, /*bRequiredForReady*/ false);
	if (State == nullptr || !State->bLoaded)
	{
		if (bLogMissing)
		{
			GB_WARN(TEXT("Configuration '%s' has not loaded yet; returning the default. Use Observe or OnReady to wait for it."),
				*Alias);
		}
		return nullptr;
	}
	const TSharedPtr<FJsonValue> Value = Gamebeast::Json::Resolve(State->Values, Segments);
	return Value.IsValid() && Value->Type != EJson::Null ? Value : nullptr;
}

bool UGamebeastConfigs::HasValue(const FString& Path)
{
	return Lookup(Path, /*bLogMissing*/ false).IsValid();
}

TSharedPtr<FJsonValue> UGamebeastConfigs::GetJson(const FString& Path)
{
	return Lookup(Path, /*bLogMissing*/ false);
}

FString UGamebeastConfigs::GetString(const FString& Path, const FString& DefaultValue)
{
	const TSharedPtr<FJsonValue> Value = Lookup(Path, true);
	FString Result;
	return Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(Result) ? Result : DefaultValue;
}

int32 UGamebeastConfigs::GetInt(const FString& Path, int32 DefaultValue)
{
	const TSharedPtr<FJsonValue> Value = Lookup(Path, true);
	double Number = 0.0;
	if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number))
	{
		return DefaultValue;
	}
	return static_cast<int32>(FMath::RoundToInt(Number));
}

float UGamebeastConfigs::GetFloat(const FString& Path, float DefaultValue)
{
	const TSharedPtr<FJsonValue> Value = Lookup(Path, true);
	double Number = 0.0;
	return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Number)
		? static_cast<float>(Number)
		: DefaultValue;
}

bool UGamebeastConfigs::GetBool(const FString& Path, bool DefaultValue)
{
	const TSharedPtr<FJsonValue> Value = Lookup(Path, true);
	bool bResult = false;
	return Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(bResult) ? bResult : DefaultValue;
}

FString UGamebeastConfigs::GetJsonString(const FString& Path, const FString& DefaultValue)
{
	const TSharedPtr<FJsonValue> Value = Lookup(Path, true);
	return Value.IsValid() ? Gamebeast::Json::SerializeValue(Value) : DefaultValue;
}

// --- Subscriptions -----------------------------------------------------------------------------

void UGamebeastConfigs::OnReady(TFunction<void()> Callback)
{
	if (!Callback)
	{
		return;
	}
	if (bReady)
	{
		Callback();
		return;
	}
	ReadyCallbacks.Add(MoveTemp(Callback));
}

int64 UGamebeastConfigs::AddListener(const FString& Path, FGamebeastConfigValueCallback Callback, bool bIncludeInitial)
{
	FListener Listener;
	if (!Callback.IsBound() || !Gamebeast::Json::SplitConfigPath(Path, Listener.Alias, Listener.Segments))
	{
		GB_ERROR(TEXT("Subscribing needs a configuration path (\"ConfigName.SomeKey\") and a bound callback."));
		return 0;
	}
	Listener.Id = NextListenerId++;
	Listener.Callback = MoveTemp(Callback);
	Listener.bIncludeInitial = bIncludeInitial;

	// Register (and maybe load from cache) before adding the listener, so a cache hit doesn't
	// call an Observe callback twice.
	const FConfigState* State = EnsureConfig(Listener.Alias, /*bRequiredForReady*/ false);
	Listeners.Add(Listener);
	State = Configs.Find(Listener.Alias);
	if (bIncludeInitial && State != nullptr && State->bLoaded)
	{
		Listener.Callback.ExecuteIfBound(Gamebeast::Json::Resolve(State->Values, Listener.Segments));
	}
	return Listener.Id;
}

int64 UGamebeastConfigs::OnChanged(const FString& Path, FGamebeastConfigValueCallback Callback)
{
	return AddListener(Path, MoveTemp(Callback), /*bIncludeInitial*/ false);
}

int64 UGamebeastConfigs::Observe(const FString& Path, FGamebeastConfigValueCallback Callback)
{
	return AddListener(Path, MoveTemp(Callback), /*bIncludeInitial*/ true);
}

void UGamebeastConfigs::Unsubscribe(int64 Handle)
{
	Listeners.RemoveAll([Handle](const FListener& Listener) { return Listener.Id == Handle; });
}

// --- Disk cache --------------------------------------------------------------------------------

FString UGamebeastConfigs::CacheFileName(const FString& Alias) const
{
	return FString::Printf(TEXT("configuration_%s_%s.json"),
		*Gamebeast::Storage::SafeFileComponent(Context.IsValid() ? Context->EnvironmentAlias : FString()),
		*Gamebeast::Storage::SafeFileComponent(Alias.ToLower()));
}

TSharedPtr<FJsonObject> UGamebeastConfigs::LoadCache(const FString& Alias) const
{
	if (!Context.IsValid())
	{
		return nullptr;
	}
	FString Text;
	if (!Gamebeast::Storage::Load(CacheFileName(Alias), Text))
	{
		return nullptr;
	}
	const TSharedPtr<FJsonObject> Entry = Gamebeast::Json::ParseObject(Text);
	const TSharedPtr<FJsonObject>* Response = nullptr;
	FString AppVersion;
	FString SdkVersion;
	FString DistinctId;
	int32 Format = 0;
	if (!Entry.IsValid() || !Entry->TryGetNumberField(TEXT("format"), Format) || Format != CacheFormatVersion
		|| !Entry->TryGetStringField(TEXT("appVersion"), AppVersion)
		|| !Entry->TryGetStringField(TEXT("sdkVersion"), SdkVersion)
		|| !Entry->TryGetStringField(TEXT("distinctId"), DistinctId)
		|| !Entry->TryGetObjectField(TEXT("response"), Response))
	{
		return nullptr;
	}
	// A different build may read different keys; a different user gets different experiment values.
	if (AppVersion != FGamebeastContext::AppVersion() || SdkVersion != GAMEBEAST_SDK_VERSION
		|| DistinctId != Context->DistinctId)
	{
		return nullptr;
	}
	return *Response;
}

void UGamebeastConfigs::SaveCache(const FString& Alias, const TSharedPtr<FJsonObject>& Response) const
{
	if (!Context.IsValid() || !Response.IsValid())
	{
		return;
	}
	const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetNumberField(TEXT("format"), CacheFormatVersion);
	Entry->SetStringField(TEXT("appVersion"), FGamebeastContext::AppVersion());
	Entry->SetStringField(TEXT("sdkVersion"), GAMEBEAST_SDK_VERSION);
	Entry->SetStringField(TEXT("distinctId"), Context->DistinctId);
	Entry->SetObjectField(TEXT("response"), Response);
	Gamebeast::Storage::Save(CacheFileName(Alias), Gamebeast::Json::Serialize(Entry));
}
