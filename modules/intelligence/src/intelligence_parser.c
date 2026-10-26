#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "intelligence_internal.h"

static const char *skip_space(const char *p, const char *end)
{
    while (p < end && isspace((unsigned char)*p)) ++p;
    return p;
}

static int parse_string(const char *p, const char *end,
                        char *output, size_t output_size,
                        const char **next)
{
    size_t used = 0U;

    if (!p || p >= end || *p != '"' || !output || !output_size || !next)
        return 0;
    ++p;
    while (p < end) {
        unsigned char c = (unsigned char)*p++;
        if (c == '"') {
            output[used] = '\0';
            *next = p;
            return 1;
        }
        if (c == '\\') {
            if (p >= end) return 0;
            c = (unsigned char)*p++;
            if (c == 'u') return 0;
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
        }
        if (c < 0x20U || used + 1U >= output_size) return 0;
        output[used++] = (char)c;
    }
    return 0;
}

static int string_bounds(const char *p, const char *end,
                         const char **content_start,
                         const char **content_end,
                         const char **next)
{
    int escaped = 0;
    const char *start;

    if (!p || p >= end || *p != '"' || !content_start || !content_end || !next)
        return 0;
    start = ++p;
    while (p < end) {
        if (escaped) {
            escaped = 0;
            ++p;
            continue;
        }
        if (*p == '\\') {
            escaped = 1;
            ++p;
            continue;
        }
        if (*p == '"') {
            *content_start = start;
            *content_end = p;
            *next = p + 1;
            return 1;
        }
        ++p;
    }
    return 0;
}

static int top_level_string_field(const char *object, size_t object_size,
                                  const char *wanted,
                                  char *output, size_t output_size)
{
    const char *p = object;
    const char *end = object + object_size;
    size_t wanted_length = strlen(wanted);
    int depth = 0;

    while (p < end) {
        if (*p == '"') {
            const char *content_start;
            const char *content_end;
            const char *after;
            const char *value;
            size_t raw_length;

            if (!string_bounds(p, end, &content_start, &content_end, &after))
                return 0;

            if (depth == 1) {
                value = skip_space(after, end);
                if (value < end && *value == ':') {
                    raw_length = (size_t)(content_end - content_start);
                    if (raw_length == wanted_length &&
                        !memchr(content_start, '\\', raw_length) &&
                        !memcmp(content_start, wanted, wanted_length)) {
                        value = skip_space(value + 1, end);
                        if (value >= end || *value != '"') return 0;
                        return parse_string(value, end, output, output_size, &after);
                    }
                }
            }

            p = after;
            continue;
        }

        if (*p == '{') ++depth;
        else if (*p == '}') --depth;
        ++p;
    }
    return 0;
}

static int parse_record(const char *object, size_t object_size,
                        rictus_intelligence_record_t *record)
{
    if (!object || !record) return 0;
    memset(record, 0, sizeof *record);

    if (!top_level_string_field(object, object_size, "id",
                                record->id, sizeof record->id) ||
        !top_level_string_field(object, object_size, "type",
                                record->type, sizeof record->type) ||
        !top_level_string_field(object, object_size, "severity",
                                record->severity, sizeof record->severity) ||
        !top_level_string_field(object, object_size, "relevance",
                                record->relevance, sizeof record->relevance))
        return 0;

    (void)top_level_string_field(object, object_size, "created_at",
                                 record->created_at, sizeof record->created_at);
    return 1;
}

int rictus_intelligence_relevant(const char *relevance)
{
    if (!relevance) return 0;
    return !strcasecmp(relevance, "MEDIUM") ||
           !strcasecmp(relevance, "HIGH");
}

int rictus_intelligence_extract_records(
    const char *json,
    rictus_intelligence_record_t *records,
    size_t capacity,
    size_t *count)
{
    const char *p;
    const char *array;
    const char *object_start = NULL;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    size_t found = 0U;

    if (!json || !records || !capacity || !count) return 0;
    *count = 0U;

    p = strstr(json, "\"threats\"");
    if (!p) return 0;
    array = strchr(p, '[');
    if (!array) return 0;

    for (p = array + 1; *p; ++p) {
        char c = *p;

        if (in_string) {
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (c == '\\') {
                escaped = 1;
                continue;
            }
            if (c == '"') in_string = 0;
            continue;
        }

        if (c == '"') {
            in_string = 1;
            continue;
        }

        if (c == '{') {
            if (depth == 0) object_start = p;
            ++depth;
            continue;
        }

        if (c == '}') {
            if (depth <= 0) return 0;
            --depth;
            if (depth == 0 && object_start) {
                if (found < capacity) {
                    if (!parse_record(object_start, (size_t)(p - object_start + 1),
                                      &records[found]))
                        return 0;
                    ++found;
                }
                object_start = NULL;
            }
            continue;
        }

        if (c == ']' && depth == 0) {
            *count = found;
            return 1;
        }
    }

    return 0;
}

static int expect(int condition, unsigned int *executed, unsigned int *passed)
{
    ++*executed;
    if (condition) {
        ++*passed;
        return 1;
    }
    return 0;
}

int rictus_intelligence_run_selftests(unsigned int *executed,
                                      unsigned int *passed,
                                      int *negative_executed,
                                      int *negative_passed)
{
    static const char sample[] =
        "{\"threats\":["
        "{\"id\":\"t_2\",\"ip\":\"10.0.0.2\",\"type\":\"malware\","
        "\"severity\":\"HIGH\",\"relevance\":\"HIGH\","
        "\"details\":{\"domain\":\"example.test\"},"
        "\"created_at\":\"2026-10-26 01:00:00\"},"
        "{\"id\":\"t_1\",\"ip\":\"10.0.0.1\",\"type\":\"pattern_match\","
        "\"severity\":\"MEDIUM\",\"relevance\":\"MEDIUM\","
        "\"details\":{\"matched_pattern\":\"xmlrpc\"},"
        "\"created_at\":\"2026-10-26 00:59:00\"}"
        "]}";
    rictus_intelligence_record_t records[4];
    size_t count = 0U;
    unsigned int e = 0U;
    unsigned int p = 0U;
    int neg_e = 0;
    int neg_p = 0;

    expect(rictus_intelligence_relevant("MEDIUM"), &e, &p);
    expect(rictus_intelligence_relevant("HIGH"), &e, &p);
    expect(rictus_intelligence_relevant("medium"), &e, &p);
    expect(!rictus_intelligence_relevant("LOW"), &e, &p);
    expect(!rictus_intelligence_relevant(NULL), &e, &p);
    expect(rictus_intelligence_extract_records(sample, records, 4U, &count),
           &e, &p);
    expect(count == 2U, &e, &p);
    expect(!strcmp(records[0].id, "t_2") &&
           !strcmp(records[0].type, "malware"), &e, &p);
    expect(!strcmp(records[0].severity, "HIGH") &&
           !strcmp(records[0].relevance, "HIGH"), &e, &p);
    expect(!strcmp(records[1].id, "t_1") &&
           !strcmp(records[1].relevance, "MEDIUM"), &e, &p);
    expect(!strcmp(records[1].created_at, "2026-10-26 00:59:00"), &e, &p);
    expect(!rictus_intelligence_extract_records(
        "{\"wrong\":[]}", records, 4U, &count), &e, &p);

    neg_e = 1;
    neg_p = !rictus_intelligence_extract_records(NULL, records, 4U, &count);

    if (executed) *executed = e;
    if (passed) *passed = p;
    if (negative_executed) *negative_executed = neg_e;
    if (negative_passed) *negative_passed = neg_p;
    return e >= 10U && e == p && neg_e && neg_p;
}
