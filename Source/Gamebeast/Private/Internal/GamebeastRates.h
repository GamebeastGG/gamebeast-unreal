// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"

/** Rate validation and the `/status` poll schedule. Pure functions, so they're unit tested. */
namespace Gamebeast::Rates
{
	constexpr float MinMarkerFlushRate = 5.f;
	constexpr float MinStatusPollRate = 5.f;
	constexpr float MinAssignmentRefreshRate = 10.f;

	/** Jitter when the backend sends none, so clients that start together don't poll together. */
	constexpr float DefaultJitterRatio = 0.2f;
	/** Failed polls back off exponentially from the poll rate, up to this. */
	constexpr float MaxBackoffSeconds = 300.f;

	/**
	 * Rates with every value raised to its minimum. Each value below its minimum (or not a finite
	 * number) is logged as a warning naming the setting.
	 */
	FGamebeastRates Validate(const FGamebeastRates& Requested);

	/** The rates Setup runs with: the minimums in the editor when bFastRatesInEditor, else Validate(Rates). */
	FGamebeastRates Resolve(const FGamebeastSetupOptions& Options, bool bIsEditor);

	/** `polling` from a `/status` response: present only while the backend is shedding load. */
	struct FPollingOverride
	{
		bool bPresent = false;
		float IntervalSeconds = 0.f;
		float JitterRatio = DefaultJitterRatio;
	};

	/**
	 * Seconds until the next `/status` poll.
	 *
	 * - Base: max(PollRate, Override.IntervalSeconds) while an override is present (it's a floor).
	 * - After failures: Base * 2^(failures - 1), capped at MaxBackoffSeconds.
	 * - Spread by ±JitterRatio (the override's, else DefaultJitterRatio). RandomUnit is in [0, 1].
	 * - Never sooner than RetryAfterSeconds (from a 429/503 `retry-after`; negative when absent).
	 */
	float NextPollDelay(
		float PollRate, const FPollingOverride& Override, int32 ConsecutiveFailures, float RetryAfterSeconds,
		float RandomUnit);
}
