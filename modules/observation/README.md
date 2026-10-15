# Digit passive observation 0.1.0 — operator testing pending

Observes authorized public Slack channels (Slack `channel_type=channel`) and the single configured IRC public channel. The transport modules forward bounded events via the existing host service interface `observation.record`; neither Core nor Router is changed. Slash commands, responses, and intelligence operations remain separate. The observer never replies, decides what is true, extracts relationships, or writes to Corpus.

Records use private append-only JSONL at `/opt/digit/observation/observations.jsonl` (mode 0600) under `/opt/digit/observation` (mode 0750, owned by digit). Fields are timestamp, transport, source channel, source sender, event ID and message text. They are observations, not verified facts or general long-term memory. The module does not observe private Slack channels, groups, DMs, IRC PMs, or bot-authored messages. Source-channel membership, retention, and future interpretation require separate operator direction. Transport behavior is unchanged if the observer service is absent.

Build and install observation first, then install the updated transport modules. Verify passive traffic appends observations without eliciting any response. Full operating-client testing and explicit operator acceptance remain required.
