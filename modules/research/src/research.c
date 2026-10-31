#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "research.h"

/* [AI:GPT-6 | 2026-10-31 UTC]
 * Research owns web acquisition only. It has no topic rules or LLM dependency.
 * Search provider: DuckDuckGo's public HTML search endpoint (no paid API).
 * HTML is untrusted; all returned material is treated as external evidence.
 */
#define HTTP_LIMIT (512U * 1024U)
#define REPLY_LIMIT 439U
#define MAX_RESULTS 5U
#define MAX_FETCHES 3U

typedef struct { char *data; size_t length; } buffer_t;
typedef struct { char title[180], url[1000], snippet[600]; } hit_t;
typedef struct { char text[960], url[1000]; int score; time_t when; } passage_t;
static const stnlabz_module_host_t *owner;
static int registered_answer, registered_search, registered_command;

static size_t received(char *ptr, size_t size, size_t count, void *ctx)
{
    buffer_t *b = ctx;
    size_t n;
    char *p;
    if (!b || !size || count > SIZE_MAX / size) return 0;
    n = size * count;
    if (n > HTTP_LIMIT - b->length) return 0;
    p = realloc(b->data, b->length + n + 1);
    if (!p) return 0;
    b->data = p;
    memcpy(b->data + b->length, ptr, n);
    b->length += n;
    b->data[b->length] = 0;
    return n;
}

static int safe_url(const char *url)
{
    const char *p;
    if (!url || strncmp(url, "https://", 8)) return 0;
    p = url + 8;
    if (!isalnum((unsigned char)*p)) return 0;
    if (!strncmp(p, "localhost", 9) && (p[9] == 0 || p[9] == "/"[0] || p[9] == ':')) return 0;
    if (!strncmp(p, "127.", 4) || !strncmp(p, "10.", 3) ||
        !strncmp(p, "192.168.", 8) || !strncmp(p, "169.254.", 8) ||
        !strncmp(p, "172.16.", 7) || !strncmp(p, "172.17.", 7) ||
        !strncmp(p, "172.18.", 7) || !strncmp(p, "172.19.", 7) ||
        !strncmp(p, "172.20.", 7) || !strncmp(p, "172.21.", 7) ||
        !strncmp(p, "172.22.", 7) || !strncmp(p, "172.23.", 7) ||
        !strncmp(p, "172.24.", 7) || !strncmp(p, "172.25.", 7) ||
        !strncmp(p, "172.26.", 7) || !strncmp(p, "172.27.", 7) ||
        !strncmp(p, "172.28.", 7) || !strncmp(p, "172.29.", 7) ||
        !strncmp(p, "172.30.", 7) || !strncmp(p, "172.31.", 7)) return 0;
    while (*p && *p != '/') {
        unsigned char c = (unsigned char)*p++;
        if (!(isalnum(c) || c == '.' || c == '-' || c == ':')) return 0;
    }
    return 1;
}

static int http_get(const char *url, buffer_t *body)
{
    CURL *curl;
    CURLcode rc;
    long code = 0;
    if (!body || !safe_url(url)) return 0;
    curl = curl_easy_init();
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; DigitResearch/1.0)");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, received);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
    rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK && code == 200L && body->data && body->length;
}

static void normalize(char *s)
{
    size_t r = 0, w = 0;
    int space = 1;
    while (s[r]) {
        unsigned char c = (unsigned char)s[r++];
        if (isspace(c)) {
            if (!space) s[w++] = ' ';
            space = 1;
        } else {
            s[w++] = (char)c;
            space = 0;
        }
    }
    if (w && s[w - 1] == ' ') --w;
    s[w] = 0;
}

