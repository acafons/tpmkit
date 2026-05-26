# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Add `noop_logger::instance()` as a stateless no-op logger reference for adapter defaults and tests.

### Changed

- **Breaking:** Replace `TPMKIT_WITH_SPDLOG` with `TPMKIT_LOG_ADAPTER={none|spdlog|stdio}` and default it to `none` so adapter selection is explicit and mutually exclusive. Migration: set `-DTPMKIT_LOG_ADAPTER=spdlog` for the spdlog adapter. See [ADR-001](.compozy/tasks/stdio-logger-adapter/adrs/adr-001.md) and [ADR-002](.compozy/tasks/stdio-logger-adapter/adrs/adr-002.md).
- **Breaking:** Default builds no external logging adapter and uses `noop_logger` at runtime, avoiding an implicit logging dependency and unsolicited output. Migration: set `-DTPMKIT_LOG_ADAPTER=stdio` or `-DTPMKIT_LOG_ADAPTER=spdlog` and wire the selected adapter at the composition root. See [ADR-002](.compozy/tasks/stdio-logger-adapter/adrs/adr-002.md).
- **Breaking:** Relocate public logging headers from `<tpmkit/...>` to `<tpmkit/logging/...>` (`logger.h`, `noop_logger.h`, `spdlog_logger.h`, `spdlog_api.h`). `recording_logger.h` stays under `<tpmkit/testing/recording_logger.h>`. Migration: update consumer include paths to the new logging directory. See [ADR-004](.compozy/tasks/stdio-logger-adapter/adrs/adr-004.md).
- **Breaking:** Replace `create_esys_pcr_provider(ctx, observer)` with `ctx.create_pcr_provider(observer)`. PCR providers now use the logger configured on the owning `tpm_context`, and ESYS-specific factory wiring is no longer public. Migration: include `<tpmkit/tpm_context.h>`, set `tpm_context_config::log` before creating the context, and call `create_pcr_provider` on the context.
- **Breaking:** Move PCR public API types into the `tpmkit::pcr` namespace and drop the `pcr_` type prefixes. Migration: replace types such as `tpmkit::pcr_index`, `tpmkit::pcr_selection`, and `tpmkit::pcr_provider` with `tpmkit::pcr::index`, `tpmkit::pcr::selection`, and `tpmkit::pcr::provider`.
- Add `stdio_logger`, a zero-dependency stdout/stderr adapter selectable with `TPMKIT_LOG_ADAPTER=stdio`.
