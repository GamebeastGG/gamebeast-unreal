// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "Modules/ModuleManager.h"

/** SDK version, sent as `sdkversion: gamebeast-unreal/<version>`. Keep in step with Gamebeast.uplugin. */
#define GAMEBEAST_SDK_VERSION TEXT("0.1.0")
#define GAMEBEAST_SDK_NAME TEXT("gamebeast-unreal")

class FGamebeastModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};
