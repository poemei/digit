# Digit passive observation 0.1.1 — operator testing pending

Observes authorized public Slack channels (Slack `channel_type=channel`) and the single configured IRC public channel. The transport modules forward bounded events via the existing host service interface `observation.record`; neither Core nor Router is changed. Slash commands, responses, and intelligence operations remain separate. The observer never replies, decides what is true, extracts relationships, or writes to Corpus.

Records use private append-only JSONL at `/opt/digit/observation/observations.jsonl` (mode 0600) under `/opt/digit/observation` (mode 0750, owned by digit). Fields are timestamp, transport, source channel, source sender, event ID and message text. They are observations, not verified facts or general long-term memory. The module does not observe private Slack channels, groups, DMs, IRC PMs, or bot-authored messages. Source-channel membership, retention, and future interpretation require separate operator direction. Transport behavior is unchanged if the observer service is absent.

Build and install observation first, then install the updated transport modules. Verify passive traffic appends observations without eliciting any response. Full operating-client testing and explicit operator acceptance remain required.

## Bounded read service (0.1.1)

`observation.recent` is a read-only service retrieving at most six of the most recent stored observations for an explicitly supplied public transport/channel scope. Requests specify `version=1`, `transport` (`irc` or `slack`), `channel`, and `limit` (1–6). Results are ordered oldest to newest within the returned window and include original time, sender, event ID, and message text. The reader validates the owner and private file permissions, rejects symlinks, and scans only the last 256 KiB of the JSONL file. Older entries beyond that suffix are not consulted.

The service does **not** make a relevance determination, authorize a request, provide independent factual verification, or modify Corpus. It is not wired into Ask or Interpretation; such integration requires a separately authorized development step. The service is internal to the module host, not a public chat command. Caller identity/access control is not supplied by this ABI; the service only reads the public-channel observations and must not be extended to private data without explicit access-control design.
