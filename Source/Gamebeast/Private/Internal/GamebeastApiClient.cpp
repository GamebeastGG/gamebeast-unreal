// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#include "Internal/GamebeastApiClient.h"

#include "Gamebeast.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Internal/GamebeastEnvironment.h"
#include "Internal/GamebeastJson.h"
#include "Internal/GamebeastLog.h"

namespace
{
	constexpr float DefaultTimeoutSeconds = 10.f;
	/** `MARKER_SERVER_ID_MAX_LENGTH` in @gamebeast/sdk-contract. */
	constexpr int32 MarkerServerIdMaxLength = 100;
	/** `SDK_SERVER_ID_MAX_LENGTH` in @gamebeast/sdk-contract (the `server-id` snapshot header). */
	constexpr int32 SnapshotServerIdMaxLength = 128;

	FString Truncate(const FString& Text, int32 MaxLength = 200)
	{
		return Text.Len() <= MaxLength ? Text : Text.Left(MaxLength) + TEXT("...");
	}
}

TSharedPtr<FJsonObject> FGamebeastHttpResponse::JsonBody() const
{
	return Body.IsEmpty() ? nullptr : Gamebeast::Json::ParseObject(Body);
}

FString FGamebeastHttpResponse::ErrorCode() const
{
	const TSharedPtr<FJsonObject> Json = JsonBody();
	FString Code;
	if (Json.IsValid())
	{
		Json->TryGetStringField(TEXT("errorCode"), Code);
	}
	return Code;
}

FString FGamebeastHttpResponse::Describe() const
{
	if (bTransportError)
	{
		return TEXT("network error");
	}
	FString Description = FString::Printf(TEXT("HTTP %d"), Status);
	const TSharedPtr<FJsonObject> Json = JsonBody();
	FString Code;
	FString Message;
	if (Json.IsValid() && Json->TryGetStringField(TEXT("errorCode"), Code))
	{
		Description += TEXT(" ") + Code;
	}
	if (Json.IsValid() && Json->TryGetStringField(TEXT("message"), Message))
	{
		Description += TEXT(": ") + Truncate(Message);
	}
	return Description;
}

FGamebeastApiClient::FGamebeastApiClient(
	const FString& InApiKey, const FString& InApiUrl, const FString& InEnvironmentAlias)
	: ApiKey(InApiKey)
	, EnvironmentAlias(InEnvironmentAlias)
{
	BaseUrl = InApiUrl.TrimStartAndEnd();
	if (BaseUrl.IsEmpty())
	{
		BaseUrl = TEXT("https://api.gamebeast.gg");
	}
	while (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}
}

FString FGamebeastApiClient::UserAgentVersion()
{
	return FString::Printf(TEXT("%s/%s"), GAMEBEAST_SDK_NAME, GAMEBEAST_SDK_VERSION);
}

