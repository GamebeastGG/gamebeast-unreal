// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamebeastTypes.h"

#include "GamebeastSettings.generated.h"

/**
 * Project Settings > Plugins > Gamebeast. Saved to Config/DefaultGame.ini.
 *
 * With bAutoInitialize on and an API key set, the SDK starts itself when the game instance
 * starts; no code needed. Calling UGamebeastSubsystem::Setup yourself takes precedence.
 *
 * The SDK Key is a public key (it ships inside the game), but if your repository is public,
 * keep it out of DefaultGame.ini: put the [/Script/Gamebeast.GamebeastSettings] section in an
 * untracked Config/UserGame.ini instead.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Gamebeast"))
class GAMEBEAST_API UGamebeastSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGamebeastSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Start the SDK automatically when the game instance starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast")
	bool bAutoInitialize = true;

	/**
	 * Also start automatically on dedicated servers. Off by default: the SDK identifies the
	 * local player, which a dedicated server doesn't have.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast", meta = (EditCondition = "bAutoInitialize"))
	bool bAutoInitializeOnDedicatedServer = false;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast")
	FString ApiKey;

	/**
	 * The environment's dashboard alias ("production", "development", "staging", ...). Empty
	 * chooses automatically: development in the editor (including Play In Editor), production in
	 * packaged builds.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast")
	FString Environment;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast")
	TArray<FString> Configurations;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast|Rates", meta = (ShowOnlyInnerProperties))
	FGamebeastRates Rates;

	/** Use each rate's minimum in the editor so changes show up quickly. Packaged builds use Rates. */
	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast|Rates")
	bool bFastRatesInEditor = true;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast", meta = (ClampMin = "0"))
	float SessionTimeoutSeconds = 1800.f;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast")
	bool bDebugLogging = false;

	UPROPERTY(Config, EditAnywhere, Category = "Gamebeast", AdvancedDisplay)
	FString ApiUrl = TEXT("https://api.gamebeast.gg");

	/** These settings as setup options (no DistinctId or UserProperties: pass those in code). */
	FGamebeastSetupOptions ToSetupOptions() const;
};
