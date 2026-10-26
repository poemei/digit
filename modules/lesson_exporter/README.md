# Lesson Exporter — independent autonomous module v1.0.0

**Deployment:** Own systemd service, not a Digit or Rictus Core-loadable module. Does not depend on their ABIs and does not modify them. Module starts at boot, automatically polls both lesson directories, and independently requests Q&A candidates from local Ollama.

## Sources
- `/opt/digit/lessons`
- `/opt/rictus/lessons`

Both are read-only. Candidate dataset and audit: `/var/lib/lesson-exporter/`.

## Install and activate

```sh
sudo make -C modules/lesson_exporter install
sudo systemctl enable --now lesson-exporter.service
sudo systemctl status lesson-exporter.service
sudo journalctl -u lesson-exporter.service -n 50 --no-pager
```

The `lesson-exporter` system user must be able to traverse both source directories and read their lesson text; directory permissions must be configured by the operator. Ollama must be running at `127.0.0.1:11434` with `llama3.2:1b` present.

## Outputs

- `candidate_records.jsonl`: candidate entries containing `source`, `sha256`, `record` and `review_status=PENDING_HUMAN_REVIEW`.
- `audit.jsonl`: quarantined, skipped, and processed sources.
- `training.jsonl` is **not** generated automatically. Approved training data requires explicit human review.

For publication, place reviewed candidate entries into a separate `reviewed_records.jsonl` file and individually mark verified entries `review_status=APPROVED`; then run:

```sh
sudo -u lesson-exporter python3 /opt/lesson-exporter/lesson_exporter.py publish /var/lib/lesson-exporter/reviewed_records.jsonl /var/lib/lesson-exporter/training.jsonl
```

Published records have exactly `{"messages":[{"role":"user","content":"..."},{"role":"assistant","content":"..."}]}`.

## Safety and limitations

- Approved controlled policy revisions require external authoritative-index and integrity evidence. Until supplied, policy documents are quarantined, never silently selected or rewritten.
- Existing 20.6 MB source JSONL is not automatically ingested or validated.
- Structural validation is not factual validation. Human review remains required.
- **Current polling implementation regenerates candidates on each scan**, rather than caching unchanged input; this is a known issue to correct before production activation.
- The build performs a Python syntax check only. No installed runtime or end-to-end Ollama qualification has been verified.
- No Core or Rictus files changed. This module is NOT loaded via either agent's module host.