/* Small bounded HTML text extractor: no additional runtime dependencies. */
static int attr_value(const char *start, const char *end, const char *name,
                      char *out, size_t cap)
{
    const char *p = start;
    size_t key = strlen(name);
    if (!out || !cap) return 0;
    out[0] = 0;
    while (p < end) {
        const char *begin, *value;
        char quote = 0;
        size_t len;
        while (p < end && (isspace((unsigned char)*p) || *p == '/' )) ++p;
        begin = p;
        while (p < end && (isalnum((unsigned char)*p) || *p == '-' || *p == '_')) ++p;
        if (p == begin) { ++p; continue; }
        len = (size_t)(p - begin);
        while (p < end && isspace((unsigned char)*p)) ++p;
        if (p == end || *p != '=') continue;
        ++p;
        while (p < end && isspace((unsigned char)*p)) ++p;
        if (p < end && (*p == '"' || *p == '\'')) quote = *p++;
        value = p;
        while (p < end && ((quote && *p != quote) ||
                             (!quote && !isspace((unsigned char)*p) && *p != '>'))) ++p;
        if (len == key && !strncasecmp(begin, name, key)) {
            size_t n = (size_t)(p - value);
            if (!n || n >= cap) return 0;
            memcpy(out, value, n);
            out[n] = 0;
            return 1;
        }
        if (quote && p < end) ++p;
    }
    return 0;
}
static unsigned utf8_add(char *out, size_t *used, size_t cap, unsigned codepoint)
{
    if (codepoint < 128 && *used + 1 < cap) { out[(*used)++] = (char)codepoint; return 1; }
    if (codepoint < 2048 && *used + 2 < cap) {
        out[(*used)++] = (char)(192 | (codepoint >> 6));
        out[(*used)++] = (char)(128 | (codepoint & 63));
        return 1;
    }
    if (codepoint < 65536 && *used + 3 < cap) {
        out[(*used)++] = (char)(224 | (codepoint >> 12));
        out[(*used)++] = (char)(128 | ((codepoint >> 6) & 63));
        out[(*used)++] = (char)(128 | (codepoint & 63));
        return 1;
    }
    return 0;
}
static void html_text(const char *start, const char *end, char *out, size_t cap)
{
    const char *p = start;
    size_t used = 0;
    int in_tag = 0;
    if (!out || !cap) return;
    while (p < end && used + 4 < cap) {
        if (*p == '<') { in_tag = 1; if (used && out[used-1] != ' ') out[used++] = ' '; ++p; continue; }
        if (*p == '>') { in_tag = 0; ++p; continue; }
        if (in_tag) { ++p; continue; }
        if (*p == '&') {
            const char *semi = memchr(p, ';', (size_t)(end-p) < 12 ? (size_t)(end-p) : 12);
            if (semi) {
                unsigned code = 0;
                size_t n = (size_t)(semi - p);
                if (n == 4 && !strncmp(p, "&amp", 4)) code = '&';
                else if (n == 3 && !strncmp(p, "&lt", 3)) code = '<';
                else if (n == 3 && !strncmp(p, "&gt", 3)) code = '>';
                else if (n == 5 && !strncmp(p, "&quot", 5)) code = '"';
                else if (n == 5 && !strncmp(p, "&nbsp", 5)) code = ' ';
                else if (n == 5 && !strncmp(p, "&#39", 5)) code = '\'';
                else if (p[1] == '#') {
                    char number[12] = {0};
                    char *tail;
                    unsigned long value;
                    if (n > 2 && n < sizeof number) {
                        memcpy(number, p+2, n-2);
                        value = strtoul(number, &tail, (number[0] == 'x' || number[0] == 'X') ? 16 : 10);
                        if (tail != number && !*tail && value > 0 && value < 0xD800UL)
                            code = (unsigned)value;
                    }
                }
                if (code && utf8_add(out, &used, cap, code)) { p = semi + 1; continue; }
            }
        }
        out[used++] = isspace((unsigned char)*p) ? ' ' : *p;
        ++p;
    }
    out[used] = 0;
    normalize(out);
}
static const char *tag_end(const char *start)
{
    char quote = 0;
    const char *p = start;
    while (*p) {
        if (quote) { if (*p == quote) quote = 0; }
        else if (*p == '"' || *p == '\'') quote = *p;
        else if (*p == '>') return p;
        ++p;
    }
    return NULL;
}
static int absolute_url(const char *raw, char *out, size_t capacity)
{
    const char *encoded = strstr(raw, "uddg=");
    size_t n = 0;
    if (encoded) {
        const char *p = encoded + 5;
        while (*p && *p != '&' && n + 1 < capacity) {
            unsigned int byte;
            if (*p == '%' && isxdigit((unsigned char)p[1]) &&
                isxdigit((unsigned char)p[2]) && sscanf(p+1, "%2x", &byte) == 1) {
                out[n++] = (char)byte; p += 3;
            } else out[n++] = *p++;
        }
        out[n] = 0;
    } else snprintf(out, capacity, "%s", raw);
    return safe_url(out);
}
static size_t parse_results(const char *html, hit_t *hits, size_t capacity)
{
    const char *p = html;
    size_t count = 0;
    if (!p || !hits) return 0;
    while ((p = strstr(p, "<a")) != NULL && count < capacity) {
        const char *end = tag_end(p), *close;
        char cls[256], href[1600];
        if (!end) break;
        if (!attr_value(p+2, end, "class", cls, sizeof cls) ||
            (!strstr(cls, "result__a") && !strstr(cls, "result-link")) ||
            !attr_value(p+2, end, "href", href, sizeof href) ||
            !absolute_url(href, hits[count].url, sizeof hits[count].url)) { p=end+1; continue; }
        close = strstr(end+1, "</a>");
        if (!close) { p=end+1; continue; }
        html_text(end+1, close, hits[count].title, sizeof hits[count].title);
        if (!hits[count].title[0]) { p=close+4; continue; }
        hits[count].snippet[0] = 0;
        {
            const char *next = strstr(close+4, "result__a");
            const char *s = strstr(close+4, "result__snippet");
            if (s && (!next || s < next)) {
                const char *start = strchr(s, '>');
                const char *finish = start ? strchr(start+1, '<') : NULL;
                if (start && finish && (size_t)(finish - start) < 2500)
                    html_text(start+1, finish, hits[count].snippet, sizeof hits[count].snippet);
            }
        }
        ++count;
        p=close+4;
    }
    return count;
}

