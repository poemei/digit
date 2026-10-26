#define _POSIX_C_SOURCE 200809L

#include <curl/curl.h>
#include <errno.h>
#include <json-c/json.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DB "/opt/digit/data/digit_memory.db"
#define OLLAMA_URL "http://127.0.0.1:11434/api/generate"
#define OLLAMA_MODEL "llama3.2:1b"
#define LESSON_CHUNK_MAX 6000U
#define HTTP_BODY_MAX (256U * 1024U)
#define INSTRUCTION_MAX 2048U
#define RESPONSE_MAX 8192U

typedef struct {
    char *data;
    size_t used;
    size_t capacity;
} buffer_t;

typedef struct {
    const char *db_path;
    const char *output_path;
    const char *provenance_path;
    unsigned long limit;
} options_t;

static int grow(buffer_t *buffer, size_t need)
{
    size_t capacity;
    char *next;

    if (!buffer || need > HTTP_BODY_MAX + 1U) return 0;
    if (need <= buffer->capacity) return 1;

    capacity = buffer->capacity ? buffer->capacity : 4096U;
    while (capacity < need) {
        if (capacity >= HTTP_BODY_MAX / 2U) {
            capacity = HTTP_BODY_MAX + 1U;
            break;
        }
        capacity *= 2U;
    }
    if (capacity < need || capacity > HTTP_BODY_MAX + 1U) return 0;
    next = realloc(buffer->data, capacity);
    if (!next) return 0;
    buffer->data = next;
    buffer->capacity = capacity;
    return 1;
}

static size_t capture(void *contents, size_t size, size_t count, void *context)
{
    buffer_t *buffer = context;
    size_t bytes;
    size_t need;

    if (!buffer || !contents || !size || !count || count > SIZE_MAX / size)
        return 0;
    bytes = size * count;
    if (buffer->used > HTTP_BODY_MAX || bytes > HTTP_BODY_MAX - buffer->used)
        return 0;
    need = buffer->used + bytes + 1U;
    if (!grow(buffer, need)) return 0;
    memcpy(buffer->data + buffer->used, contents, bytes);
    buffer->used += bytes;
    buffer->data[buffer->used] = '\0';
    return bytes;
}

static int regular_text(const char *text, size_t maximum)
{
    size_t i;
    if (!text || !*text) return 0;
    for (i = 0U; text[i]; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (i >= maximum) return 0;
        if ((c < 0x20U && c != '\n' && c != '\r' && c != '\t') ||
            c == 0x7fU)
            return 0;
    }
    return 1;
}

static size_t utf8_chunk_length(const char *text, size_t remaining)
{
    size_t length;
    size_t floor;
    size_t i;

    if (remaining <= LESSON_CHUNK_MAX) return remaining;
    length = LESSON_CHUNK_MAX;

    while (length > 0U &&
           (((unsigned char)text[length] & 0xc0U) == 0x80U))
        --length;
    if (!length) return 0U;

    floor = length > 1000U ? length - 1000U : 0U;
    for (i = length; i > floor; --i) {
        unsigned char c = (unsigned char)text[i - 1U];
        if (c == '\n' || c == ' ' || c == '\t') {
            length = i;
            break;
        }
    }
    return length;
}

