#include <stdio.h>
#include <string.h>
#include "research_internal.h"

#define CHECK(condition) do { \
    ++tests; \
    if (condition) ++passed; \
    else fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
} while (0)

int main(void)
{
    unsigned int tests = 0, passed = 0;
    unsigned int qualified_tests = 0, qualified_passed = 0;
    int negative_executed = 0, negative_passed = 0;
    int qualified;
    char topic[64], term[512], output[512];

    CHECK(research_topic_valid("security"));
    CHECK(research_topic_valid("autonomous-systems"));
    CHECK(research_topic_valid("software-engineering"));
    CHECK(research_topic_valid("space-systems"));
    CHECK(research_parse("security firmware", topic, sizeof topic, term, sizeof term));
    CHECK(!strcmp(topic, "security"));
    CHECK(!strcmp(term, "firmware"));
    CHECK(research_parse("space-systems radiation", topic, sizeof topic, term, sizeof term));
    CHECK(research_search("/directory/which/does/not/exist", "security", "test", output, sizeof output));
    CHECK(strstr(output, "UNKNOWN") != NULL);
    CHECK(!research_topic_valid("mission"));
    CHECK(!research_topic_valid(NULL));
    CHECK(!research_parse("invalid something", topic, sizeof topic, term, sizeof term));
    CHECK(!research_parse("security", topic, sizeof topic, term, sizeof term));
    CHECK(!research_parse("security ", topic, sizeof topic, term, sizeof term));
    CHECK(!research_parse(NULL, topic, sizeof topic, term, sizeof term));
    CHECK(!research_search(NULL, "security", "test", output, sizeof output));
    CHECK(!research_search("/tmp", "bad", "test", output, sizeof output));

    /* Qualification owns its own counters. Never overwrite the runner's tally. */
    qualified = research_selftests(&qualified_tests, &qualified_passed,
                                   &negative_executed, &negative_passed);
    CHECK(qualified);
    CHECK(negative_executed && negative_passed);
    CHECK(qualified_tests >= 10);
    CHECK(qualified_tests == qualified_passed);

    printf("Research tests: %u/%u PASS\n", passed, tests);
    printf("Research module qualification: %u/%u PASS; negative tests: %s\n",
           qualified_passed, qualified_tests,
           (negative_executed && negative_passed) ? "PASS" : "FAIL");
    return tests == passed ? 0 : 1;
}
