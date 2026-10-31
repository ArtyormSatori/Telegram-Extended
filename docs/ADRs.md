# Architecture Decision Records

## ADR-001: C++20 shared domain core
Accepted for the foundation. Keep platform UI/lifecycle native. Freeze ABI only after ownership, cancellation and error contracts are defined.

## ADR-002: Local-first processing
Accepted. No remote processing or remote AI without explicit consent. Provider endpoints remain configurable and are never hard-coded.

## ADR-003: AI is advisory
Accepted. AI produces structured plans; deterministic policy validates capabilities, constraints, stream preservation, paths and safety limits.

## ADR-004: Isolate Telegram upstream
Accepted. Keep domain logic independent from Telegram UI/protocol internals and integrate through narrow adapters. Preserve upstream notices and licenses.

## ADR-005: Remux before transcode
Accepted. Stream-copy only when compatibility and requested transformations permit it; otherwise transcode with disclosed quality impact.
