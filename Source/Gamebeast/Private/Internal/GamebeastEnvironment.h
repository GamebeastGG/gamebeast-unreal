// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"

/**
 * Environment aliases, normalized the way the backend does (`toAliasSlug` in
 * @gamebeast/sdk-contract, plus the `studio` synonym).
 *
 * One setting: an alias, or empty for automatic. Only production and development exist today;
 * any other value is warned about and treated as empty.
 *
 * TODO(post-release): custom environments. When projects can define their own (e.g. `staging`),
 * let Resolve pass other aliases through. v2 routes already take any alias, but the v1 routes
 * (markers, cohorts) can only address production and development through `isstudio`, so other
 * environments also need a v1 header change or v2 marker/cohort routes to work end to end.
 */
namespace Gamebeast::Environment
{
	inline const FString Production = TEXT("production");
	inline const FString Development = TEXT("development");

	/** Lowercase, whitespace to `-`, strip non `[a-z0-9-]`, collapse and trim dashes. */
	FString ToAliasSlug(const FString& Value);

	/**
	 * The alias the SDK sends: production or development. Empty (or whitespace) is automatic:
	 * development when bIsEditor, else production. `studio` is development. Any other value is
	 * unsupported for now: bOutUnsupported is set and the automatic alias is returned.
	 */
	FString Resolve(const FString& Environment, bool bIsEditor, bool& bOutUnsupported);

	/**
	 * `isstudio` for the v1 routes: production is false, development is true. Any other alias
	 * cannot be addressed on v1; it falls back to development (true) and bSupported is false.
	 */
	bool ToLegacyIsStudio(const FString& Alias, bool& bSupported);
}
