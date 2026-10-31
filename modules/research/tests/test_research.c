#define RESEARCH_TEST
#include "../src/research.c"
int main(void)
{
 research_response_t response={0};
 const char *fixture="<?xml version=\"1.0\"?><rss><channel>"
 "<item><title>Launch schedule</title><link>https://example.org/launch</link>"
 "<pubDate>Fri, 30 Oct 2026 12:00:00 GMT</pubDate>"
 "<description>Schedule &amp; updates</description></item>"
 "<item><title>Unsafe</title><link>http://127.0.0.1/x</link></item>"
 "</channel></rss>";
 int fails=0,checks=0;
 #define CHECK(x) do{++checks;if(!(x)){++fails;fprintf(stderr,"FAIL line %d\n",__LINE__);}}while(0)
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(!forbidden("when is the next SpaceX launch?"));
 CHECK(forbidden(""));
 CHECK(extract_results(fixture,strlen(fixture),&response)==1);
 CHECK(response.sources==1);
 CHECK(strstr(response.evidence,"https://example.org/launch")!=NULL);
 CHECK(strstr(response.evidence,"Schedule & updates")!=NULL);
 CHECK(strstr(response.evidence,"127.0.0.1")==NULL);
 CHECK(extract_results("{}",2,&response)==0);
 CHECK(extract_results("<rss><item>",11,&response)==0);
 CHECK(safe_source_url("https://example.org/launch"));
 CHECK(!safe_source_url("http://example.org"));
 CHECK(!safe_source_url("https://localhost/private"));
 printf("Research qualification: %d/%d passed\n",checks-fails,checks);
 return fails?1:0;
}
