// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

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
