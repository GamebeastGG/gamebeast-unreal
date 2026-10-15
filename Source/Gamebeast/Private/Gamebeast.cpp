// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "Gamebeast.h"

#include "Internal/GamebeastLog.h"

DEFINE_LOG_CATEGORY(LogGamebeast);

namespace Gamebeast::Private
{
	bool GDebugLogging = false;
}

IMPLEMENT_MODULE(FGamebeastModule, Gamebeast)
