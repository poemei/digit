#include <stdio.h>
#include <string.h>
#include "../src/ask_module.c"

static int calls_identity,calls_research,calls_llm,calls_peer,failures,checks;
static int scenario;
static char reply_buffer[1024],peer_buffer[1024],requested_query[512];
#define CHECK(x) do {++checks;if(!(x)){++failures;fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}}while(0)

static stnlabz_module_result_t mocked_service(
 const char *name,const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used)
{
 if(response_used)*response_used=0;
 if(!strcmp(name,DIGIT_IDENTITY_ANSWER_SERVICE)){
  digit_identity_answer_t *out=response;
  calls_identity++;
  if(scenario==5){
   if(!out||response_size<sizeof *out)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
   memset(out,0,sizeof *out);
   strcpy(out->answer,"I am Digit.");
   *response_used=sizeof *out;
   return STNLABZ_MODULE_OK;
  }
  if(scenario==7)return STNLABZ_MODULE_ERR_INVALID_STATE;
  return STNLABZ_MODULE_ERR_NOT_FOUND;
 }
 if(!strcmp(name,DIGIT_RESEARCH_ANSWER_SERVICE)){
  const research_request_t *in=request;
  research_response_t *out=response;
  calls_research++;
  if(scenario==9)return STNLABZ_MODULE_ERR_NOT_FOUND;
  if(!in||request_size!=sizeof *in||!out||response_size<sizeof *out)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  snprintf(requested_query,sizeof requested_query,"%s",in->query);
  memset(out,0,sizeof *out);
  if(scenario==4){
   out->status=RESEARCH_DENIED;
   strcpy(out->evidence,"Request denied.");
  }else if(scenario==3){
   out->status=RESEARCH_UNAVAILABLE;
   strcpy(out->evidence,"Search returned no results.");
  }else{
   out->status=RESEARCH_EVIDENCE;
   out->sources=2;
   strcpy(out->evidence,
    "1. Public event notice - Sunday at 4:25 PM (https://example.org/one)"
    " | 2. Another result - details (https://example.org/two)");
  }
  *response_used=sizeof *out;
  return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_LLM_GENERATE_SERVICE)){
  digit_llm_response_t *out=response;
  calls_llm++;
  if(scenario==8)return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!request||request_size!=sizeof(digit_llm_request_t)||!out||
     response_size<sizeof *out)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(out,0,sizeof *out);
  strcpy(out->text,"Hello peer");
  *response_used=sizeof *out;
  return STNLABZ_MODULE_OK;
 }
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static int mocked_reply(void *ctx,const char *message)
{
 (void)ctx;snprintf(reply_buffer,sizeof reply_buffer,"%s",message);return 1;
}
static int mocked_send(const char *message)
{
 calls_peer++;snprintf(peer_buffer,sizeof peer_buffer,"%s",message);return 1;
}
static int mock_register(const char *name,
 stnlabz_module_command_handler_fn fn,void *context)
{(void)context;return !strcmp(name,"chat")&&fn==controller;}
static int mock_unregister(const char *name,void *context)
{(void)context;return !strcmp(name,"chat");}
static stnlabz_module_host_t mock_host={
 .send_message=mocked_send,.register_command=mock_register,
 .unregister_command=mock_unregister,.invoke_service=mocked_service
};
static stnlabz_module_result_t ask(int selected,const char *text)
{
 stnlabz_module_command_t cmd={0};
 scenario=selected;calls_identity=calls_research=calls_llm=calls_peer=0;
 reply_buffer[0]=peer_buffer[0]=requested_query[0]=0;
 snprintf(cmd.arguments,sizeof cmd.arguments,"%s",text);
 return controller(&cmd,mocked_reply,NULL,NULL);
}
int main(void)
{
 stnlabz_module_qualification_result_t q={0};
 const char *expected=
  "1. Public event notice - Sunday at 4:25 PM (https://example.org/one)"
  " | 2. Another result - details (https://example.org/two)";
 CHECK(qualify(&q)==STNLABZ_MODULE_OK);
 CHECK(q.tests_executed>=10&&q.tests_failed==0);
 CHECK(q.negative_test_executed==1&&q.negative_test_passed==1);
 CHECK(start(&mock_host)==STNLABZ_MODULE_OK);

 CHECK(ask(1,"when are the New England Patriots playing again?")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,expected));
 CHECK(!strcmp(requested_query,"when are the New England Patriots playing again?"));
 CHECK(calls_identity==1&&calls_research==1&&calls_llm==0&&calls_peer==0);
 CHECK(ask(2,"C++ & C#")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,expected));
 CHECK(!strcmp(requested_query,"C++ & C#"));
 CHECK(calls_research==1&&calls_llm==0);
 CHECK(ask(3,"custom safe subject")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"Search returned no results."));
 CHECK(calls_research==1&&calls_llm==0);
 CHECK(ask(4,"prohibited query")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"safety gate")!=NULL&&calls_llm==0);
 CHECK(ask(5,"what is your name?")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"I am Digit.")&&calls_research==0&&calls_llm==0);
 CHECK(ask(6,"ask rictus how are you")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(peer_buffer,"rictus: Hello peer")&&calls_peer==1);
 CHECK(calls_research==0&&calls_llm==1&&reply_buffer[0]==0);
 CHECK(ask(7,"hello")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"identity service")!=NULL&&calls_llm==0&&calls_research==0);
 CHECK(ask(8,"tell rictus hello")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"Language inference failed")!=NULL&&calls_llm==1);
 CHECK(ask(9,"when does the event start?")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"Research service unavailable."));
 CHECK(calls_research==1&&calls_llm==0);
 CHECK(stop()==STNLABZ_MODULE_OK);
 printf("Ask controller: %d/%d checks passed\n",checks-failures,checks);
 return failures?1:0;
}
