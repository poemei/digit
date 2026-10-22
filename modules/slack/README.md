# Digit Slack v0.1.1 — ABI 1.5 transport candidate

This is a **source candidate**, not a certified or connected Slack deployment.
It is separate from Digit Core and uses the existing ABI 1.5 named service registry.

## Scope

- Socket Mode message delivery for direct messages and app mentions through `digit.conversation`.
- Two registered outgoing delivery endpoints: `digit.slack.intel.deliver` and `digit.slack.modules.deliver`, each restricted to its configured private-channel destination.
- `learn_operator_user_id` and `learn_authorization_key` are parsed as reserved configuration for future Lessons integration, but Lessons is not active or implemented here.
- Historical STN-2 intelligence investigations, full briefs, persistent alert outbox, Slack login, and memory support are not ported.
- S2 and module lifecycle producers are **not yet connected** to the new delivery endpoints.

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

## Verification and limitations

The offline qualification checks syntax-independent helper behaviors, descriptor compatibility, and negative cases. It **does not** prove live Slack delivery, correct all-frame WebSocket handling, event replay deduplication, outbound delivery reliability, authorization for restricted content, S2 full intelligence generation, or shutdown/restart recovery. Manual end-to-end acceptance remains pending.

Current revision is intended for code review; do not equate publishing it with operator activation or security certification.
