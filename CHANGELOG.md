# Changelog

## 0.1.0

First release.

- Markers: batched, retried with backoff, saved on the device when the game backgrounds or shuts down and sent on the next launch. Marker ids are dashed UUIDs, so the backend keeps them.
- Remote configurations: evaluated per user through `POST /sdk/v2/configurations/evaluate`, cached on the device (per build and user), hash-based refresh, change callbacks.
- Experiments: read-only assignments reported by configuration evaluation, with names from `GET /sdk/v2/experiments/active`.
- Cohorts: `IsMember`, reading the `{ cohortExists, users }` response.
- Environments: Auto / Production / Development and a custom alias override. Development sends `isstudio: true` on the v1 routes, so development markers stay out of production.
- Project Settings page (no-code setup), C++ and Blueprint APIs.
