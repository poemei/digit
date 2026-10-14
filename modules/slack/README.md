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
