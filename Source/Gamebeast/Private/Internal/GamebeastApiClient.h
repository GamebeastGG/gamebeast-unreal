// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Templates/Function.h"

/** One finished request. */
struct FGamebeastHttpResponse
{
	/** The request never got an HTTP status (offline, DNS, timeout, cancelled). */
	bool bTransportError = false;
	int32 Status = 0;
	FString Body;
	/** From `retry-after`, in seconds; negative when absent. */
	float RetryAfterSeconds = -1.f;

	bool IsSuccess() const { return !bTransportError && Status >= 200 && Status < 300; }
	bool IsNotModified() const { return !bTransportError && Status == 304; }

	/** Transport errors, timeouts, 408, 429 and 5xx are worth retrying; other 4xx are permanent. */
	bool IsRetryable() const
	{
		return bTransportError || Status == 408 || Status == 429 || Status >= 500;
	}

	TSharedPtr<FJsonObject> JsonBody() const;
	/** `errorCode` from an error envelope, or empty. */
	FString ErrorCode() const;
	/** e.g. "HTTP 400 INVALID_MARKERS: ..." or "network error", for logs. */
	FString Describe() const;
};

enum class EGamebeastFetchStatus : uint8
{
	Updated,
	NotModified,
	NotFound,
	Failed,
};

struct FGamebeastFetchResult
{
	EGamebeastFetchStatus Status = EGamebeastFetchStatus::Failed;
	/** The parsed body when Status is Updated. */
	TSharedPtr<FJsonObject> Body;
	FString Hash;
	FString Error;
};

/** Cohort lookup outcome. bOk is false when the request failed (the answer is unknown). */
struct FGamebeastCohortLookup
{
	bool bOk = false;
	bool bCohortExists = false;
	bool bIsMember = false;
};

/**
 * Typed access to the Gamebeast SDK HTTP API (`/sdk/*`). Shapes follow @gamebeast/sdk-contract
 * in GamebeastGG/service-monorepo; change them there, not here.
 *
 * Headers:
 * - every route: `authorization`, `sdkversion: gamebeast-unreal/<version>`
 * - v2: `environment: <alias>`
 * - v1: `isstudio` (production false, development true), `serverid: <session id>`, `universeid: 0`
 *
 * Callbacks run on the game thread.
 */
class FGamebeastApiClient : public TSharedFromThis<FGamebeastApiClient>
{
public:
	using FResponseCallback = TFunction<void(const FGamebeastHttpResponse&)>;
	using FFetchCallback = TFunction<void(const FGamebeastFetchResult&)>;
	using FCohortCallback = TFunction<void(const FGamebeastCohortLookup&)>;

	FGamebeastApiClient(const FString& InApiKey, const FString& InApiUrl, const FString& InEnvironmentAlias);

	/** Sent as v1 `serverid`; the subsystem keeps it in step with the session. */
	FString SessionId;

	const FString& GetEnvironmentAlias() const { return EnvironmentAlias; }

	/** `POST /sdk/v1/markers`. */
	void PostMarkers(const TArray<TSharedPtr<FJsonValue>>& Markers, FResponseCallback Callback);

	/**
	 * `POST /sdk/v2/configurations/evaluate`. The backend resolves the user's experiment
	 * assignments and returns the document with their changes merged in. A 304, or a 200 echoing
	 * KnownHash, is NotModified.
	 */
	void EvaluateConfiguration(
		const FString& Alias,
		const FString& KnownHash,
		const FString& DistinctId,
		const TSharedPtr<FJsonObject>& ContextProperties,
		FFetchCallback Callback);

	/** `GET /sdk/v2/experiments/active?unit-type=user`: names and group labels for display. */
	void GetActiveExperiments(const FString& KnownHash, FFetchCallback Callback);

	/** `POST /sdk/v1/cohorts/membership` for one user. */
	void CheckCohortMembership(const FString& CohortName, const FString& UserId, FCohortCallback Callback);

	static FString UserAgentVersion();

private:
	void Send(
		const FString& Verb,
		const FString& Path,
		const TMap<FString, FString>& Query,
		const TSharedPtr<FJsonObject>& Body,
		bool bLegacyV1,
		float TimeoutSeconds,
		FResponseCallback Callback);

	FString ApiKey;
	FString BaseUrl;
	FString EnvironmentAlias;
	bool bWarnedLegacyEnvironment = false;
};
