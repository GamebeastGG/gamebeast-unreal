// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace Gamebeast::Json
{
	FString Serialize(const TSharedRef<FJsonObject>& Object);
	FString SerializeValue(const TSharedPtr<FJsonValue>& Value);

	/** Null when Text is not a JSON object. */
	TSharedPtr<FJsonObject> ParseObject(const FString& Text);
	/** Null when Text is not valid JSON. */
	TSharedPtr<FJsonValue> ParseValue(const FString& Text);

	/**
	 * An FJsonObject key as an FString. `FJsonObject::Values` is keyed by FString before UE 5.8 and
	 * by UE::FSharedString from 5.8, which doesn't convert to FString implicitly. Read keys through
	 * this, and look fields up with HasField/TryGetField rather than `Values.Find`, so the same code
	 * builds on every engine.
	 */
	inline FString KeyToString(const FString& Key)
	{
		return Key;
	}

	template <typename KeyType>
	FString KeyToString(const KeyType& Key)
	{
		return FString(*Key);
	}

	/** Structural equality; null and missing are equal to each other. */
	bool DeepEquals(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B);

	/**
	 * Splits a configuration path: "MyConfig.Shop.Prices.0" is alias "MyConfig" and segments
	 * ["Shop", "Prices", "0"]. False when the alias is empty.
	 */
	bool SplitConfigPath(const FString& Path, FString& OutAlias, TArray<FString>& OutSegments);

	/** Walks objects by key and arrays by index. Null when any segment is missing. */
	TSharedPtr<FJsonValue> Resolve(const TSharedPtr<FJsonValue>& Root, const TArray<FString>& Segments);

	/** Epoch milliseconds, the marker timestamp unit. */
	int64 NowUnixMilliseconds();

	/** A lowercase dashed UUID, the format the backend's `z.uuid()` accepts. */
	FString NewUuid();
}