static int word_match(const char *hay, const char *word)
{
    size_t n = strlen(word);
    const char *p = hay;
    if (n < 3) return 0;
    while (*p) {
        size_t i;
        for (i = 0; i < n && p[i] &&
             tolower((unsigned char)p[i]) == tolower((unsigned char)word[i]); ++i) {}
        if (i == n && (p == hay || !isalnum((unsigned char)p[-1])) &&
            !isalnum((unsigned char)p[n])) return 1;
        ++p;
    }
    return 0;
}

static int relevance(const char *question, const char *text)
{
    char word[60];
    size_t i = 0, k;
    int score = 0;
    while (question[i]) {
        while (question[i] && !isalnum((unsigned char)question[i])) ++i;
        k = 0;
        while (question[i] && isalnum((unsigned char)question[i])) {
            if (k + 1 < sizeof word) word[k++] = (char)tolower((unsigned char)question[i]);
            ++i;
        }
        word[k] = 0;
        if (k >= 3 && !word_match("the and for are was were when what where how who again next playing research find information about please today", word)
            && word_match(text, word)) score += 3;
    }
    return score;
}

static time_t find_date(const char *text, time_t now)
{
    static const char *formats[] = {"%B %d, %Y", "%b %d, %Y", "%Y-%m-%d", "%m/%d/%Y"};
    size_t i;
    time_t earliest = 0;
    for (i = 0; text[i]; ++i) {
        size_t f;
        for (f = 0; f < sizeof formats / sizeof formats[0]; ++f) {
            struct tm date = {0};
            char *end;
            time_t t;
            date.tm_isdst = -1;
            end = strptime(text + i, formats[f], &date);
            if (!end || (end == text + i) || date.tm_year < 126 || date.tm_year > 140) continue;
            t = timegm(&date);
            if (t >= now - 86400 && (!earliest || t < earliest)) earliest = t;
        }
    }
    return earliest;
}

