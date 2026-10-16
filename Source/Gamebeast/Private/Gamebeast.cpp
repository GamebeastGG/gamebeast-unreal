// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Gamebeast.h"

#include "Internal/GamebeastLog.h"

DEFINE_LOG_CATEGORY(LogGamebeast);

namespace Gamebeast::Private
{
	bool GDebugLogging = false;
}

IMPLEMENT_MODULE(FGamebeastModule, Gamebeast)