static int ollama_example(const char *lesson,
                          size_t lesson_length,
                          char **instruction_out,
                          char **response_out)
{
    static const char system_text[] =
        "You prepare high-quality supervised fine-tuning examples for the "
        "shared STN-LABZ language model. Use only the supplied approved lesson. "
        "Return exactly one JSON object with keys instruction and response. "
        "Write natural English. The instruction must sound like a real useful "
        "question or request, not a database record description. The response "
        "must faithfully teach the supplied lesson and must not add unsupported "
        "facts. Do not mention SQLite, Corpus record numbers, verification "
        "labels, training data, or provenance unless the lesson itself teaches "
        "those concepts. Do not invent an agent identity. If the lesson is "
        "explicitly about a named STN-LABZ agent, preserve that fact accurately.";
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    CURLcode curl_result;
    long status = 0L;
    json_object *request = NULL;
    json_object *options = NULL;
    json_object *root = NULL;
    json_object *response_value = NULL;
    json_object *generated = NULL;
    json_object *instruction_value = NULL;
    json_object *answer_value = NULL;
    buffer_t body = {0};
    char *chunk = NULL;
    const char *encoded;
    const char *generated_text;
    const char *instruction;
    const char *answer;
    int ok = 0;

    *instruction_out = NULL;
    *response_out = NULL;

    chunk = malloc(lesson_length + 1U);
    if (!chunk) goto done;
    memcpy(chunk, lesson, lesson_length);
    chunk[lesson_length] = '\0';

    request = json_object_new_object();
    options = json_object_new_object();
    if (!request || !options) goto done;

    json_object_object_add(request, "model",
                           json_object_new_string(OLLAMA_MODEL));
    json_object_object_add(request, "system",
                           json_object_new_string(system_text));
    json_object_object_add(request, "prompt",
                           json_object_new_string(chunk));
    json_object_object_add(request, "stream",
                           json_object_new_boolean(0));
    json_object_object_add(request, "format",
                           json_object_new_string("json"));
    json_object_object_add(options, "temperature",
                           json_object_new_double(0.2));
    json_object_object_add(options, "num_predict",
                           json_object_new_int(768));
    json_object_object_add(request, "options", options);
    options = NULL;

    encoded = json_object_to_json_string_ext(request,
                                             JSON_C_TO_STRING_PLAIN);
    if (!encoded) goto done;

    curl = curl_easy_init();
    if (!curl) goto done;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!headers) goto done;

    curl_easy_setopt(curl, CURLOPT_URL, OLLAMA_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encoded);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(encoded));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, capture);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_PROXY, "");
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    curl_result = curl_easy_perform(curl);
    if (curl_result != CURLE_OK) goto done;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK ||
        status != 200L || !body.data)
        goto done;

    root = json_tokener_parse(body.data);
    if (!root || !json_object_is_type(root, json_type_object)) goto done;
    if (!json_object_object_get_ex(root, "response", &response_value) ||
        !json_object_is_type(response_value, json_type_string))
        goto done;
    generated_text = json_object_get_string(response_value);
    if (!generated_text || !*generated_text) goto done;

    generated = json_tokener_parse(generated_text);
    if (!generated || !json_object_is_type(generated, json_type_object))
        goto done;
    if (!json_object_object_get_ex(generated, "instruction",
                                   &instruction_value) ||
        !json_object_is_type(instruction_value, json_type_string) ||
        !json_object_object_get_ex(generated, "response",
                                   &answer_value) ||
        !json_object_is_type(answer_value, json_type_string))
        goto done;

    instruction = json_object_get_string(instruction_value);
    answer = json_object_get_string(answer_value);
    if (!regular_text(instruction, INSTRUCTION_MAX) ||
        !regular_text(answer, RESPONSE_MAX))
        goto done;

    *instruction_out = strdup(instruction);
    *response_out = strdup(answer);
    if (!*instruction_out || !*response_out) goto done;
    ok = 1;

done:
    if (!ok) {
        free(*instruction_out);
        free(*response_out);
        *instruction_out = NULL;
        *response_out = NULL;
    }
    free(chunk);
    free(body.data);
    if (generated) json_object_put(generated);
    if (root) json_object_put(root);
    if (options) json_object_put(options);
    if (request) json_object_put(request);
    curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    return ok;
}

static int write_training(FILE *file,
                          const char *instruction,
                          const char *response)
{
    json_object *root = NULL;
    json_object *messages = NULL;
    json_object *user = NULL;
    json_object *assistant = NULL;
    const char *encoded;
    int ok = 0;

    root = json_object_new_object();
    messages = json_object_new_array();
    user = json_object_new_object();
    assistant = json_object_new_object();
    if (!root || !messages || !user || !assistant) goto done;

    json_object_object_add(user, "role", json_object_new_string("user"));
    json_object_object_add(user, "content",
                           json_object_new_string(instruction));
    json_object_object_add(assistant, "role",
                           json_object_new_string("assistant"));
    json_object_object_add(assistant, "content",
                           json_object_new_string(response));
    json_object_array_add(messages, user);
    user = NULL;
    json_object_array_add(messages, assistant);
    assistant = NULL;
    json_object_object_add(root, "messages", messages);
    messages = NULL;

    encoded = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!encoded || fprintf(file, "%s\n", encoded) < 0) goto done;
    ok = 1;

done:
    if (assistant) json_object_put(assistant);
    if (user) json_object_put(user);
    if (messages) json_object_put(messages);
    if (root) json_object_put(root);
    return ok;
}

static int write_provenance(FILE *file,
                            unsigned long line_number,
                            sqlite3_int64 corpus_id,
                            const char *source_record_id,
                            const char *source,
                            const char *category,
                            const char *verification,
                            unsigned int chunk_number)
{
    json_object *root = json_object_new_object();
    const char *encoded;
    int ok = 0;

    if (!root) return 0;
    json_object_object_add(root, "line",
                           json_object_new_int64((int64_t)line_number));
    json_object_object_add(root, "corpus_id",
                           json_object_new_int64(corpus_id));
    json_object_object_add(root, "source_record_id",
                           json_object_new_string(source_record_id ?
                                                  source_record_id : ""));
    json_object_object_add(root, "source",
                           json_object_new_string(source ? source : ""));
    json_object_object_add(root, "category",
                           json_object_new_string(category ? category : ""));
    json_object_object_add(root, "verification_status",
                           json_object_new_string(verification ?
                                                  verification : ""));
    json_object_object_add(root, "chunk",
                           json_object_new_int((int)chunk_number));
    encoded = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (encoded && fprintf(file, "%s\n", encoded) >= 0) ok = 1;
    json_object_put(root);
    return ok;
}

