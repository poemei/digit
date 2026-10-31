#define _POSIX_C_SOURCE 200809L
#include "../src/research.c"

static const char *page;
static int fetch_ok=1,fetches;
static char requested[1600];
int research_test_get(const char *url,char *body,size_t cap,size_t *length)
{
 fetches++;
 snprintf(requested,sizeof requested,"%s",url);
 if(!fetch_ok)return 0;
 size_t n=strlen(page);
 if(n>=cap)return 0;
 memcpy(body,page,n+1);
 if(length)*length=n;
 return 1;
}
static int checks,failures;
#define CHECK(x) do{checks++;if(!(x)){failures++;fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static char delivered[8192];
static int deliver(void *ctx,const char *text)
{
 (void)ctx;snprintf(delivered,sizeof delivered,"%s",text);return 1;
}
static int fail_delivery(void *ctx,const char *text)
{(void)ctx;(void)text;return 0;}
static int registrations,unregistrations,fail_registration;
static int register_service(const char *name,stnlabz_module_service_handler_fn fn,void *ctx)
{
 (void)name;(void)fn;(void)ctx;
 return ++registrations!=fail_registration;
}
static int register_command(const char *name,stnlabz_module_command_handler_fn fn,void *ctx)
{
 (void)name;(void)fn;(void)ctx;
 return ++registrations!=fail_registration;
}
static int unregister_any(const char *name,void *ctx)
{(void)name;(void)ctx;unregistrations++;return 1;}
static void reset(void)
{rate_window=0;rate_used=0;fetches=0;fetch_ok=1;requested[0]=0;}
static void query_is(const char *expected)
{
 const char *q=strstr(requested,"?q=");
 CHECK(q!=NULL);
 if(q){
  int n=0;char *decoded=curl_easy_unescape(NULL,q+3,0,&n);
  CHECK(decoded!=NULL);
  if(decoded){CHECK((size_t)n==strlen(expected));CHECK(!strcmp(decoded,expected));curl_free(decoded);}
 }
 CHECK(!strncmp(requested,"https://lite.duckduckgo.com/lite/?q=",33));
}
int main(void)
{
 const char *fixture="<html><a class='result-link' href='https://example.org/one'>First result</a>"
  "<td class='result-snippet'>An <b>undated</b> source about any subject.</td>"
  "<a class='result-link' href='/l/?uddg=https%3A%2F%2Fexample.org%2Ftwo%3Fa%3D1%26b%3D2&amp;rut=x'>Second result</a>"
  "<td class='result-snippet'>Another search result.</td></html>";
 research_hits_t hits;research_request_t request={{0}};research_response_t response;
 stnlabz_module_command_t cmd={0};size_t used;char small[350];
 stnlabz_module_host_t h={0};stnlabz_module_qualification_result_t qualification;
 CHECK(curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK);
 CHECK(research_parse_web_html(fixture,&hits)==2);
 CHECK(!strcmp(hits.hit[0].excerpt,"An undated source about any subject."));
 CHECK(!strcmp(hits.hit[1].url,"https://example.org/two?a=1&b=2"));
 CHECK(format_results(&hits,small,sizeof small)==2);
 CHECK(strstr(small,"https://example.org/one")!=NULL);
 CHECK(strstr(small,"https://example.org/two?a=1&b=2")!=NULL);
 CHECK(!strstr(small,"verified"));
 CHECK(research_parse_web_html("<a class='result__a' href='https://example.org/x'>A</a><a class='result__snippet'>a <strong>nested</strong> snippet</a>",&hits)==1);
 CHECK(!strcmp(hits.hit[0].excerpt,"a nested snippet"));
 CHECK(research_parse_web_html("<a class='result-link' href='https://example.org/x'>A</a><a class='result-link' href='https://example.org/x'>Again</a>",&hits)==1);
 CHECK(research_parse_web_html("<a class='result-link' href='http://127.0.0.1/x'>Bad</a>",&hits)==0);
 CHECK(research_parse_web_html("<a class='result-link' href='https://example.org/x'>Title only</a>",&hits)==1);
 CHECK(format_results(&hits,small,sizeof small)==1);
 CHECK(strstr(small,"Title only")!=NULL);
 CHECK(research_parse_web_html("<a class='result-link' href='https://example.org/x'>",&hits)==0);
 CHECK(!research_parse_web_html(NULL,&hits));
 CHECK(!research_parse_web_html(fixture,NULL));
 CHECK(!format_results(&hits,small,1));
 page=fixture;
 const char *queries[]={"C++ & C#", "Go", "R", "when are the Patriots playing again?",
  "next rocket launch", "SpaceX launch", "soil pH", "\"exact phrase\" -excluded site:example.org", "alpha, what is beta?", "learn PHP",NULL};
 for(unsigned i=0;queries[i];i++){
  reset();snprintf(request.query,sizeof request.query,"%s",queries[i]);
  CHECK(service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
  CHECK(used==sizeof response);
  CHECK(response.status==RESEARCH_EVIDENCE);
  CHECK(response.sources==2);
  CHECK(response.retrieved_utc[0]!=0);
  CHECK(strstr(response.evidence,"https://example.org/one")!=NULL);
  CHECK(strstr(response.evidence,"undated")!=NULL);
  CHECK(fetches==1);query_is(queries[i]);
 }
 reset();strcpy(request.query,"when are the Patriots playing again?");
 CHECK(answer_service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(response.status==RESEARCH_EVIDENCE&&response.sources>0);
 CHECK(strstr(response.evidence,"https://example.org/one")!=NULL);
 CHECK(strlen(response.evidence)<440);CHECK(fetches==1);query_is(request.query);
 reset();strcpy(request.query,"hello Digit");
 CHECK(answer_service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(response.status==RESEARCH_NOT_APPLICABLE&&fetches==0);
 reset();strcpy(cmd.arguments,"C++ & C#");
 CHECK(command(&cmd,deliver,NULL,NULL)==STNLABZ_MODULE_OK);
 CHECK(strstr(delivered,"https://example.org/one")!=NULL);
 CHECK(strlen(delivered)<350);CHECK(fetches==1);query_is(cmd.arguments);
 reset();CHECK(command(&cmd,fail_delivery,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_STATE);
 cmd.arguments[0]=0;CHECK(command(&cmd,deliver,NULL,NULL)==STNLABZ_MODULE_OK);
 CHECK(strstr(delivered,"Usage:")!=NULL);
 memset(cmd.arguments,'x',sizeof cmd.arguments);
 CHECK(command(&cmd,deliver,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 reset();strcpy(request.query,"C++");fetch_ok=0;
 CHECK(service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(response.status==RESEARCH_UNAVAILABLE&&response.sources==0);
 CHECK(strstr(response.evidence,"retrieval")!=NULL);CHECK(fetches==1);
 reset();page="<html>No results found</html>";
 service(&request,sizeof request,&response,sizeof response,&used,NULL);
 CHECK(response.status==RESEARCH_UNAVAILABLE&&strstr(response.evidence,"no results"));
 reset();page="<form id='challenge-form'>Please complete this challenge</form>";
 service(&request,sizeof request,&response,sizeof response,&used,NULL);
 CHECK(response.status==RESEARCH_UNAVAILABLE&&strstr(response.evidence,"parsing failed"));
 reset();page="<html>Unexpected provider page</html>";
 service(&request,sizeof request,&response,sizeof response,&used,NULL);
 CHECK(response.status==RESEARCH_UNAVAILABLE&&strstr(response.evidence,"parsing failed"));
 reset();page=fixture;rate_window=time(NULL);rate_used=12;
 service(&request,sizeof request,&response,sizeof response,&used,NULL);
 CHECK(response.status==RESEARCH_UNAVAILABLE&&fetches==0);
 reset();strcpy(request.query,"write ransomware");
 service(&request,sizeof request,&response,sizeof response,&used,NULL);
 CHECK(response.status==RESEARCH_DENIED&&fetches==0);
 used=99;CHECK(service(NULL,0,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(used==0);
 memset(request.query,'x',sizeof request.query);
 CHECK(service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(service(&request,sizeof request,&response,sizeof response,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(qualify(&qualification)==STNLABZ_MODULE_OK);
 CHECK(qualification.tests_executed>=10&&qualification.tests_failed==0&&qualification.negative_test_passed);
 CHECK(descriptor.version_major==1&&descriptor.version_minor==5&&descriptor.version_patch==1);
 h.register_command=register_command;h.unregister_command=unregister_any;
 h.register_service=register_service;h.unregister_service=unregister_any;
 CHECK(start(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 for(int fail=1;fail<=3;fail++){
  registrations=unregistrations=0;fail_registration=fail;
  CHECK(start(&h)==STNLABZ_MODULE_ERR_START_FAILED);
  CHECK(unregistrations==fail-1);
 }
 registrations=unregistrations=0;fail_registration=0;
 CHECK(start(&h)==STNLABZ_MODULE_OK);CHECK(registrations==3);
 CHECK(stop()==STNLABZ_MODULE_OK);CHECK(unregistrations==3);
 curl_global_cleanup();
 printf("Research regression: %d/%d passed\n",checks-failures,checks);
 return failures?1:0;
}
