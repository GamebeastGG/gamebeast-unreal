// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Internal/GamebeastApiClient.h"
#include "Templates/Function.h"

/**
 * Buffers markers and posts them in batches. Driven by Tick on the game thread; owns no timers.
 *
 * Delivery rules (the same as the Unity and JavaScript SDKs):
 * - A batch is sent when it reaches MaxBatchSize, when the oldest waiting marker has waited
 *   FlushIntervalSeconds, or on Flush().
 * - Transport errors, 408, 429 and 5xx retry with exponential backoff (2s, 4s, 8s, 16s, or the
 *   server's `retry-after` when longer), up to MaxSendAttempts in total.
 * - Any other 4xx (invalid markers, a blocked subscription, a bad key) is permanent: the batch
 *   is dropped and logged, since resending cannot succeed.
 * - Everything buffered (waiting and awaiting retry) is capped at MaxBufferedMarkers; the
 *   oldest markers are dropped first.
 *
 * The backend does not deduplicate markers, so the queue never sends a marker twice on purpose:
 * each marker is either buffered, in flight, or handed off with TakeUnsent().
 */
class FGamebeastMarkerQueue
{
public:
	using FSendFn = TFunction<void(
		const TArray<TSharedPtr<FJsonValue>>& /* Markers */,
		TFunction<void(const FGamebeastHttpResponse&)> /* OnComplete */)>;

	struct FOptions
	{
		int32 MaxBatchSize = 10;
		/** Gamebeast::Rates validates the configured value (default 30, at least 5). */
		float FlushIntervalSeconds = 30.f;
		int32 MaxBufferedMarkers = 500;
		int32 MaxSendAttempts = 5;
		float RetryBaseDelaySeconds = 2.f;
	};

	FGamebeastMarkerQueue(FSendFn InSend, FOptions InOptions);
	~FGamebeastMarkerQueue();

	void Enqueue(const TSharedPtr<FJsonValue>& Marker);

	/** Send markers recovered from disk right away (first attempt). */
	void Restore(const TArray<TSharedPtr<FJsonValue>>& Markers);

	/** Send everything waiting now, including batches in retry backoff. */
	void Flush();

	/** Advance flush and retry timers. */
	void Tick(float DeltaSeconds);

	/**
	 * Remove and return everything not yet handed to the network, so the caller can persist it.
	 * In-flight batches are not included: they either land, or come back as retries.
	 */
	TArray<TSharedPtr<FJsonValue>> TakeUnsent();

	/** Waiting plus awaiting retry (not in flight). */
	int32 BufferedCount() const;
	int32 InFlightCount() const { return InFlight; }

	/** Stop sending; later Enqueue calls are dropped with a warning. */
	void Stop() { bStopped = true; }

	/** Applies to markers already waiting too: they go out once they've waited the new interval. */
	void SetFlushInterval(float Seconds) { Options.FlushIntervalSeconds = Seconds; }

private:
	struct FRetryBatch
	{
		TArray<TSharedPtr<FJsonValue>> Markers;
		int32 NextAttempt = 2;
		float SecondsUntilSend = 0.f;
	};

	void FlushPending();
	void Dispatch(TArray<TSharedPtr<FJsonValue>> Markers, int32 Attempt);
	void HandleResponse(TArray<TSharedPtr<FJsonValue>> Markers, int32 Attempt, const FGamebeastHttpResponse& Response);
	void EnforceBufferCap();

	FSendFn Send;
	FOptions Options;

	TArray<TSharedPtr<FJsonValue>> Pending;
	float PendingAgeSeconds = 0.f;
	TArray<FRetryBatch> Retries;
	int32 InFlight = 0;
	bool bStopped = false;

	/** Lets in-flight callbacks notice the queue was destroyed. */
	TSharedRef<bool> Alive = MakeShared<bool>(true);
};
