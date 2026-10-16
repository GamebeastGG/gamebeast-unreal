// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastContext.h"

#include "GeneralProjectSettings.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastLog.h"
#include "Internal/GamebeastStorage.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Internationalization.h"

namespace
{
	const TCHAR* IdentityFile = TEXT("identity.json");

	/** `CONTEXT_MAX_PROPERTIES` / `CUSTOM_PROPERTY_NAME_MAX` in @gamebeast/sdk-contract. */
	constexpr int32 MaxContextProperties = 100;
	constexpr int32 MaxPropertyNameLength = 128;

	/** Targeting accepts scalars and homogeneous scalar arrays only. */
	bool IsValidContextValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return false;
		}
		switch (Value->Type)
		{
		case EJson::String:
		case EJson::Number:
		case EJson::Boolean:
		case EJson::Null:
			return true;
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
			if (Items.Num() > 100)
			{
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Item : Items)
			{
				if (!Item.IsValid() || Item->Type != Items[0]->Type
					|| (Item->Type != EJson::String && Item->Type != EJson::Number && Item->Type != EJson::Boolean))
				{
					return false;
				}
			}
			return true;
		}
		default:
			return false;
		}
	}
}

TSharedRef<FJsonObject> FGamebeastContext::BuildEvaluationProperties() const
{
	const TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
	Properties->SetStringField(TEXT("platform"), UGameplayStatics::GetPlatformName());
	Properties->SetStringField(TEXT("operatingSystem"), FPlatformMisc::GetOSVersion());
	Properties->SetStringField(TEXT("deviceModel"), FPlatformMisc::GetDeviceMakeAndModel());
	Properties->SetStringField(TEXT("systemLanguage"), FInternationalization::Get().GetCurrentCulture()->GetName());
	Properties->SetStringField(TEXT("appVersion"), AppVersion());
	Properties->SetStringField(TEXT("engineVersion"), FApp::GetBuildVersion());

	const TSharedRef<FJsonObject> Custom = Options.UserProperties.ToJson();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Custom->Values)
	{
		if (Pair.Key.IsEmpty() || Pair.Key.Len() > MaxPropertyNameLength)
		{
			GB_WARN(TEXT("User property '%s' skipped: names must be 1-%d characters."), *Pair.Key, MaxPropertyNameLength);
			continue;
		}
		if (!IsValidContextValue(Pair.Value))
		{
			GB_WARN(TEXT("User property '%s' skipped: targeting values must be strings, numbers, booleans, or arrays of one of those."),
				*Pair.Key);
			continue;
		}
		if (!Properties->HasField(Pair.Key) && Properties->Values.Num() >= MaxContextProperties)
		{
			GB_WARN(TEXT("User property '%s' skipped: at most %d properties are sent."), *Pair.Key, MaxContextProperties);
			continue;
		}
		Properties->SetField(Pair.Key, Pair.Value);
	}
	return Properties;
}

FString FGamebeastContext::AppVersion()
{
	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	return ProjectSettings != nullptr ? ProjectSettings->ProjectVersion : FString();
}

FString FGamebeastContext::LoadOrCreateAnonymousId()
{
	FString Text;
	if (Gamebeast::Storage::Load(IdentityFile, Text))
	{
		const TSharedPtr<FJsonObject> Json = Gamebeast::Json::ParseObject(Text);
		FString Id;
		if (Json.IsValid() && Json->TryGetStringField(TEXT("anonymousId"), Id) && !Id.IsEmpty())
		{
			return Id;
		}
	}

	const FString Id = Gamebeast::Json::NewUuid();
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("anonymousId"), Id);
	Gamebeast::Storage::Save(IdentityFile, Gamebeast::Json::Serialize(Json));
	return Id;
}

FString FGamebeastContext::NewSessionId()
{
	return TEXT("unreal-") + Gamebeast::Json::NewUuid();
}
