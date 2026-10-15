// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "Internal/GamebeastMarkerQueue.h"

#include "Internal/GamebeastLog.h"

FGamebeastMarkerQueue::FGamebeastMarkerQueue(FSendFn InSend, FOptions InOptions)
	: Send(MoveTemp(InSend))
	, Options(InOptions)
{
	Options.MaxBatchSize = FMath::Max(1, Options.MaxBatchSize);
	Options.MaxSendAttempts = FMath::Max(1, Options.MaxSendAttempts);
}

FGamebeastMarkerQueue::~FGamebeastMarkerQueue()
{
	*Alive = false;
}

int32 FGamebeastMarkerQueue::BufferedCount() const
{
	int32 Count = Pending.Num();
	for (const FRetryBatch& Batch : Retries)
	{
		Count += Batch.Markers.Num();
	}
	return Count;
}

void FGamebeastMarkerQueue::Enqueue(const TSharedPtr<FJsonValue>& Marker)
{
	if (bStopped)
	{
		GB_WARN(TEXT("Marker dropped: the SDK has been shut down."));
		return;
	}
	if (Pending.Num() == 0)
	{
		PendingAgeSeconds = 0.f;
	}
	Pending.Add(Marker);
	EnforceBufferCap();

	if (Pending.Num() >= Options.MaxBatchSize)
	{
		FlushPending();
	}
}

void FGamebeastMarkerQueue::Restore(const TArray<TSharedPtr<FJsonValue>>& Markers)
{
	if (bStopped || Markers.Num() == 0)
	{
		return;
	}
	const int32 Start = FMath::Max(0, Markers.Num() - Options.MaxBufferedMarkers);
	if (Start > 0)
	{
		GB_WARN(TEXT("Dropping %d persisted marker(s) over the buffer cap."), Start);
	}
	for (int32 Index = Start; Index < Markers.Num(); Index += Options.MaxBatchSize)
	{
		const int32 Count = FMath::Min(Options.MaxBatchSize, Markers.Num() - Index);
		TArray<TSharedPtr<FJsonValue>> Batch(Markers.GetData() + Index, Count);
		Dispatch(MoveTemp(Batch), 1);
	}
}

void FGamebeastMarkerQueue::Flush()
{
	FlushPending();

	TArray<FRetryBatch> Due = MoveTemp(Retries);
	Retries.Reset();
	for (FRetryBatch& Batch : Due)
	{
		Dispatch(MoveTemp(Batch.Markers), Batch.NextAttempt);
	}
}

void FGamebeastMarkerQueue::Tick(float DeltaSeconds)
{
	if (bStopped)
	{
		return;
	}

	if (Pending.Num() > 0)
	{
		PendingAgeSeconds += DeltaSeconds;
		if (PendingAgeSeconds >= Options.FlushIntervalSeconds)
		{
			FlushPending();
		}
	}

	if (Retries.Num() == 0)
	{
		return;
	}
	TArray<FRetryBatch> Due;
	for (int32 Index = Retries.Num() - 1; Index >= 0; --Index)
	{
		Retries[Index].SecondsUntilSend -= DeltaSeconds;
		if (Retries[Index].SecondsUntilSend <= 0.f)
		{
			Due.Insert(MoveTemp(Retries[Index]), 0);
			Retries.RemoveAt(Index);
		}
	}
	for (FRetryBatch& Batch : Due)
	{
		Dispatch(MoveTemp(Batch.Markers), Batch.NextAttempt);
	}
}

TArray<TSharedPtr<FJsonValue>> FGamebeastMarkerQueue::TakeUnsent()
{
	TArray<TSharedPtr<FJsonValue>> Unsent;
	for (FRetryBatch& Batch : Retries)
	{
		Unsent.Append(MoveTemp(Batch.Markers));
	}
	Unsent.Append(MoveTemp(Pending));
	Retries.Reset();
	Pending.Reset();
	PendingAgeSeconds = 0.f;
	return Unsent;
}

void FGamebeastMarkerQueue::FlushPending()
{
	PendingAgeSeconds = 0.f;
	while (Pending.Num() > 0)
	{
		const int32 Count = FMath::Min(Options.MaxBatchSize, Pending.Num());
		TArray<TSharedPtr<FJsonValue>> Batch(Pending.GetData(), Count);
		Pending.RemoveAt(0, Count);
		Dispatch(MoveTemp(Batch), 1);
	}
}

void FGamebeastMarkerQueue::Dispatch(TArray<TSharedPtr<FJsonValue>> Markers, int32 Attempt)
{
	if (Markers.Num() == 0)
	{
		return;
	}
	++InFlight;
	TWeakPtr<bool> WeakAlive = Alive;
	// Copy for the request; the callback keeps its own copy in case the batch must be retried.
	const TArray<TSharedPtr<FJsonValue>> RequestMarkers = Markers;
	Send(RequestMarkers,
		[this, WeakAlive, Markers = MoveTemp(Markers), Attempt](const FGamebeastHttpResponse& Response) mutable
		{
			const TSharedPtr<bool> StillAlive = WeakAlive.Pin();
			if (!StillAlive.IsValid() || !*StillAlive)
			{
				return;
			}
			HandleResponse(MoveTemp(Markers), Attempt, Response);
		});
}

void FGamebeastMarkerQueue::HandleResponse(
	TArray<TSharedPtr<FJsonValue>> Markers, int32 Attempt, const FGamebeastHttpResponse& Response)
{
	--InFlight;

	if (Response.IsSuccess())
	{
		GB_INFO(TEXT("Sent %d marker(s)."), Markers.Num());
		return;
	}

	if (!Response.IsRetryable())
	{
		GB_ERROR(TEXT("Dropped %d marker(s): %s"), Markers.Num(), *Response.Describe());
		return;
	}

	if (Attempt >= Options.MaxSendAttempts)
	{
		GB_ERROR(TEXT("Dropped %d marker(s) after %d attempts: %s"), Markers.Num(), Attempt, *Response.Describe());
		return;
	}

	const float Backoff = Options.RetryBaseDelaySeconds * static_cast<float>(1 << (Attempt - 1));
	const float Delay = FMath::Max(Backoff, Response.RetryAfterSeconds);
	GB_WARN(TEXT("Failed to send %d marker(s) (%s); retrying in %.0fs (attempt %d of %d)."),
		Markers.Num(), *Response.Describe(), Delay, Attempt, Options.MaxSendAttempts);

	FRetryBatch Retry;
	Retry.Markers = MoveTemp(Markers);
	Retry.NextAttempt = Attempt + 1;
	Retry.SecondsUntilSend = Delay;
	Retries.Add(MoveTemp(Retry));
	EnforceBufferCap();
}

void FGamebeastMarkerQueue::EnforceBufferCap()
{
	int32 Excess = BufferedCount() - Options.MaxBufferedMarkers;
	if (Excess <= 0)
	{
		return;
	}
	int32 Dropped = 0;
	// Oldest first: retry batches predate anything still waiting.
	while (Excess > 0 && Retries.Num() > 0)
	{
		const int32 Count = Retries[0].Markers.Num();
		Dropped += Count;
		Excess -= Count;
		Retries.RemoveAt(0);
	}
	if (Excess > 0)
	{
		const int32 Count = FMath::Min(Excess, Pending.Num());
		Pending.RemoveAt(0, Count);
		Dropped += Count;
	}
	GB_ERROR(TEXT("Marker buffer full; dropped %d oldest marker(s)."), Dropped);
}