void FGamebeastApiClient::Send(
	const FString& Verb,
	const FString& Path,
	const TMap<FString, FString>& Query,
	const TSharedPtr<FJsonObject>& Body,
	bool bLegacyV1,
	float TimeoutSeconds,
	FResponseCallback Callback,
	const TMap<FString, FString>& ExtraHeaders)
{
	FString Url = BaseUrl + TEXT("/sdk/") + Path;
	bool bFirst = true;
	for (const TPair<FString, FString>& Pair : Query)
	{
		Url += bFirst ? TEXT("?") : TEXT("&");
		Url += FGenericPlatformHttp::UrlEncode(Pair.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Pair.Value);
		bFirst = false;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetTimeout(TimeoutSeconds);
	Request->SetHeader(TEXT("accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("authorization"), ApiKey);
	Request->SetHeader(TEXT("sdkversion"), UserAgentVersion());

	if (bLegacyV1)
	{
		// TODO(post-release): custom environments. v1 picks the environment from `isstudio`
		// only, so an alias other than production/development cannot be addressed here.
		bool bSupported = true;
		const bool bIsStudio = Gamebeast::Environment::ToLegacyIsStudio(EnvironmentAlias, bSupported);
		if (!bSupported && !bWarnedLegacyEnvironment)
		{
			bWarnedLegacyEnvironment = true;
			GB_WARN(TEXT("Environment '%s' is not supported for markers and cohorts yet; they go to development."),
				*EnvironmentAlias);
		}
		Request->SetHeader(TEXT("isstudio"), bIsStudio ? TEXT("true") : TEXT("false"));
		Request->SetHeader(TEXT("serverid"), SessionId.Left(MarkerServerIdMaxLength));
		Request->SetHeader(TEXT("universeid"), TEXT("0"));
	}
	else
	{
		Request->SetHeader(TEXT("environment"), EnvironmentAlias);
	}
	for (const TPair<FString, FString>& Header : ExtraHeaders)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}

	if (Body.IsValid())
	{
		Request->SetHeader(TEXT("content-type"), TEXT("application/json"));
		Request->SetContentAsString(Gamebeast::Json::Serialize(Body.ToSharedRef()));
	}

	Request->OnProcessRequestComplete().BindLambda(
		[Callback = MoveTemp(Callback)](FHttpRequestPtr, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully)
		{
			FGamebeastHttpResponse Response;
			if (!bConnectedSuccessfully || !HttpResponse.IsValid())
			{
				Response.bTransportError = true;
			}
			else
			{
				Response.Status = HttpResponse->GetResponseCode();
				Response.Body = HttpResponse->GetContentAsString();
				const FString RetryAfter = HttpResponse->GetHeader(TEXT("retry-after"));
				if (!RetryAfter.IsEmpty() && RetryAfter.IsNumeric())
				{
					Response.RetryAfterSeconds = FCString::Atof(*RetryAfter);
				}
			}
			if (Callback)
			{
				Callback(Response);
			}
		});

	Request->ProcessRequest();
}

void FGamebeastApiClient::PostMarkers(const TArray<TSharedPtr<FJsonValue>>& Markers, FResponseCallback Callback)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("markers"), Markers);
	Send(TEXT("POST"), TEXT("v1/markers"), {}, Body, /*bLegacyV1*/ true, DefaultTimeoutSeconds, MoveTemp(Callback));
}

void FGamebeastApiClient::EvaluateConfiguration(
	const FString& Alias,
	const FString& KnownHash,
	const FString& DistinctId,
	const TSharedPtr<FJsonObject>& EvaluationContext,
	FFetchCallback Callback)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("configuration"), Alias);

	const TSharedRef<FJsonObject> Unit = MakeShared<FJsonObject>();
	Unit->SetStringField(TEXT("type"), TEXT("user"));
	Unit->SetStringField(TEXT("distinctId"), DistinctId);
	Body->SetObjectField(TEXT("unit"), Unit);

	if (EvaluationContext.IsValid())
	{
		Body->SetObjectField(TEXT("context"), EvaluationContext);
	}
	if (!KnownHash.IsEmpty())
	{
		Body->SetStringField(TEXT("knownHash"), KnownHash);
	}

	Send(TEXT("POST"), TEXT("v2/configurations/evaluate"), {}, Body, /*bLegacyV1*/ false, DefaultTimeoutSeconds,
		[KnownHash, Callback = MoveTemp(Callback)](const FGamebeastHttpResponse& Response)
		{
			FGamebeastFetchResult Result;
			if (Response.IsNotModified())
			{
				Result.Status = EGamebeastFetchStatus::NotModified;
				Callback(Result);
				return;
			}

			const FString ErrorCode = Response.ErrorCode();
			if (!Response.bTransportError
				&& (Response.Status == 404 || (Response.Status == 400 && ErrorCode == TEXT("CONFIGURATION_NOT_FOUND"))))
			{
				Result.Status = EGamebeastFetchStatus::NotFound;
				Result.Error = Response.Describe();
				Callback(Result);
				return;
			}

			if (!Response.IsSuccess())
			{
				Result.Error = Response.Describe();
				Callback(Result);
				return;
			}

			const TSharedPtr<FJsonObject> Json = Response.JsonBody();
			FString Hash;
			if (!Json.IsValid() || !Json->TryGetStringField(TEXT("hash"), Hash) || Hash.IsEmpty()
				|| !Json->HasField(TEXT("configuration")))
			{
				Result.Error = TEXT("unexpected configuration response shape");
				Callback(Result);
				return;
			}

			Result.Hash = Hash;
			if (!KnownHash.IsEmpty() && Hash == KnownHash)
			{
				Result.Status = EGamebeastFetchStatus::NotModified;
			}
			else
			{
				Result.Status = EGamebeastFetchStatus::Updated;
				Result.Body = Json;
			}
			Callback(Result);
		});
}

