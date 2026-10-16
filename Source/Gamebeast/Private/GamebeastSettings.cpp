// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

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
	Options.CustomEnvironment = CustomEnvironment;
	Options.Configurations = Configurations;
	Options.Rates = Rates;
	Options.bFastRatesInEditor = bFastRatesInEditor;
	Options.SessionTimeoutSeconds = SessionTimeoutSeconds;
	Options.bDebugLogging = bDebugLogging;
	Options.ApiUrl = ApiUrl;
	return Options;
}
