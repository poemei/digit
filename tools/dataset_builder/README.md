# STN-LABZ Dataset Builder

Operator-side dataset preparation tool for the STN-LABZ model-development path.

It does **not** modify Digit, her Core, her SQLite3 module, or her database.

## Input

Default database:

```text
/opt/digit/data/digit_memory.db
```

The database is opened read-only. Only Corpus records satisfying both conditions are selected:

- `category = LESSON` (case-insensitive)
- `verification_status = operator-verified`

Observations, Context, IRC conversation records, and unverified Corpus records are not exported.

## Generation

Each approved lesson is split into bounded UTF-8-safe chunks. The local Ollama endpoint:

```text
http://127.0.0.1:11434/api/generate
```

using:

```text
llama3.2:1b
```

is asked to transform each chunk into one natural instruction/response training example. The model is instructed to preserve the source meaning, avoid unsupported claims, and avoid synthetic database-record wording.

Ollama output is treated as a candidate transformation, not source authority.

## Output

The training JSONL contains only normal conversational training records:

```json
{"messages":[{"role":"user","content":"..."},{"role":"assistant","content":"..."}]}
```

Provenance is written separately so database metadata does not become conversational training text:

```json
{"line":1,"corpus_id":42,"source_record_id":"lesson:example.txt","source":"/opt/digit/lessons/example.txt","category":"LESSON","verification_status":"operator-verified","chunk":1}
```

## Build

```sh
make -C tools/dataset_builder
```

## Run

```sh
./tools/dataset_builder/dataset_builder \
  --output stn_training.jsonl \
  --provenance stn_training.provenance.jsonl
```

Useful operator test option:

```sh
./tools/dataset_builder/dataset_builder \
  --output test.jsonl \
  --provenance test.provenance.jsonl \
  --limit 5
```

Existing output files are not overwritten. Use new filenames for another run.
