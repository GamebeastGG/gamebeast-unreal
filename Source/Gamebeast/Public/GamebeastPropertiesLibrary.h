// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GamebeastPropertiesLibrary.generated.h"

/** Blueprint nodes for building FGamebeastProperties. Each returns a new value; the input is unchanged. */
UCLASS()
class GAMEBEAST_API UGamebeastPropertiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties")
	static FGamebeastProperties MakeGamebeastProperties();

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set Number"))
	static FGamebeastProperties SetNumber(const FGamebeastProperties& Properties, const FString& Key, double Value);

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set Integer"))
	static FGamebeastProperties SetInteger(const FGamebeastProperties& Properties, const FString& Key, int64 Value);

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set String"))
	static FGamebeastProperties SetString(const FGamebeastProperties& Properties, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set Boolean"))
	static FGamebeastProperties SetBoolean(const FGamebeastProperties& Properties, const FString& Key, bool Value);

	/** Stored as `[x, y, z]`. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set Vector"))
	static FGamebeastProperties SetVector(const FGamebeastProperties& Properties, const FString& Key, FVector Value);

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set String Array"))
	static FGamebeastProperties SetStringArray(
		const FGamebeastProperties& Properties, const FString& Key, const TArray<FString>& Values);

	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Set Nested Properties"))
	static FGamebeastProperties SetNested(
		const FGamebeastProperties& Properties, const FString& Key, const FGamebeastProperties& Nested);

	/** Every value is sent as a string. */
	UFUNCTION(BlueprintPure, Category = "Gamebeast|Properties", meta = (DisplayName = "Gamebeast Properties From String Map"))
	static FGamebeastProperties FromStringMap(const TMap<FString, FString>& Values);
};
