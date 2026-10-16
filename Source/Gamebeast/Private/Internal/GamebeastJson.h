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
