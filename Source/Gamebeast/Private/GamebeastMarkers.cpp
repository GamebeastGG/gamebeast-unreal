// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "GamebeastMarkers.h"

#include "GamebeastSubsystem.h"
#include "Internal/GamebeastContext.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastLog.h"
#include "Internal/GamebeastMarkerQueue.h"
#include "Internal/GamebeastStorage.h"

void UGamebeastMarkers::SendMarker(const FString& MarkerType, const FGamebeastProperties& Properties)
{
	SendMarkerJson(MarkerType, Properties.ToJson());
}

void UGamebeastMarkers::SendMarkerJson(const FString& MarkerType, const TSharedPtr<FJsonObject>& Properties)
{
	if (!Queue.IsValid() || !Context.IsValid())
	{
		GB_WARN(TEXT("Marker '%s' dropped: call Setup (or set an API key in Project Settings) first."), *MarkerType);
		return;
	}
	if (MarkerType.TrimStartAndEnd().IsEmpty())
	{
		GB_ERROR(TEXT("SendMarker called without a marker type; marker dropped."));
		return;
	}

	const TSharedRef<FJsonObject> Marker = MakeShared<FJsonObject>();
	Marker->SetStringField(TEXT("markerId"), Gamebeast::Json::NewUuid());
	Marker->SetNumberField(TEXT("timestamp"), static_cast<double>(Gamebeast::Json::NowUnixMilliseconds()));
	Marker->SetStringField(TEXT("eventName"), MarkerType);
	Marker->SetStringField(TEXT("sessionId"), Context->SessionId);
	Marker->SetStringField(TEXT("distinctId"), Context->DistinctId);
	Marker->SetObjectField(TEXT("properties"), Properties.IsValid() ? Properties : MakeShared<FJsonObject>());

	GB_INFO(TEXT("Queued marker '%s'."), *MarkerType);
	Queue->Enqueue(MakeShared<FJsonValueObject>(Marker));
}

void UGamebeastMarkers::Flush()
{
	if (Queue.IsValid())
	{
		Queue->Flush();
	}
}

int32 UGamebeastMarkers::GetBufferedCount() const
{
	return Queue.IsValid() ? Queue->BufferedCount() : 0;
}

FString UGamebeastMarkers::StoreFileName() const
{
	return FString::Printf(TEXT("markers_%s.json"), *Gamebeast::Storage::SafeFileComponent(Context->EnvironmentAlias));
}

void UGamebeastMarkers::Start(const TSharedRef<FGamebeastContext>& InContext)
{
	Context = InContext;

	FGamebeastMarkerQueue::FOptions QueueOptions;
	QueueOptions.FlushIntervalSeconds = InContext->Rates.MarkerFlushRate;

	const TWeakPtr<FGamebeastApiClient> WeakApi = InContext->Api;
	Queue = MakeShared<FGamebeastMarkerQueue>(
		[WeakApi](const TArray<TSharedPtr<FJsonValue>>& Markers, TFunction<void(const FGamebeastHttpResponse&)> OnComplete)
		{
			const TSharedPtr<FGamebeastApiClient> Api = WeakApi.Pin();
			if (!Api.IsValid())
			{
				FGamebeastHttpResponse Response;
				Response.bTransportError = true;
				OnComplete(Response);
				return;
			}
			Api->PostMarkers(Markers, MoveTemp(OnComplete));
		},
		QueueOptions);

	RestorePersisted();
}

void UGamebeastMarkers::OnRatesChanged()
{
	if (Queue.IsValid() && Context.IsValid())
	{
		Queue->SetFlushInterval(Context->Rates.MarkerFlushRate);
	}
}

void UGamebeastMarkers::Tick(float DeltaSeconds)
{
	if (Queue.IsValid())
	{
		Queue->Tick(DeltaSeconds);
	}
}

void UGamebeastMarkers::PersistUnsent()
{
	if (!Queue.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>> Unsent = Queue->TakeUnsent();
	if (Unsent.Num() == 0)
	{
		return;
	}

	// Merge with anything already on disk (e.g. a second background before a resume).
	TArray<TSharedPtr<FJsonValue>> All;
	FString Existing;
	if (Gamebeast::Storage::Load(StoreFileName(), Existing))
	{
		const TSharedPtr<FJsonValue> Parsed = Gamebeast::Json::ParseValue(Existing);
		if (Parsed.IsValid() && Parsed->Type == EJson::Array)
		{
			All = Parsed->AsArray();
		}
	}
	All.Append(Unsent);

	const FString Text = Gamebeast::Json::SerializeValue(MakeShared<FJsonValueArray>(All));
	if (Gamebeast::Storage::Save(StoreFileName(), Text))
	{
		GB_INFO(TEXT("Saved %d unsent marker(s) to send on the next launch."), Unsent.Num());
	}
	else
	{
		GB_ERROR(TEXT("Could not save %d unsent marker(s); they are lost."), Unsent.Num());
	}
}

void UGamebeastMarkers::RestorePersisted()
{
	if (!Queue.IsValid())
	{
		return;
	}
	FString Text;
	if (!Gamebeast::Storage::Load(StoreFileName(), Text))
	{
		return;
	}
	// Delete before sending: from here the queue owns them, so they are never replayed twice.
	Gamebeast::Storage::Delete(StoreFileName());

	const TSharedPtr<FJsonValue> Parsed = Gamebeast::Json::ParseValue(Text);
	if (!Parsed.IsValid() || Parsed->Type != EJson::Array)
	{
		GB_WARN(TEXT("Discarded an unreadable saved-marker file."));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>& Markers = Parsed->AsArray();
	GB_INFO(TEXT("Sending %d marker(s) saved by a previous session."), Markers.Num());
	Queue->Restore(Markers);
}

void UGamebeastMarkers::Stop()
{
	PersistUnsent();
	if (Queue.IsValid())
	{
		Queue->Stop();
	}
	Queue.Reset();
	Context.Reset();
}