void FGamebeastApiClient::GetStatus(const FString& KnownHash, FFetchCallback Callback)
{
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("server-id"), SessionId.Left(SnapshotServerIdMaxLength));
	if (!KnownHash.IsEmpty())
	{
		// The ETag is the hash, quoted.
		Headers.Add(TEXT("if-none-match"), FString::Printf(TEXT("\"%s\""), *KnownHash));
	}

	Send(TEXT("GET"), TEXT("v2/status"), {}, nullptr, /*bLegacyV1*/ false, DefaultTimeoutSeconds,
		[KnownHash, Callback = MoveTemp(Callback)](const FGamebeastHttpResponse& Response)
		{
			FGamebeastFetchResult Result;
			if (Response.IsNotModified())
			{
				Result.Status = EGamebeastFetchStatus::NotModified;
				Result.Hash = KnownHash;
				Callback(Result);
				return;
			}
			if (!Response.IsSuccess())
			{
				Result.Error = Response.Describe();
				Result.RetryAfterSeconds = Response.RetryAfterSeconds;
				Callback(Result);
				return;
			}

			// `polling` is optional (present only while the backend sheds load), so it isn't required.
			const TSharedPtr<FJsonObject> Json = Response.JsonBody();
			const TArray<TSharedPtr<FJsonValue>>* Configurations = nullptr;
			const TSharedPtr<FJsonObject>* Experiments = nullptr;
			if (!Json.IsValid() || !Json->TryGetStringField(TEXT("hash"), Result.Hash) || Result.Hash.IsEmpty()
				|| !Json->TryGetArrayField(TEXT("configurations"), Configurations)
				|| !Json->TryGetObjectField(TEXT("experiments"), Experiments))
			{
				Result.Error = TEXT("unexpected status response shape");
				Callback(Result);
				return;
			}

			// Some platform HTTP stacks answer a conditional request from their own cache with a 200.
			if (!KnownHash.IsEmpty() && Result.Hash == KnownHash)
			{
				Result.Status = EGamebeastFetchStatus::NotModified;
			}
			else
			{
				Result.Status = EGamebeastFetchStatus::Updated;
				Result.Body = Json;
			}
			Callback(Result);
		},
		Headers);
}

void FGamebeastApiClient::CheckCohortMembership(
	const FString& CohortName, const FString& UserId, FCohortCallback Callback)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("cohortName"), CohortName);
	TArray<TSharedPtr<FJsonValue>> UserIds;
	UserIds.Add(MakeShared<FJsonValueString>(UserId));
	Body->SetArrayField(TEXT("userIds"), UserIds);

	Send(TEXT("POST"), TEXT("v1/cohorts/membership"), {}, Body, /*bLegacyV1*/ true, DefaultTimeoutSeconds,
		[UserId, Callback = MoveTemp(Callback)](const FGamebeastHttpResponse& Response)
		{
			FGamebeastCohortLookup Lookup;
			if (!Response.IsSuccess())
			{
				GB_WARN(TEXT("Cohort membership check failed: %s"), *Response.Describe());
				Callback(Lookup);
				return;
			}

			// { cohortExists: boolean, users: [{ userId, isMember }] }
			const TSharedPtr<FJsonObject> Json = Response.JsonBody();
			const TArray<TSharedPtr<FJsonValue>>* Users = nullptr;
			if (!Json.IsValid() || !Json->TryGetBoolField(TEXT("cohortExists"), Lookup.bCohortExists)
				|| !Json->TryGetArrayField(TEXT("users"), Users))
			{
				GB_WARN(TEXT("Unexpected cohort membership response shape."));
				Callback(Lookup);
				return;
			}

			Lookup.bOk = true;
			for (const TSharedPtr<FJsonValue>& Entry : *Users)
			{
				const TSharedPtr<FJsonObject>* EntryObject = nullptr;
				FString EntryUserId;
				bool bIsMember = false;
				if (Entry.IsValid() && Entry->TryGetObject(EntryObject)
					&& (*EntryObject)->TryGetStringField(TEXT("userId"), EntryUserId) && EntryUserId == UserId
					&& (*EntryObject)->TryGetBoolField(TEXT("isMember"), bIsMember))
				{
					Lookup.bIsMember = bIsMember;
				}
			}
			Callback(Lookup);
		});
}
