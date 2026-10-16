// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"

/**
 * Environment aliases, normalized the way the backend does (`toAliasSlug` in
 * @gamebeast/sdk-contract, plus the `studio` synonym).
 *
 * One setting: an alias, or empty for automatic. The set of environments is data, not an enum,
 * since projects may define their own (e.g. `staging`).
 *
 * TODO(post-release): custom environments. v2 routes already take any alias, but the v1 routes
 * (markers, cohorts) can only address production and development through `isstudio`, so other
 * environments need a v1 header change or v2 marker/cohort routes before they work end to end.
 */
namespace Gamebeast::Environment
{
	inline const FString Production = TEXT("production");
	inline const FString Development = TEXT("development");

	/** Lowercase, whitespace to `-`, strip non `[a-z0-9-]`, collapse and trim dashes. */
	FString ToAliasSlug(const FString& Value);

	/**
	 * The alias the SDK sends. Empty (or whitespace) Environment is automatic: development when
	 * bIsEditor, else production. Otherwise the normalized alias (`studio` is development). False,
	 * with OutAlias empty, when a non-empty Environment normalizes to nothing (e.g. "!!!").
	 */
	bool Resolve(const FString& Environment, bool bIsEditor, FString& OutAlias);

	/**
	 * `isstudio` for the v1 routes: production is false, development is true. Any other alias
	 * cannot be addressed on v1; it falls back to development (true) and bSupported is false.
	 */
	bool ToLegacyIsStudio(const FString& Alias, bool& bSupported);
}
