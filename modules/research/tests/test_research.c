#define RESEARCH_TEST
#include "../src/research.c"
int main(void)
{
 research_response_t r={0};
 const char *fixture="{\"web\":{\"results\":["
 "{\"title\":\"Launch schedule\",\"url\":\"https://example.org/launch\",\"description\":\"Launch updates\"},"
 "{\"title\":\"Bad link\",\"url\":\"http://127.0.0.1/private\",\"description\":\"Private\"}]}}";
 int failures=0,checks=0;
 #define CHECK(x) do {++checks;if(!(x)){++failures;fprintf(stderr,"FAIL line %d\n",__LINE__);}}while(0)
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(!forbidden("when is the next SpaceX launch?"));
 CHECK(forbidden(""));
 CHECK(extract_results(fixture,strlen(fixture),&r)==1);
 CHECK(r.sources==1);
 CHECK(strstr(r.evidence,"https://example.org/launch")!=NULL);
 CHECK(strstr(r.evidence,"127.0.0.1")==NULL);
 CHECK(extract_results("{}",2,&r)==0);
 CHECK(extract_results("{",1,&r)==0);
 printf("Live Research: %d/%d checks passed\n",checks-failures,checks);
 return failures?1:0;
}
