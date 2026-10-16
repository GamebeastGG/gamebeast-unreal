// Copyright (c) Gamebeast Inc. All Rights Reserved. Licensed under the Gamebeast Community License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GamebeastTypes.h"

/**
 * Environment aliases, normalized the way the backend does (`toAliasSlug` in
 * @gamebeast/sdk-contract, plus the `studio` synonym).
 *
 * TODO(post-release): custom environments. This copies the Unity SDK's model (Auto / Production
 * / Development plus a CustomEnvironment alias). When the backend ships project-defined
 * environments, revisit: v2 routes already take any alias, but the v1 routes (markers, cohorts)
 * can only address production and development through `isstudio`, so custom environments need a
 * v1 header change or v2 marker/cohort routes before they work end to end.
 */
namespace Gamebeast::Environment
{
	inline const FString Production = TEXT("production");
	inline const FString Development = TEXT("development");

	/** Lowercase, whitespace to `-`, strip non `[a-z0-9-]`, collapse and trim dashes. */
	FString ToAliasSlug(const FString& Value);

	/** The alias the SDK sends. `bIsEditor` decides what Auto means (development in the editor). */
	FString Resolve(EGamebeastEnvironment Environment, const FString& CustomEnvironment, bool bIsEditor);

	/**
	 * `isstudio` for the v1 routes: production is false, development is true. Any other alias
	 * cannot be addressed on v1; it falls back to development (true) and bSupported is false.
	 */
	bool ToLegacyIsStudio(const FString& Alias, bool& bSupported);
}
