// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#include "Internal/GamebeastJson.h"

#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace Gamebeast::Json
{
	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	FString Serialize(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = FCondensedWriterFactory::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	FString SerializeValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("null");
		}
		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = FCondensedWriterFactory::Create(&Output);
		FJsonSerializer::Serialize(Value, FString(), Writer);
		return Output;
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Object))
		{
			return nullptr;
		}
		return Object;
	}

	TSharedPtr<FJsonValue> ParseValue(const FString& Text)
	{
		TSharedPtr<FJsonValue> Value;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Value))
		{
			return nullptr;
		}
		return Value;
	}

	static bool IsNullish(const TSharedPtr<FJsonValue>& Value)
	{
		return !Value.IsValid() || Value->Type == EJson::Null || Value->Type == EJson::None;
	}

	bool DeepEquals(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (IsNullish(A) || IsNullish(B))
		{
			return IsNullish(A) && IsNullish(B);
		}
		if (A->Type != B->Type)
		{
			return false;
		}

		switch (A->Type)
		{
		case EJson::Boolean:
			return A->AsBool() == B->AsBool();
		case EJson::Number:
			return A->AsNumber() == B->AsNumber();
		case EJson::String:
			return A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive);
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayA = A->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& ArrayB = B->AsArray();
			if (ArrayA.Num() != ArrayB.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < ArrayA.Num(); ++Index)
			{
				if (!DeepEquals(ArrayA[Index], ArrayB[Index]))
				{
					return false;
				}
			}
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject>& ObjectA = A->AsObject();
			const TSharedPtr<FJsonObject>& ObjectB = B->AsObject();
			if (ObjectA->Values.Num() != ObjectB->Values.Num())
			{
				return false;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ObjectA->Values)
			{
				const TSharedPtr<FJsonValue>* Other = ObjectB->Values.Find(Pair.Key);
				if (Other == nullptr || !DeepEquals(Pair.Value, *Other))
				{
					return false;
				}
			}
			return true;
		}
		default:
			return true;
		}
	}

	bool SplitConfigPath(const FString& Path, FString& OutAlias, TArray<FString>& OutSegments)
	{
		OutAlias.Reset();
		OutSegments.Reset();

		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("."), /*bCullEmpty*/ false);
		if (Parts.Num() == 0)
		{
			return false;
		}

		OutAlias = Parts[0].TrimStartAndEnd();
		if (OutAlias.IsEmpty())
		{
			return false;
		}
		for (int32 Index = 1; Index < Parts.Num(); ++Index)
		{
			OutSegments.Add(Parts[Index]);
		}
		return true;
	}

	/** Digits only: "-1" and "1.5" are keys, not indices. */
	static bool IsArrayIndex(const FString& Segment)
	{
		if (Segment.IsEmpty() || Segment.Len() > 9)
		{
			return false;
		}
		for (TCHAR Char : Segment)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		return true;
	}

	TSharedPtr<FJsonValue> Resolve(const TSharedPtr<FJsonValue>& Root, const TArray<FString>& Segments)
	{
		TSharedPtr<FJsonValue> Current = Root;
		for (const FString& Segment : Segments)
		{
			if (!Current.IsValid())
			{
				return nullptr;
			}
			if (Current->Type == EJson::Object)
			{
				const TSharedPtr<FJsonValue>* Next = Current->AsObject()->Values.Find(Segment);
				Current = Next != nullptr ? *Next : nullptr;
				continue;
			}
			if (Current->Type == EJson::Array && IsArrayIndex(Segment))
			{
				const int32 Index = FCString::Atoi(*Segment);
				const TArray<TSharedPtr<FJsonValue>>& Items = Current->AsArray();
				Current = Items.IsValidIndex(Index) ? Items[Index] : nullptr;
				continue;
			}
			return nullptr;
		}
		return Current;
	}

	int64 NowUnixMilliseconds()
	{
		const FTimespan SinceEpoch = FDateTime::UtcNow() - FDateTime(1970, 1, 1);
		return static_cast<int64>(SinceEpoch.GetTotalMilliseconds());
	}

	FString NewUuid()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens).ToLower();
	}
}
