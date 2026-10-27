#include <string.h>
#include "research_internal.h"
int research_selftests(unsigned int *tests,unsigned int *passed,
 int *negative_executed,int *negative_passed)
{
 unsigned int n=0,p=0;int negative_count=0,negative_ok=0;
 char topic[64],term[512],out[512];
#define CHECK(x) do{++n;if(x)++p;}while(0)
#define NEG(x) do{++negative_count;++n;if(x){++negative_ok;++p;}}while(0)
 CHECK(research_topic_valid("security"));
 CHECK(research_topic_valid("autonomous-systems"));
 CHECK(research_topic_valid("software-engineering"));
 CHECK(research_topic_valid("space-systems"));
 CHECK(research_parse("security policy",topic,sizeof topic,term,sizeof term));
 CHECK(!strcmp(topic,"security"));
 CHECK(!strcmp(term,"policy"));
 CHECK(research_parse("space-systems radiation",topic,sizeof topic,term,sizeof term));
 CHECK(research_search("/does/not/exist","security","radiation",out,sizeof out));
 CHECK(strstr(out,"UNKNOWN")!=NULL);
 NEG(!research_topic_valid("st1"));
 NEG(!research_topic_valid(""));
 NEG(!research_topic_valid(NULL));
 NEG(!research_parse("security",topic,sizeof topic,term,sizeof term));
 NEG(!research_parse("security ",topic,sizeof topic,term,sizeof term));
 NEG(!research_parse("other thing",topic,sizeof topic,term,sizeof term));
 NEG(!research_parse(NULL,topic,sizeof topic,term,sizeof term));
 NEG(!research_search(NULL,"security","x",out,sizeof out));
#undef CHECK
#undef NEG
 if(tests)*tests=n;
 if(passed)*passed=p;
 if(negative_executed)*negative_executed=negative_count>0;
 if(negative_passed)*negative_passed=negative_count==negative_ok;
 return n>=10&&n==p&&negative_count>0&&negative_count==negative_ok;
}