static int upcoming(const char *question)
{
    return (word_match(question, "when") &&
           (word_match(question, "again") || word_match(question, "next") ||
            word_match(question, "upcoming")));
}

static void best_passage(const char *html, const char *question, const char *url,
                         const char *title, time_t now, passage_t *best)
{
    const char *p = html;
    if (!p || !best) return;
    while ((p = strchr(p, '<')) != NULL) {
        const char *end, *close;
        char tag[8] = {0}, closing[16];
        size_t k = 0;
        passage_t candidate = {0};
        size_t len;
        ++p;
        if (*p == '/') continue;
        while (k + 1 < sizeof tag && isalpha((unsigned char)p[k])) {
            tag[k] = (char)tolower((unsigned char)p[k]); ++k;
        }
        if (strcmp(tag,"p") && strcmp(tag,"li") && strcmp(tag,"tr") &&
            strcmp(tag,"h1") && strcmp(tag,"h2") && strcmp(tag,"h3")) continue;
        if (p[k] != '>' && p[k] != ' ' && p[k] != '\t' && p[k] != '\n') continue;
        end = tag_end(p);
        if (!end) break;
        snprintf(closing, sizeof closing, "</%s>", tag);
        close = strcasestr(end+1, closing);
        if (!close) { p = end+1; continue; }
        html_text(end+1, close, candidate.text, sizeof candidate.text);
        p = close + strlen(closing);
        len = strlen(candidate.text);
        if (len < 30 || len >= sizeof candidate.text - 1) continue;
        candidate.score = relevance(question, candidate.text);
        candidate.when = find_date(candidate.text, now);
        if (!candidate.score && candidate.when && relevance(question, title) > 0)
            candidate.score = 1;
        if (!candidate.score) continue;
        if (upcoming(question)) {
            if (!candidate.when) continue;
            if (best->when && candidate.when > best->when) continue;
            if (best->when == candidate.when && candidate.score <= best->score) continue;
        } else if (candidate.score <= best->score) continue;
        snprintf(candidate.url, sizeof candidate.url, "%s", url);
        *best = candidate;
    }
}

static int build_reply(const char *question, const hit_t *hits, size_t count,
                       const passage_t *passage, char *out, size_t cap)
{
    const char *text = NULL;
    char chosen[960];
    size_t i;
    if (!out || !cap) return 0;
    out[0] = 0;
    if (passage && passage->text[0]) text = passage->text;
    if (!text && !upcoming(question)) for (i = 0; i < count; ++i) {
        if (hits[i].snippet[0] && relevance(question, hits[i].snippet) > 0) {
            text = hits[i].snippet;
            break;
        }
    }
    if (!text) {
        snprintf(out, cap, "I couldn't verify the requested information from retrieved sources.");
        return 0;
    }
    snprintf(chosen, sizeof chosen, "%s", text);
    normalize(chosen);
    /* Do not slice a source statement and then claim its truncated facts are complete. */
    if (strlen(chosen) >= cap) {
        size_t boundary = cap - 2;
        while (boundary > 50 && chosen[boundary] != '.') --boundary;
        if (boundary <= 50) {
            snprintf(out, cap, "Retrieved relevant content, but no concise verified answer was available.");
            return 0;
        }
        chosen[boundary + 1] = 0;
    }
    snprintf(out, cap, "%s", chosen);
    return 1;
}