static void usage(const char *program)
{
    fprintf(stderr,
        "usage: %s --output FILE --provenance FILE [--db FILE] [--limit N]\n",
        program);
}

static int parse_options(int argc, char **argv, options_t *options)
{
    int i;

    memset(options, 0, sizeof *options);
    options->db_path = DEFAULT_DB;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--output") && i + 1 < argc) {
            options->output_path = argv[++i];
        } else if (!strcmp(argv[i], "--provenance") && i + 1 < argc) {
            options->provenance_path = argv[++i];
        } else if (!strcmp(argv[i], "--db") && i + 1 < argc) {
            options->db_path = argv[++i];
        } else if (!strcmp(argv[i], "--limit") && i + 1 < argc) {
            char *end = NULL;
            errno = 0;
            options->limit = strtoul(argv[++i], &end, 10);
            if (errno || !end || *end) return 0;
        } else {
            return 0;
        }
    }

    return options->output_path && options->provenance_path;
}

int main(int argc, char **argv)
{
    static const char sql[] =
        "SELECT id,category,source,text,verification_status,source_record_id "
        "FROM corpus "
        "WHERE verification_status='operator-verified' "
        "AND upper(category)='LESSON' "
        "ORDER BY id";
    options_t options;
    sqlite3 *database = NULL;
    sqlite3_stmt *statement = NULL;
    FILE *output = NULL;
    FILE *provenance = NULL;
    unsigned long rows = 0U;
    unsigned long emitted = 0U;
    unsigned long failed = 0U;
    int rc;
    int exit_code = 1;

    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "dataset_builder: curl initialization failed\n");
        return 1;
    }

    rc = sqlite3_open_v2(options.db_path, &database,
                         SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "dataset_builder: cannot open database read-only: %s\n",
                database ? sqlite3_errmsg(database) : "no handle");
        goto done;
    }
    if (sqlite3_db_readonly(database, "main") != 1) {
        fprintf(stderr, "dataset_builder: database is not read-only\n");
        goto done;
    }

    output = fopen(options.output_path, "wx");
    if (!output) {
        fprintf(stderr, "dataset_builder: cannot create %s: %s\n",
                options.output_path, strerror(errno));
        goto done;
    }
    provenance = fopen(options.provenance_path, "wx");
    if (!provenance) {
        fprintf(stderr, "dataset_builder: cannot create %s: %s\n",
                options.provenance_path, strerror(errno));
        goto done;
    }

    rc = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "dataset_builder: query prepare failed: %s\n",
                sqlite3_errmsg(database));
        goto done;
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        sqlite3_int64 corpus_id = sqlite3_column_int64(statement, 0);
        const char *category = (const char *)sqlite3_column_text(statement, 1);
        const char *source = (const char *)sqlite3_column_text(statement, 2);
        const char *text = (const char *)sqlite3_column_text(statement, 3);
        const char *verification =
            (const char *)sqlite3_column_text(statement, 4);
        const char *source_record_id =
            (const char *)sqlite3_column_text(statement, 5);
        size_t total;
        size_t offset = 0U;
        unsigned int chunk_number = 0U;

        if (options.limit && rows >= options.limit) break;
        ++rows;
        if (!text || !*text) {
            ++failed;
            continue;
        }
        total = strlen(text);

        while (offset < total) {
            size_t length = utf8_chunk_length(text + offset, total - offset);
            char *instruction = NULL;
            char *answer = NULL;

            if (!length) {
                ++failed;
                break;
            }
            ++chunk_number;
            fprintf(stderr,
                    "dataset_builder: corpus=%lld chunk=%u generating...\n",
                    (long long)corpus_id, chunk_number);

            if (!ollama_example(text + offset, length,
                                &instruction, &answer)) {
                fprintf(stderr,
                        "dataset_builder: corpus=%lld chunk=%u generation failed\n",
                        (long long)corpus_id, chunk_number);
                ++failed;
                free(instruction);
                free(answer);
                offset += length;
                continue;
            }

            if (!write_training(output, instruction, answer) ||
                !write_provenance(provenance, emitted + 1U, corpus_id,
                                  source_record_id, source, category,
                                  verification, chunk_number)) {
                fprintf(stderr, "dataset_builder: output write failed\n");
                free(instruction);
                free(answer);
                goto done;
            }
            ++emitted;
            free(instruction);
            free(answer);
            offset += length;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        fprintf(stderr, "dataset_builder: query failed: %s\n",
                sqlite3_errmsg(database));
        goto done;
    }

    if (fflush(output) != 0 || fflush(provenance) != 0) {
        fprintf(stderr, "dataset_builder: output flush failed\n");
        goto done;
    }

    fprintf(stderr,
            "dataset_builder: lessons=%lu examples=%lu generation_failures=%lu\n",
            rows, emitted, failed);
    exit_code = failed ? 3 : 0;

done:
    if (statement) sqlite3_finalize(statement);
    if (provenance) fclose(provenance);
    if (output) fclose(output);
    if (database) sqlite3_close(database);
    curl_global_cleanup();
    return exit_code;
}
