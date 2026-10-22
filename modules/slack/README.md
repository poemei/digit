# Digit Slack v0.1.8 — ABI 1.5 transport candidate

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
- `learn_operator_user_id` and `learn_authorization_key` are parsed as reserved configuration for future Lessons integration, but Lessons is not active or implemented here.
- Historical STN-2 intelligence investigations, full briefs, persistent alert outbox, Slack login, and memory support are not ported.
- S2 and module lifecycle producers are **not yet connected** to the new delivery endpoints.

## IRC / Slack general-chat bridge

The bridge uses two named ABI services without modifying Core:

- `digit.slack.bridge.deliver`: IRC sends bounded, single-line public chat to the explicitly configured Slack `bridge_channel_id`.
- `digit.irc.bridge.deliver`: Slack sends bounded general chat to the existing configured IRC channel.

Only the selected Slack bridge channel participates. Never configure `bridge_channel_id` to equal `intel_channel_id` or `module_channel_id`; the module rejects this overlap. Slack DMs, other channels, and protected reporting destinations do not bridge. IRC DMs are excluded. Both sides suppress their own bot-origin messages to avoid re-relaying their bridge posts.

Bridged messages carry an origin prefix (IRC or Slack) and may arrive after a transport delay. The bridge is best-effort, not an exactly-once durable queue; it does not change Digit's Conversation decision about which messages warrant responses. Set the public `bridge_channel_id` in the private Slack configuration, and ensure the Slack app receives channel message events. With that field absent, the bridge remains disabled.

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
