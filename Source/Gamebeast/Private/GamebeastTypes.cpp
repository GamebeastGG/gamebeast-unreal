// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "GamebeastTypes.h"

FGamebeastProperties::FGamebeastProperties(const TSharedPtr<FJsonObject>& InJson)
	: Json(InJson)
{
}

FJsonObject& FGamebeastProperties::Mutable()
{
	if (!Json.IsValid())
	{
		Json = MakeShared<FJsonObject>();
	}
	else if (Json.GetSharedReferenceCount() > 1)
	{
		// Copy-on-write: a struct copy shares the object until one side writes.
		TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
		Copy->Values = Json->Values;
		Json = Copy;
	}
	return *Json;
}

FGamebeastProperties& FGamebeastProperties::SetJson(const FString& Key, const TSharedPtr<FJsonValue>& Value)
{
	Mutable().SetField(Key, Value.IsValid() ? Value : MakeShared<FJsonValueNull>());
	return *this;
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, int32 Value)
{
	return SetJson(Key, MakeShared<FJsonValueNumber>(Value));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, int64 Value)
{
	return SetJson(Key, MakeShared<FJsonValueNumber>(static_cast<double>(Value)));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, float Value)
{
	return SetJson(Key, MakeShared<FJsonValueNumber>(Value));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, double Value)
{
	return SetJson(Key, MakeShared<FJsonValueNumber>(Value));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, bool Value)
{
	return SetJson(Key, MakeShared<FJsonValueBoolean>(Value));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const TCHAR* Value)
{
	return SetJson(Key, MakeShared<FJsonValueString>(FString(Value)));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const FString& Value)
{
	return SetJson(Key, MakeShared<FJsonValueString>(Value));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const FName& Value)
{
	return SetJson(Key, MakeShared<FJsonValueString>(Value.ToString()));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const FVector& Value)
{
	TArray<TSharedPtr<FJsonValue>> Components;
	Components.Add(MakeShared<FJsonValueNumber>(Value.X));
	Components.Add(MakeShared<FJsonValueNumber>(Value.Y));
	Components.Add(MakeShared<FJsonValueNumber>(Value.Z));
	return SetJson(Key, MakeShared<FJsonValueArray>(Components));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const FVector2D& Value)
{
	TArray<TSharedPtr<FJsonValue>> Components;
	Components.Add(MakeShared<FJsonValueNumber>(Value.X));
	Components.Add(MakeShared<FJsonValueNumber>(Value.Y));
	return SetJson(Key, MakeShared<FJsonValueArray>(Components));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const FGamebeastProperties& Nested)
{
	return SetJson(Key, MakeShared<FJsonValueObject>(Nested.ToJson()));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FString& Item : Values)
	{
		Items.Add(MakeShared<FJsonValueString>(Item));
	}
	return SetJson(Key, MakeShared<FJsonValueArray>(Items));
}

FGamebeastProperties& FGamebeastProperties::Set(const FString& Key, const TArray<double>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (double Item : Values)
	{
		Items.Add(MakeShared<FJsonValueNumber>(Item));
	}
	return SetJson(Key, MakeShared<FJsonValueArray>(Items));
}

FGamebeastProperties& FGamebeastProperties::Remove(const FString& Key)
{
	if (Json.IsValid() && Json->HasField(Key))
	{
		Mutable().RemoveField(Key);
	}
	return *this;
}

bool FGamebeastProperties::IsEmpty() const
{
	return Num() == 0;
}

int32 FGamebeastProperties::Num() const
{
	return Json.IsValid() ? Json->Values.Num() : 0;
}

TSharedRef<FJsonObject> FGamebeastProperties::ToJson() const
{
	return Json.IsValid() ? Json.ToSharedRef() : MakeShared<FJsonObject>();
}
