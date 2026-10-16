// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastRates.h"

#include "Internal/GamebeastLog.h"

namespace
{
	float AtLeast(const TCHAR* Name, float Value, float Minimum)
	{
		if (!FMath::IsFinite(Value))
		{
			GB_WARN(TEXT("%s must be a number of seconds; using %.0f."), Name, Minimum);
			return Minimum;
		}
		if (Value < Minimum)
		{
			GB_WARN(TEXT("%s must be at least %.0f seconds, got %g; using %.0f."), Name, Minimum, Value, Minimum);
			return Minimum;
		}
		return Value;
	}
}

namespace Gamebeast::Rates
{
	FGamebeastRates Validate(const FGamebeastRates& Requested)
	{
		FGamebeastRates Rates;
		Rates.MarkerFlushRate = AtLeast(TEXT("MarkerFlushRate"), Requested.MarkerFlushRate, MinMarkerFlushRate);
		Rates.StatusPollRate = AtLeast(TEXT("StatusPollRate"), Requested.StatusPollRate, MinStatusPollRate);
		Rates.AssignmentRefreshRate =
			AtLeast(TEXT("AssignmentRefreshRate"), Requested.AssignmentRefreshRate, MinAssignmentRefreshRate);
		return Rates;
	}

	FGamebeastRates Resolve(const FGamebeastSetupOptions& Options, bool bIsEditor)
	{
		if (bIsEditor && Options.bFastRatesInEditor)
		{
			FGamebeastRates Fast;
			Fast.MarkerFlushRate = MinMarkerFlushRate;
			Fast.StatusPollRate = MinStatusPollRate;
			Fast.AssignmentRefreshRate = MinAssignmentRefreshRate;
			return Fast;
		}
		return Validate(Options.Rates);
	}

	float NextPollDelay(
		float PollRate, const FPollingOverride& Override, int32 ConsecutiveFailures, float RetryAfterSeconds,
		float RandomUnit)
	{
		float Base = FMath::Max(PollRate, MinStatusPollRate);
		float Jitter = DefaultJitterRatio;
		if (Override.bPresent)
		{
			Base = FMath::Max(Base, Override.IntervalSeconds);
			Jitter = FMath::Clamp(Override.JitterRatio, 0.f, 1.f);
		}

		float Delay = Base;
		if (ConsecutiveFailures > 0)
		{
			const int32 Doublings = FMath::Min(ConsecutiveFailures - 1, 8);
			Delay = FMath::Min(MaxBackoffSeconds, Base * static_cast<float>(1 << Doublings));
		}

		const float Unit = FMath::Clamp(RandomUnit, 0.f, 1.f);
		Delay *= 1.f + Jitter * (2.f * Unit - 1.f);
		return FMath::Max(Delay, RetryAfterSeconds);
	}
}
