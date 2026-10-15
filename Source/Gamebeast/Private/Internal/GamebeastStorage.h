// Copyright Gamebeast. Licensed under the Apache License, Version 2.0.

#pragma once

#include "CoreMinimal.h"

/**
 * The SDK's on-device files, under <ProjectSaved>/Gamebeast/. Writes are atomic (temp file then
 * rename) so a crash mid-write never leaves a half-written file behind.
 */
namespace Gamebeast::Storage
{
	FString Directory();
	FString PathFor(const FString& FileName);

	bool Load(const FString& FileName, FString& OutText);
	bool Save(const FString& FileName, const FString& Text);
	void Delete(const FString& FileName);

	/** File-name-safe form of an alias or environment (anything outside [A-Za-z0-9_-] becomes '_'). */
	FString SafeFileComponent(const FString& Value);
}
