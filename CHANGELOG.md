# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

- **Breaking:** Replace `TPMKIT_WITH_SPDLOG` with `TPMKIT_LOG_ADAPTER={none|spdlog|stdio}` and default it to `none` so adapter selection is explicit and mutually exclusive. Migration: set `-DTPMKIT_LOG_ADAPTER=spdlog` for the spdlog adapter. See [ADR-001](.compozy/tasks/stdio-logger-adapter/adrs/adr-001.md) and [ADR-002](.compozy/tasks/stdio-logger-adapter/adrs/adr-002.md).
- **Breaking:** Default builds no external logging adapter and uses `noop_logger` at runtime, avoiding an implicit logging dependency and unsolicited output. Migration: set `-DTPMKIT_LOG_ADAPTER=stdio` or `-DTPMKIT_LOG_ADAPTER=spdlog` and wire the selected adapter at the composition root. See [ADR-002](.compozy/tasks/stdio-logger-adapter/adrs/adr-002.md).
- **Breaking:** Relocate public logging headers from `<tpmkit/...>` to `<tpmkit/logging/...>` (`logger.h`, `noop_logger.h`, `spdlog_logger.h`, `spdlog_api.h`). `recording_logger.h` stays under `<tpmkit/testing/recording_logger.h>`. Migration: update consumer include paths to the new logging directory. See [ADR-004](.compozy/tasks/stdio-logger-adapter/adrs/adr-004.md).
- Add `stdio_logger`, a zero-dependency stdout/stderr adapter selectable with `TPMKIT_LOG_ADAPTER=stdio`.
