// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Internal/GamebeastRates.h"
#include "Templates/Function.h"

class FGamebeastApiClient;

/**
 * Polls `GET /sdk/v2/status` and reports each snapshot that changed. Driven by Tick on the game
 * thread; owns no timers.
 *
 * The contract (core-backend `src/sdk/v2/AGENTS.md`): send the last hash as `If-None-Match`; a
 * 304 is no work. While a 200 carries `polling`, poll no faster than `polling.intervalSeconds` and
 * spread each wait by `polling.jitterRatio`. Honour `retry-after`, and back off on failures.
 *
 * Unlike the Node.js SDK, a 200 without `polling` clears the override: the backend adds and removes
 * it with the snapshot hash, so its absence on a fresh snapshot means the incident is over.
 */
class FGamebeastStatusPoller
{
public:
	/** Called with every 200 whose hash differs from the last one seen (the first included). */
	using FOnSnapshot = TFunction<void(const TSharedRef<FJsonObject>& /* Status */)>;

	FGamebeastStatusPoller(const TSharedRef<FGamebeastApiClient>& InApi, float InPollRate, FOnSnapshot InOnSnapshot);
	~FGamebeastStatusPoller();

	/** The first Tick polls right away. */
	void Tick(float DeltaSeconds);

	/** Poll now (after the request in flight, if any) and restart the schedule from it. */
	void PollNow();

	/** Takes effect from the next scheduled poll. Already validated (Gamebeast::Rates). */
	void SetPollRate(float Seconds) { PollRate = Seconds; }

	/** Whether a snapshot has been received. */
	bool HasSnapshot() const { return !KnownHash.IsEmpty(); }

	/** The backend's load-shedding floor, while one is in force. */
	const Gamebeast::Rates::FPollingOverride& GetOverride() const { return Override; }

	/** `polling` from a status body; bPresent is false when the field is absent or malformed. */
	static Gamebeast::Rates::FPollingOverride ParseOverride(const FJsonObject& Status);

	/** What a client re-evaluates against, keyed by normalized alias (`toAliasSlug`). */
	struct FSummary
	{
		/**
		 * Content hash plus every user experiment built on the configuration. An evaluated
		 * configuration changes when either does: an edit moves the hash, and starting or ending
		 * an experiment (or changing its groups or schedule) moves its descriptor. Changes the
		 * descriptor doesn't show, such as per-user assignments, are AssignmentRefreshRate's job.
		 */
		TMap<FString, FString> Fingerprints;
		/** Configurations with at least one active user experiment: the assignment refresh set. */
		TSet<FString> WithUserExperiments;
	};

	/** Configurations without an alias can't be evaluated by name and are skipped. */
	static FSummary Summarize(const FJsonObject& Status);

private:
	void Poll();
	void Schedule(float RetryAfterSeconds);

	TWeakPtr<FGamebeastApiClient> Api;
	FOnSnapshot OnSnapshot;
	float PollRate = Gamebeast::Rates::MinStatusPollRate;

	FString KnownHash;
	Gamebeast::Rates::FPollingOverride Override;
	int32 ConsecutiveFailures = 0;
	float SecondsUntilPoll = 0.f;
	bool bInFlight = false;
	bool bPollQueued = false;

	/** Lets in-flight callbacks notice the poller was destroyed. */
	TSharedRef<bool> Alive = MakeShared<bool>(true);
};
