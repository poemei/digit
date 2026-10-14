# Digit Slack 0.1.1 — Socket Mode communication transport

Independent native C11 ABI 1.5 transport. **No Intel module dependency.** It receives messages from one configured STN-LABZ workspace, dispatches only the existing anonymous Router `/ask` route, and returns Ask's response to Slack. No Slack actor receives Core command authority.

The private configuration must already exist at `/opt/digit/auth/slack/slack.conf`, owned by root or Digit with mode 0600 and containing:

```ini
enabled=true
workspace_id=<workspace>
app_token=<xapp-token>
bot_token=<xoxb-token>
intel_channel_id=<private-channel-id>
```

Credentials are never committed, printed or logged. Slack Socket Mode must be enabled in the app, the bot invited to the private intelligence channel, and the app configured for app mentions and direct-message events. Necessary bot permissions include `chat:write`, `app_mentions:read`, and `im:history`, with Socket Mode app-level `connections:write`. After changing Slack OAuth scopes/events reinstall the app if required by Slack.

Channel conversation requires a direct `@Digit` app mention **in the configured intel channel only**. Direct messages sent to the bot are also accepted. Other channels, automated messages and all non-addressed messages are ignored. All accepted text is public Ask conversation, never an instruction to Intel. Sessions retain last-request diagnostic context per Slack user+channel in memory, without permanent storage or privilege elevation. The app posts replies in the original thread when available.

Version: `0.1.1`. Compile and test with `make -C modules/slack clean all test`, install using `sudo make -C modules/slack install`, restart the authorized Digit service, and verify `@Digit version?` yields `Digit 1.0.0` in the private channel. Also test `@Digit hello` and `@Digit diagnose my last request`, an unauthorized channel, and a DM. Missing/unsafe config fails closed; transport or API failures are recorded as module messages. Qualification/activation is not acceptance.

No Core or Router changes. Intel collection, investigation, scheduled alerts, Slack commands, and unrelated functionality are out of scope.

## Revision 0.1.1 — Slack response presentation

Slack converts legacy escaped `\\n` line separators in Ask/DSD output into real newlines and emphasizes the DSD report heading using Slack mrkdwn. Plain conversational responses remain unchanged. The formatter is bounded and regression-tested for DSD reports, plain messages, native line breaks, and undersized buffers. No DSD, Core, Router, Intel, or authorization changes. Live Slack validation and explicit acceptance remain pending.

## Intel outbound service — Slack 0.1.2 (awaiting acceptance)

Registers `slack.intel.publish` under the existing module host ABI; accepts a bounded versioned intelligence report, posts it only to the configured intelligence channel, and returns Slack API success/failure. Does not grant Slack users Intel commands or Core authority. Existing Ask and DSD conversation behavior is intended to be preserved and must be regression-tested by the operator.

## 0.1.6 operator test (pending acceptance)

Conversation replies remain in the originating Slack channel, and only existing threads are continued. The module suppresses repeat events for the same workspace, channel and Slack source-message timestamp, in addition to duplicate event IDs. The suppression cache is in-process and bounded (128 messages); this does not prove exactly-once delivery after process restarts.

For operator-only on-demand intelligence reports, add `operator_user_id=U...` to the existing private `/opt/digit/auth/slack/slack.conf`. Use the *actual Slack member ID* shown by Slack profile, not a display name. The option is intentionally optional: when absent, no user is authorized for the Intel report command. Never publish the full report to a private DM automatically; it replies in the channel where the authorized user requested it. Membership in that channel is the visibility boundary—request sensitive briefs only in suitable channels. Other conversation functions still work for channel members.

## Architecture repair note (2026-10-14)

The temporary Intel-specific flag in the shared Ask context was reverted. Slack does not set or depend on it. The previously proposed `operator_user_id`-based on-demand Intel command is not operational or accepted; do not enable it as a means of Intel report authorization. Conversation message deduplication and existing channel replies remain separate behaviors pending live acceptance.

## Slack 0.1.9 — on-demand Intel read (operator testing pending)

An exact `report intel` or `intel report` message from the configured Slack operator is handled directly by Slack's conversational controller, invoking Intel's independently registered `intel.report.read` ABI service. This deliberately does **not** change Core, Router, shared Ask contexts, or Ask's existing greeting/research behavior. The operator identity is `operator_user_id=U...` in the private `/opt/digit/auth/slack/slack.conf`, checked against Slack's authenticated event user ID; without the configured ID, full-report requests are denied. The complete stored bounded Intel brief is returned to the originating Slack DM/channel (or the existing thread). Choose an appropriate channel because its members can read the report. Intel collection remains separate; this reads the latest persisted report. Test real Slack channel and DM delivery and denial from a different user before acceptance. No live deployment or build validation is claimed by these source commits.

## Slack 0.1.10 — operator-directed removal of member-ID gate (testing pending)

The `report intel` command no longer requires or checks `operator_user_id`. Slack accepts an addressed command received through its configured workspace's existing Socket Mode connection and reads Intel's latest brief through the existing `intel.report.read` module service. The response is delivered in the originating channel or DM; for threads, it remains in the existing thread. An obsolete `operator_user_id` entry in `slack.conf` is ignored, but not required. Channel membership determines who can see channel replies. This does not establish per-user access control; anyone able to address Digit in an accessible Slack conversation may request the brief. Earlier README instructions requiring a user ID are superseded by this revision. Core, Router, and Ask ABI are unchanged.

## 0.1.12 — repeated operator requests (operator testing pending)

Each distinct incoming Slack event containing `report intel` requests `intel.report.generate` and sends the fresh response back to the requesting conversation. Distinct Slack message timestamps are not subject to report cooldown; retries of the *same* original event remain deduplicated. Slack continues to have no operator member-ID requirement. This is not a guarantee of delivery during network outages or when Digit is stopped; failures must be reported, not represented as successful reports.
