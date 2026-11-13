# Changelog

## 0.1.0

First release.

- Markers: batched (sent every `MarkerFlushRate`, default 30s, or when 10 are waiting), retried with backoff, saved on the device when the game backgrounds or shuts down and sent on the next launch. Marker ids are dashed UUIDs, so the backend keeps them.
- Remote configurations: evaluated per user through `POST /sdk/v2/configurations/evaluate`, cached on the device (per build and user), and change callbacks. The SDK polls `GET /sdk/v2/status` (every `StatusPollRate`, default 30s) and re-evaluates only configurations that changed, honouring the backend's load-shedding `polling` floor and `retry-after`. Configurations with a running experiment are also re-evaluated every `AssignmentRefreshRate` (default 60s), so per-player group changes arrive mid-session.
- Experiments: read-only assignments reported by configuration evaluation, with names and group labels from `/status`.
- Cohorts: `IsMember`, reading the `{ cohortExists, users }` response.
- Environments: one `Environment` setting, `production`, `development`, or empty for automatic (development in the editor, production in packaged builds). Other values log a warning and fall back to automatic. Development sends `isstudio: true` on the v1 routes, so development markers stay out of production.
- Project Settings page (no-code setup), C++ and Blueprint APIs.
- Rates (`MarkerFlushRate` ≥ 5s, `StatusPollRate` ≥ 5s, `AssignmentRefreshRate` ≥ 10s) are validated with a warning, use their minimums in the editor (`bFastRatesInEditor`), and can be changed at runtime with `UpdateRates`.
