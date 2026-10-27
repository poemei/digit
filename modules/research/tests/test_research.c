#include <stdio.h>
#include <string.h>
#include "research_internal.h"
#define CHECK(condition) do{tests++;if(condition)passed++;else fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition);}while(0)
int main(void)
{
 unsigned int tests=0,passed=0;int negative=0,negative_passed=0;
 char topic[64],term[512],output[512];
 CHECK(research_topic_valid("security"));
 CHECK(research_topic_valid("autonomous-systems"));
 CHECK(research_topic_valid("software-engineering"));
 CHECK(research_topic_valid("space-systems"));
 CHECK(research_parse("security firmware",topic,sizeof topic,term,sizeof term));
 CHECK(!strcmp(topic,"security"));
 CHECK(!strcmp(term,"firmware"));
 CHECK(research_parse("space-systems radiation",topic,sizeof topic,term,sizeof term));
 CHECK(research_search("/directory/which/does/not/exist","security","test",output,sizeof output));
 CHECK(strstr(output,"UNKNOWN")!=NULL);
 CHECK(!research_topic_valid("mission"));
 CHECK(!research_topic_valid(NULL));
 CHECK(!research_parse("invalid something",topic,sizeof topic,term,sizeof term));
 CHECK(!research_parse("security",topic,sizeof topic,term,sizeof term));
 CHECK(!research_parse("security ",topic,sizeof topic,term,sizeof term));
 CHECK(!research_parse(NULL,topic,sizeof topic,term,sizeof term));
 CHECK(!research_search(NULL,"security","test",output,sizeof output));
 CHECK(!research_search("/tmp","bad","test",output,sizeof output));
 CHECK(research_selftests(&tests,&passed,&negative,&negative_passed));
 CHECK(negative&&negative_passed);
 printf("Research tests: %u/%u PASS\n",passed,tests);
 return tests==passed?0:1;
}