static int research(const char *question, research_response_t *result, int full)
{
    CURL *curl;
    char *encoded, search_url[1600];
    buffer_t search = {0};
    hit_t hits[MAX_RESULTS] = {0};
    passage_t best = {0};
    size_t count, i, downloaded = 0;
    time_t now = time(NULL);
    struct tm utc;
    if (!question || !*question || !result) return 0;
    memset(result, 0, sizeof *result);
    result->status = RESEARCH_UNAVAILABLE;
    if (gmtime_r(&now, &utc))
        strftime(result->retrieved_utc, sizeof result->retrieved_utc, "%Y-%m-%d %H:%M:%S UTC", &utc);
    curl = curl_easy_init();
    if (!curl) return 0;
    encoded = curl_easy_escape(curl, question, 0);
    if (!encoded || snprintf(search_url, sizeof search_url,
        "https://html.duckduckgo.com/html/?q=%s", encoded) >= (int)sizeof search_url) {
        if (encoded) curl_free(encoded);
        curl_easy_cleanup(curl);
        return 0;
    }
    curl_free(encoded);
    curl_easy_cleanup(curl);
    if (!http_get(search_url, &search)) goto done;
    count = parse_results(search.data, hits, MAX_RESULTS);
    if (!count) goto done;
    result->sources = (unsigned)count;
    if (full) {
        size_t used = 0;
        for (i = 0; i < count; ++i) {
            int n = snprintf(result->evidence + used, sizeof result->evidence - used,
                "%s | %s | %s\n", hits[i].title, hits[i].url, hits[i].snippet);
            if (n < 0 || (size_t)n >= sizeof result->evidence - used) break;
            used += (size_t)n;
        }
        result->status = RESEARCH_EVIDENCE;
        goto done;
    }
    for (i = 0; i < count && downloaded < MAX_FETCHES; ++i) {
        buffer_t page = {0};
        if (http_get(hits[i].url, &page)) {
            ++downloaded;
            best_passage(page.data, question, hits[i].url, hits[i].title, now, &best);
        }
        free(page.data);
    }
    if (build_reply(question, hits, count, &best, result->evidence, REPLY_LIMIT))
        result->status = RESEARCH_EVIDENCE;
    else result->status = RESEARCH_UNAVAILABLE;
done:
    if (result->status == RESEARCH_UNAVAILABLE && !result->evidence[0])
        snprintf(result->evidence, sizeof result->evidence, "Research unavailable: no usable information retrieved.");
    free(search.data);
    return result->status == RESEARCH_EVIDENCE;
}

static stnlabz_module_result_t run_service(const void *request, size_t size,
    void *response, size_t capacity, size_t *used, void *ctx, int full)
{
    const research_request_t *in = request;
    research_response_t *out = response;
    (void)ctx;
    if (used) *used = 0;
    if (!in || size != sizeof *in || !out || capacity < sizeof *out ||
        !memchr(in->query, 0, sizeof in->query) || !in->query[0])
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    (void)research(in->query, out, full);
    if (used) *used = sizeof *out;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t answer_service(const void *q, size_t n, void *r,
    size_t cap, size_t *used, void *ctx)
{ return run_service(q, n, r, cap, used, ctx, 0); }
static stnlabz_module_result_t search_service(const void *q, size_t n, void *r,
    size_t cap, size_t *used, void *ctx)
{ return run_service(q, n, r, cap, used, ctx, 1); }

static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
    stnlabz_module_command_reply_fn reply, void *context, void *unused)
{
    research_response_t *result;
    (void)unused;
    if (!cmd || !reply || !memchr(cmd->arguments, 0, sizeof cmd->arguments))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    result = calloc(1, sizeof *result);
    if (!result) return STNLABZ_MODULE_ERR_START_FAILED;
    (void)research(cmd->arguments, result, 0);
    int ok = reply(context, result->evidence);
    free(result);
    return ok ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_INVALID_STATE;
}

