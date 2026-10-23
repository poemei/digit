# Digit Slack v0.1.11 — ABI 1.5 transport candidate

This is a **source candidate**, not a certified or connected Slack deployment.
It is separate from Digit Core and uses the existing ABI 1.5 named service registry.

## Scope

- Socket Mode message delivery for direct messages and app mentions through `digit.conversation`.
- Every eligible public/private channel message is submitted to the same Observation ABI service and SQLite3 fallback used by IRC, whether Digit responds or not. General conversation then reaches `digit.conversation.listen`; Conversation decides relevance and participation.
- A leading `digit?`, `digicat?`, `digit ...`, `digit, ...`, or `digicat: ...` in a Slack channel takes the direct Conversation route just as an explicit IRC nick address does. A Slack `@Digit` mention also uses the direct route. Other ambient chat remains observational.
- Direct messages and addressed `app_mention` events use `digit.conversation`; Slack `<@USER>` markup is stripped from addressed mentions. The transport does not manufacture a greeting or override Conversation's participation decision.
- DM-only DSD `show dsd`, `show trace`, and `diagnose my last request` commands now invoke the existing DSD ABI service. The transport stores each Slack user's previous DM trace separately. Unscoped trace/module commands remain denied. General channels cannot invoke DSD.
- Channel event classification falls back to Slack channel IDs when `channel_type` is absent; it continues to reject bot and subtype events.
- Two registered outgoing delivery endpoints: `digit.slack.intel.deliver` and `digit.slack.modules.deliver`, each restricted to its configured private-channel destination.
- Operational feedback: Slack reads **new** authoritative loader audit events from `/opt/digit/logs/modules.log` and forwards admitted/replaced, denied, and failed hot-load outcomes to the configured private `module_channel_id` (intended for `#digit-ops`). Failure reports never claim a candidate was activated. The module does not replay the historical log when first activated, and retries unsuccessful Slack posts from their original file offset while running.
- SQLite3 ingestion sends `[SQLITE MEMORY] ...` sync reports to the existing `digit.slack.modules.deliver` endpoint with an independent bounded retry queue. The current SQLite ingestion text reports per-source added, updated, skipped counts rather than the historical aggregate example.
- `learn_operator_user_id` and `learn_authorization_key` are parsed as reserved configuration for future Lessons integration, but Lessons is not active or implemented here.
- Historical STN-2 intelligence investigations, full briefs, persistent alert outbox, Slack login, and memory support are not ported.
- S2 intelligence generation remains separate; no complete historical S2 brief workflow has been restored. SQLite ingest reporting and successful module hotload reporting are now wired to the operations destination.

## IRC bridge removed (v0.1.10)

The two-way IRC/Slack general-chat bridge is removed. The legacy `bridge_channel_id` key is ignored for compatibility with existing private configuration. Direct Slack conversations, private intelligence delivery and module operations remain operational. No chat is forwarded to or from IRC.

## Build

From the current Digit repository root, with the existing ABI 1.5 headers and required libraries:

```sh
make -C modules/slack test
```

Dependencies: C11 compiler, libcurl built with WebSocket support, json-c, pthread and dlopen.

After review and module qualification, the operator controls installation and activation. Core changes are not needed.

## Configuration

Create `/opt/digit/auth/slack/slack.conf` with owner root or the Digit process owner, ordinary file, permissions `0600`. Use `slack.conf.example`. The configuration must include workspace, app and bot tokens, and private channel IDs. Invite the app to private channels. **Never commit or post credentials.**

Previously exposed Slack and learning credentials must be rotated before live activation. The candidate has no credentials embedded.

## Required Slack event subscriptions

In Slack app configuration, subscribe to bot events `message.channels` and `message.groups` as appropriate, with `channels:history` and `groups:history` scopes; retain `message.im` (`im:history`) and `app_mention` (`app_mentions:read`). Reinstall after scope changes and add Digit to channels to be observed. Without those events, the module cannot observe ordinary chat even while Socket Mode is connected.

## Verification and limitations

The offline qualification checks helper behavior, mention normalization, direct/passive classification (including missing `channel_type`), DSD command classification, descriptor compatibility and negative cases. These tests do not constitute a live DSD or Slack integration test. It **does not** prove live Slack delivery, correct all-frame WebSocket handling, event replay deduplication, outbound delivery reliability, authorization for restricted content, S2 full intelligence generation, or shutdown/restart recovery. Manual end-to-end acceptance remains pending.

Current revision is intended for code review; do not equate publishing it with operator activation or security certification.
