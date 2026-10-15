# [Gamebeast](https://gamebeast.gg/) Unreal Engine SDK

_Documentation: https://docs.gamebeast.gg/sdk/unreal_

Client SDK for Unreal Engine 5.3 and later. It ships **Markers**, **Remote Configurations**, **Experiments** and **Cohorts**, from both C++ and Blueprint.

## Installation

1. Sign up at https://dashboard.gamebeast.gg/, create an **Unreal** project, and copy its SDK Key from **Project Settings > API Keys**.
2. Copy this repository into your project as `Plugins/Gamebeast` (or add it as a git submodule there).
3. Regenerate project files, build, and enable **Gamebeast** in **Edit > Plugins** if it isn't already.
4. If your game module calls the SDK from C++, add `"Gamebeast"` to `PublicDependencyModuleNames` in its `.Build.cs`.

## Quick start

**No-code setup:** open **Project Settings > Plugins > Gamebeast** and paste your SDK Key. The SDK starts itself on the first frame of every game instance (including Play In Editor). The settings are saved to `Config/DefaultGame.ini`; if your repository is public, move the `[/Script/Gamebeast.GamebeastSettings]` section to an untracked `Config/UserGame.ini`.

**Code setup** (takes precedence when called in your GameInstance's `Init`; needed to pass a `DistinctId` or `UserProperties`):

```cpp
#include "GamebeastSubsystem.h"
#include "GamebeastMarkers.h"
#include "GamebeastConfigs.h"

void UMyGameInstance::Init()
{
    Super::Init();

    FGamebeastSetupOptions Options;
    Options.ApiKey = TEXT("your-sdk-key");
    Options.DistinctId = TEXT("player-123");        // optional: omit for an anonymous, persisted id
    Options.Configurations = { TEXT("MyConfig") };  // optional: preload, and gate OnReady on these
    GetSubsystem<UGamebeastSubsystem>()->SetupWithOptions(Options);
}

// Anywhere with a world context:
UGamebeastSubsystem* Gamebeast = UGamebeastSubsystem::Get(this);

// Markers: batched analytics events, attributed to the local user.
Gamebeast->GetMarkers()->SendMarker(TEXT("LevelCompleted"), FGamebeastProperties()
    .Set(TEXT("Level"), 3)
    .Set(TEXT("Time"), 42.5f)
    .Set(TEXT("Position"), GetActorLocation()));

// Configs: "<ConfigName>.<Key>", the first segment is the configuration alias.
Gamebeast->GetConfigs()->OnReady([this]()
{
    const float Speed = UGamebeastSubsystem::Get(this)->GetConfigs()->GetFloat(TEXT("MyConfig.PlayerSpeed"), 600.f);
});

// React to live changes:
Gamebeast->GetConfigs()->Observe(TEXT("MyConfig.PlayerSpeed"),
    FGamebeastConfigValueCallback::CreateLambda([](const TSharedPtr<FJsonValue>& Value)
    {
        if (Value.IsValid()) { /* Value->AsNumber() */ }
    }));
```

**Blueprint:** get the **Gamebeast Subsystem** node, then **Get Markers > Send Marker** (build properties with **Make Gamebeast Properties** and the **Set Number / Set String / Set Vector ...** nodes), **Get Configs > Get Float**, **Get Configs > On Ready Event**, and the **Is Cohort Member** async node.

## User identity

The SDK identifies the local user by a **distinct id**: the id you supply, or an anonymous id generated once and saved on the device. Configurations are evaluated and experiments assigned per distinct id.

- `Identify(TEXT("player-123"))`: switch identity after login. Configurations re-evaluate for the new user right away. Not persisted: call it again on the next launch.
- `ResetIdentity()`: go back to the device's anonymous id after logout.
- `GetDistinctId()`: the effective id.

## Services

All services hang off `UGamebeastSubsystem`. `Shutdown()` saves what is buffered and stops; `Setup` may be called again afterwards. The subsystem shuts itself down when the game instance ends.

### Markers: `UGamebeastMarkers`

- `SendMarker(MarkerType, Properties)`: send an event. Leave `Properties` empty (`{}` in C++) for none.
- `SendMarkerJson(MarkerType, TSharedPtr<FJsonObject>)`: C++, raw JSON.
- `Flush()`: send what is buffered now.

Markers are batched: a full batch of 10 is sent immediately, otherwise every `MarkerFlushRate` seconds (default 30). Failed sends retry with exponential backoff; permanent rejections (4xx) are dropped and logged. Anything still buffered when the game goes to the background or shuts down is saved under `Saved/Gamebeast/` and sent on the next launch. Vectors are sent as `[x, y, z]`, the shape heatmaps read.

Sessions last one game instance and rotate after `SessionTimeoutSeconds` in the background (default 30 minutes).

### Configs: `UGamebeastConfigs`

Values are addressed by dot-path: the first segment is the configuration alias, the rest walks into the document (`"MyConfig.UI.ButtonColor"`, array index `"MyConfig.Levels.0"`). A bare alias is the whole document.

- `GetString` / `GetInt` / `GetFloat` / `GetBool` / `GetJsonString(Path, Default)`, `HasValue(Path)`, and `GetJson(Path)` in C++.
- `IsReady()` / `OnReady(Callback)` / `OnReadyEvent`: ready once every configuration listed in Setup has loaded.
- `OnChanged(Path, Callback)` / `Observe(Path, Callback)` / `Unsubscribe(Handle)`: C++ change callbacks. `Observe` also fires with the current value.
- `OnConfigurationUpdated`: Blueprint event with the alias of a configuration that just loaded new values.
- `Preload(Alias)` / `Refresh()`.

Configurations load on demand: the first read fetches the configuration. Every configuration is cached on the device (`Saved/Gamebeast/`), so values are there instantly on the next launch and offline. The cache is stamped with your project version (**Project Settings > Description > Project Version**) and the distinct id; a cache from another build or user is discarded. The SDK keeps them current two ways:

- **Project changes** (an edit on the dashboard, an experiment starting or ending): every `StatusPollRate` seconds (default 30) the SDK asks `GET /sdk/v2/status` whether anything changed. Unchanged answers cost almost nothing, and only configurations that moved are re-evaluated. During an incident Gamebeast can ask the SDK to check less often, never more often.
- **Per-player changes** (moving this player to another group, a staged rollout reaching them): every `AssignmentRefreshRate` seconds (default 60) the SDK re-evaluates configurations that have a running experiment. Unchanged ones answer "not modified".

`Refresh()` does both now. Returning from the background does too.

Configurations are **evaluated for the local user**: if they are in an active experiment, the values already include their group's changes.

### Experiments: `UGamebeastExperiments`

Enrollment is automatic when a configuration is evaluated. This service is read-only, for display, segmentation and debugging.

- `GetAssignments()`: the user's assignments across every loaded configuration.
- `OnAssignmentsChanged` (Blueprint) / `OnAssignmentsChangedNative` (C++).

Experiments can target the built-in **Unreal fields** the SDK sends with every evaluation: platform, device, country and language (from the device's settings), game version and engine version. Each is normalized to the vocabulary the dashboard offers, or `unknown`. Target anything else with `FGamebeastSetupOptions::UserProperties` (strings, numbers, booleans, or arrays of one of those).

### Cohorts: `UGamebeastCohorts`

- `IsMember(CohortName, Callback)`: whether the local user is in the cohort (false when unknown). Blueprint: **Is Cohort Member**.

Results are cached for a minute; membership is recomputed by the backend, so treat it as eventually consistent.

## Settings

| Field | Default | Description |
| --- | --- | --- |
| `ApiKey` | none | Required SDK Key. |
| `DistinctId` | auto | Code only. The local user's id; omit for an anonymous, persisted one. |
| `Environment` | `Auto` | `Production` / `Development`. `Auto` is Development in the editor (including PIE) and Production in packaged builds. |
| `CustomEnvironment` | none | Target an environment by its dashboard alias; overrides `Environment`. |
| `Configurations` | none | Configurations to preload and gate `OnReady` on. Others load on first use. |
| `UserProperties` | none | Code only. Custom targeting properties. |
| `Rates.MarkerFlushRate` | `30` | Seconds between marker sends. At least 5. |
| `Rates.StatusPollRate` | `30` | Seconds between checks for configuration and experiment changes. At least 5. |
| `Rates.AssignmentRefreshRate` | `60` | Seconds between re-evaluations of configurations with a running experiment. At least 10. |
| `bFastRatesInEditor` | `true` | In the editor (including PIE), use each rate's minimum (5 / 5 / 10) so changes show up quickly. Packaged builds always use `Rates`. |
| `SessionTimeoutSeconds` | `1800` | New session after this long in the background; `0` keeps one per run. |
| `bDebugLogging` | `false` | Verbose `LogGamebeast` output. |
| `ApiUrl` | `https://api.gamebeast.gg` | API base URL override. |
| `bAutoInitialize` | `true` | Project Settings only. Start without code when an API key is set. |
| `bAutoInitializeOnDedicatedServer` | `false` | Project Settings only. Also auto-start on dedicated servers. |

Values below a rate's minimum are raised to it, with a warning in the log. To change the rates while the game runs, call `UGamebeastSubsystem::UpdateRates(Rates)`; `GetRates()` returns the rates in effect.

## Layout

```
Gamebeast.uplugin
Source/Gamebeast/
  Public/    Subsystem, settings, services, types (the API)
  Private/   Services, HTTP client, marker queue, storage, tests
```

## Tests

Automation tests live in `Source/Gamebeast/Private/Tests`. Run them from **Tools > Session Frontend > Automation** (filter `Gamebeast`), or headless:

```
UnrealEditor-Cmd <YourProject>.uproject -ExecCmds="Automation RunTests Gamebeast; Quit" -unattended -nullrhi
```

## Versioning

`VersionName` in `Gamebeast.uplugin` and `GAMEBEAST_SDK_VERSION` in `Source/Gamebeast/Public/Gamebeast.h` must match (CI checks it). The SDK reports itself as `gamebeast-unreal/<version>`.

Please visit https://docs.gamebeast.gg/ for more details.
