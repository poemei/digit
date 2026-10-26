# Rictus Intelligence Module

`intelligence` is Rictus's bounded threat-intelligence watcher.

## Scope

The module consumes the STN-LABZ Threat API `/threats` feed and trusts the
API's explicit `severity` and `relevance` classifications.

- `relevance=LOW` is ignored.
- `relevance=MEDIUM` is recorded as an intelligence candidate.
- `relevance=HIGH` is recorded as an intelligence candidate.
- Rictus does not locally reclassify severity or relevance.
- Fetch, parse, and state failures do not terminate Rictus.

The watcher is persistent. The newest observed threat ID is stored in:

```text
/opt/rictus/state/intelligence.cursor
```

That cursor prevents the entire feed from replaying after a normal restart.

## Source

This module is Rictus-specific. Its intelligence source is compiled into the module:

```text
https://api.stn-labz.com/threats
```

The polling interval is compiled as 60 seconds. No intelligence env file or deployment URL is required.

## Logging

Intelligence findings and watcher diagnostics append to:

```text
/var/log/rictus/rictus.log
```

Example finding:

```text
2026-10-26T01:30:00Z INTELLIGENCE CANDIDATE id=t_example type=pattern_match severity=MEDIUM relevance=HIGH observed=2026-10-26 01:29:58
```

The module logs classification metadata, not raw request payloads or credentials.

## Build and qualification

```sh
make clean
make
make test
```

The qualification suite is offline and deterministic. It validates Threat API
record extraction, relevance threshold behavior, malformed input rejection,
and negative paths without making a network request.

## Install

```sh
sudo make install
```

The hotload artifact is:

```text
/opt/rictus/modules/intelligence/intelligence.so
```

Rictus sees the new shared object, runs qualification, and activates it only
when qualification passes.
