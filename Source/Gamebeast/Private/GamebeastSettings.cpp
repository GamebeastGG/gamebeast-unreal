// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastSettings.h"

UGamebeastSettings::UGamebeastSettings()
{
	SectionName = TEXT("Gamebeast");
}

FGamebeastSetupOptions UGamebeastSettings::ToSetupOptions() const
{
	FGamebeastSetupOptions Options;
	Options.ApiKey = ApiKey;
	Options.Environment = Environment;
	Options.Configurations = Configurations;
	Options.Rates = Rates;
	Options.bFastRatesInEditor = bFastRatesInEditor;
	Options.SessionTimeoutSeconds = SessionTimeoutSeconds;
	Options.bDebugLogging = bDebugLogging;
	Options.ApiUrl = ApiUrl;
	return Options;
}
