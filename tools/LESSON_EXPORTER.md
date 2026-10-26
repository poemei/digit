# Independent Lesson Exporter — review gate

This utility lives in `tools/lesson_exporter.py`; it is **not a Digit or Rictus module** and is never loaded by either Core.

## Boundaries

- Source directories are read-only: `/opt/digit/lessons` and `/opt/rictus/lessons`.
- Output defaults to `/var/lib/lesson-exporter`, outside both agent installations.
- `scan` generates a review queue, **not** an approved training dataset.
- Each generated candidate contains a two-message `messages` record, plus source metadata held outside that record.
- `publish` emits *only* the exact two-message JSONL schema, one object per line, after human review.
- Controlled/revisioned documents are quarantined until an authoritative revision/approval/integrity mechanism is provided. Approval claimed inside a document is not sufficient.
- A structurally correct generated answer may still be wrong; reviewers must check every candidate against its original source. This script does not prove semantic fidelity.
- Existing `digit_approved_learning_messages.jsonl` is **not** imported or validated by this utility. It must be audited separately before inclusion.
- No GGUF is generated here; a later fine-tuning/conversion workflow is required.

## Usage

Prerequisites: Python 3.10+, locally running Ollama, and a locally installed model (default `llama3.2:1b`). No third-party Python packages are required.

One-shot candidate generation:

```sh
python3 tools/lesson_exporter.py scan --output /var/lib/lesson-exporter
```

Watch by periodic scanning (seconds):

```sh
python3 tools/lesson_exporter.py scan --watch 300 --output /var/lib/lesson-exporter
```

Review `audit.jsonl` for excluded sources. Review `candidate_records.jsonl` against the original lesson text. Change `review_status` from `PENDING_HUMAN_REVIEW` to `APPROVED` **only for individually verified entries**, and save the reviewed queue as a *separate file* (watching overwrites the generated candidate queue at the next pass). For example, `reviewed_records.jsonl`.

Publish the checked Q&A dataset:

```sh
python3 tools/lesson_exporter.py publish \
  /var/lib/lesson-exporter/reviewed_records.jsonl \
  /var/lib/lesson-exporter/training.jsonl
```

The resulting `training.jsonl` rows are precisely:

```json
{"messages":[{"role":"user","content":"Question"},{"role":"assistant","content":"Supported answer"}]}
```

## Status and limitations

- Source code committed for review; **no build, local runtime, Ollama, dataset-generation, or integration tests have been executed**.
- Re-scans regenerate candidates; **there is no hash-based generation cache yet**.
- Source checks impose a conservative 1 MiB limit on text lessons; the existing large JSONL is excluded.
- Exports are atomic, but external locking/concurrent-run handling is not implemented.
- The current process is deliberately *fail-closed*: controlled-document approval and revision resolution remain unresolved, so those files do not produce candidates.
- Human approval currently uses explicit reviewed-file status; deployment should additionally protect that file from unauthorized modification.
