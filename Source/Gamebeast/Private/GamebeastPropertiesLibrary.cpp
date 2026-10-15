// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastPropertiesLibrary.h"

FGamebeastProperties UGamebeastPropertiesLibrary::MakeGamebeastProperties()
{
	return FGamebeastProperties();
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetNumber(
	const FGamebeastProperties& Properties, const FString& Key, double Value)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Value);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetInteger(
	const FGamebeastProperties& Properties, const FString& Key, int64 Value)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Value);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetString(
	const FGamebeastProperties& Properties, const FString& Key, const FString& Value)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Value);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetBoolean(
	const FGamebeastProperties& Properties, const FString& Key, bool Value)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Value);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetVector(
	const FGamebeastProperties& Properties, const FString& Key, FVector Value)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Value);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetStringArray(
	const FGamebeastProperties& Properties, const FString& Key, const TArray<FString>& Values)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Values);
}

FGamebeastProperties UGamebeastPropertiesLibrary::SetNested(
	const FGamebeastProperties& Properties, const FString& Key, const FGamebeastProperties& Nested)
{
	FGamebeastProperties Result = Properties;
	return Result.Set(Key, Nested);
}

FGamebeastProperties UGamebeastPropertiesLibrary::FromStringMap(const TMap<FString, FString>& Values)
{
	FGamebeastProperties Result;
	for (const TPair<FString, FString>& Pair : Values)
	{
		Result.Set(Pair.Key, Pair.Value);
	}
	return Result;
}