static int selftest(unsigned *executed, unsigned *passed, int *negative)
{
    unsigned e = 0, p = 0;
    hit_t hits[2] = {0};
    char answer[440];
    passage_t chosen = {0};
    #define CHECK(x) do { ++e; if (x) ++p; } while (0)
    CHECK(safe_url("https://example.org/page"));
    CHECK(!safe_url("http://example.org"));
    CHECK(!safe_url("file:///etc/passwd"));
    CHECK(!safe_url("https://127.0.0.1@evil.com"));
    CHECK(absolute_url("//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.org%2Fa",hits[0].url,sizeof hits[0].url));
    CHECK(!strcmp(hits[0].url,"https://example.org/a"));
    CHECK(parse_results("<html><a class='result__a' href='https://example.org/go'>Go docs</a></html>",hits,2)==1);
    CHECK(!strcmp(hits[0].title,"Go docs"));
    CHECK(relevance("research Go programming language","Go programming language documentation")>0);
    CHECK(relevance("information about PHP 8.5", "PHP 8.5 release notes") > 0);
    CHECK(relevance("Falcon 9 booster recovery", "Falcon 9 booster landing details") > 0);
    CHECK(upcoming("When is the next conference?"));
    CHECK(!upcoming("Research PHP 8.5"));
    CHECK(find_date("Nov 1, 2026",0)>0);
    CHECK(find_date("Jan 1, 2020",time(NULL))==0);
    CHECK(!build_reply("when is the next event",hits,1,&chosen,answer,sizeof answer));
    CHECK(strstr(answer,"couldn't verify")!=NULL);
    snprintf(chosen.text,sizeof chosen.text,"The event begins November 1, 2026 at 4:25 PM.");
    CHECK(build_reply("when is next event",hits,1,&chosen,answer,sizeof answer));
    CHECK(strstr(answer,"4:25 PM")!=NULL);
    CHECK(!safe_url("https://localhost/path"));
    {
        passage_t earlier = {0};
        const char *document = "<html><body><p>The conference starts December 8, 2026 at 8 AM.</p>"
          "<p>The conference starts November 3, 2026 at 9 AM.</p></body></html>";
        best_passage(document, "When is the next conference?", "https://example.org/events",
                     "Conference schedule", timegm(&(struct tm){.tm_year=126,.tm_mon=9,.tm_mday=31}), &earlier);
        CHECK(strstr(earlier.text, "November 3") != NULL);
    }
    #undef CHECK
    if (executed) *executed = e;
    if (passed) *passed = p;
    if (negative) *negative = 1;
    return e == p && e >= 10;
}

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    unsigned executed, passed;
    int negative;
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof *q);
    int ok = selftest(&executed, &passed, &negative);
    q->tests_executed = executed;
    q->tests_passed = passed;
    q->tests_failed = executed - passed;
    q->negative_test_executed = negative;
    q->negative_test_passed = negative && executed == passed;
    return ok ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    if (!host || !host->register_command || !host->unregister_command ||
        !host->register_service || !host->unregister_service)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return STNLABZ_MODULE_ERR_START_FAILED;
    owner = host;
    if (!host->register_service(DIGIT_RESEARCH_ANSWER_SERVICE, answer_service, NULL)) goto fail;
    registered_answer = 1;
    if (!host->register_service(DIGIT_RESEARCH_SEARCH_SERVICE, search_service, NULL)) goto fail;
    registered_search = 1;
    if (!host->register_command("research", command, NULL)) goto fail;
    registered_command = 1;
    return STNLABZ_MODULE_OK;
fail:
    if (registered_search) (void)host->unregister_service(DIGIT_RESEARCH_SEARCH_SERVICE, NULL);
    if (registered_answer) (void)host->unregister_service(DIGIT_RESEARCH_ANSWER_SERVICE, NULL);
    registered_search = registered_answer = 0;
    owner = NULL;
    curl_global_cleanup();
    return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void)
{
    if (registered_command && owner && !owner->unregister_command("research", NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    registered_command = 0;
    if (registered_search && owner && !owner->unregister_service(DIGIT_RESEARCH_SEARCH_SERVICE, NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    registered_search = 0;
    if (registered_answer && owner && !owner->unregister_service(DIGIT_RESEARCH_ANSWER_SERVICE, NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    registered_answer = 0;
    owner = NULL;
    curl_global_cleanup();
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor = {
    "research", "Digit Research", 1, 0, 0,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{ return &descriptor; }
/* [End AI:GPT-6] */
