// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"
#include "UObject/Object.h"

#include "GamebeastMarkers.generated.h"

class FGamebeastMarkerQueue;
struct FGamebeastContext;

/**
 * Analytics markers (events), attributed to the local user.
 *
 * Markers are batched and sent automatically: when 10 are waiting, every 10 seconds, and on
 * Flush. Failed sends retry with backoff. Markers still buffered when the game goes to the
 * background or shuts down are saved on the device and sent on the next launch.
 */
UCLASS(BlueprintType)
class GAMEBEAST_API UGamebeastMarkers : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Send a marker. Leave Properties unconnected for a marker without properties.
	 * C++: SendMarker(TEXT("LevelCompleted"), FGamebeastProperties().Set(TEXT("Level"), 3));
	 */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Markers", meta = (AutoCreateRefTerm = "Properties"))
	void SendMarker(const FString& MarkerType, const FGamebeastProperties& Properties);

	/** Send a marker with a raw JSON object as its properties. */
	void SendMarkerJson(const FString& MarkerType, const TSharedPtr<FJsonObject>& Properties);

	/** Send everything buffered now, instead of waiting for the next batch. */
	UFUNCTION(BlueprintCallable, Category = "Gamebeast|Markers")
	void Flush();

	/** Markers waiting to be sent or awaiting a retry. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Markers")
	int32 GetBufferedCount() const;

private:
	friend class UGamebeastSubsystem;

	void Start(const TSharedRef<FGamebeastContext>& InContext);
	void Tick(float DeltaSeconds);
	/** Save what's buffered to disk. Nothing is sent twice: saved markers leave the queue. */
	void PersistUnsent();
	/** Send markers saved by PersistUnsent (on resume, or by a previous run). */
	void RestorePersisted();
	void Stop();

	FString StoreFileName() const;

	TSharedPtr<FGamebeastContext> Context;
	TSharedPtr<FGamebeastMarkerQueue> Queue;
};
