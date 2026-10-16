# Contributing

Use small modules and native ESP-IDF facilities. Original contributions are
accepted under the MIT license; preserve upstream notices. Describe hardware
revision, IDF version, protocol effects and verification in changes. Never change
an existing physical board's saved calibration based only on another board.

Run `./tools/test.sh`, `./tools/test-web.sh` and `./tools/build.sh` as appropriate.
Keep acquisition separate from visualization and statistical interpretation;
changes to sampling, health checks or export format need explicit algorithm/schema
versioning and reproducibility analysis. Never make a deterministic stream appear
to be hardware entropy. Do not infer psychokinetic causation from a p-value.

Hardware qualification follows `docs/validation.md`; include actual measurements
and distinguish them from simulations or mocks. Do not commit private flash
backups, tokens, Wi-Fi credentials, participant data or full onboarding logs.
