#include <stdio.h>
#include <string.h>
#include "../src/ask_module.c"

static int calls_identity,calls_research,calls_llm,calls_peer,failures,checks;
static int scenario;
static char reply_buffer[1024],peer_buffer[1024];
#define CHECK(x) do {++checks;if(!(x)){++failures;fprintf(stderr,"FAIL line %d\n",__LINE__);}}while(0)

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
  research_response_t *out=response;
  calls_research++;
  if(scenario==9)return STNLABZ_MODULE_ERR_NOT_FOUND;
  if(!request||request_size!=sizeof(research_request_t)||!out||
     response_size<sizeof *out)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(out,0,sizeof *out);
  out->status=scenario==1?RESEARCH_EVIDENCE:
    scenario==3?RESEARCH_UNAVAILABLE:
    scenario==4?RESEARCH_DENIED:RESEARCH_NOT_APPLICABLE;
  if(scenario==1){
   out->sources=1;
   strcpy(out->evidence,"Next SpaceX launch: Starlink. Rocket: Falcon 9.");
  }else if(scenario==3){
   strcpy(out->evidence,"Search completed but returned no relevant sources.");
  }
  *response_used=sizeof *out;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_LLM_GENERATE_SERVICE)){
  digit_llm_response_t *out=response;
  calls_llm++;
  if(scenario==8)return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!request||request_size!=sizeof(digit_llm_request_t)||!out||
     response_size<sizeof *out)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(out,0,sizeof *out);
  strcpy(out->text,scenario==6?"hello":"The answer comes from the language service.");
  *response_used=sizeof *out;
  return STNLABZ_MODULE_OK;
 }
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static int mocked_reply(void *ctx,const char *message)
{
 (void)ctx;
 snprintf(reply_buffer,sizeof reply_buffer,"%s",message);
 return 1;
}
static int mocked_send(const char *message)
{
 calls_peer++;
 snprintf(peer_buffer,sizeof peer_buffer,"%s",message);
 return 1;
}
static int mock_register(const char *name,
 stnlabz_module_command_handler_fn fn,void *context)
{
 (void)context;
 return !strcmp(name,"chat")&&fn==controller;
}
static int mock_unregister(const char *name,void *context)
{
 (void)context;return !strcmp(name,"chat");
}
static stnlabz_module_host_t mock_host={
 .send_message=mocked_send,
 .register_command=mock_register,
 .unregister_command=mock_unregister,
 .invoke_service=mocked_service
};
static stnlabz_module_result_t ask(int selected,const char *text)
{
 stnlabz_module_command_t cmd={0};
 scenario=selected;calls_identity=calls_research=calls_llm=calls_peer=0;
 reply_buffer[0]=peer_buffer[0]=0;
 snprintf(cmd.arguments,sizeof cmd.arguments,"%s",text);
 return controller(&cmd,mocked_reply,NULL,NULL);
}
int main(void)
{
 stnlabz_module_qualification_result_t q={0};
 CHECK(qualify(&q)==STNLABZ_MODULE_OK);
 CHECK(q.tests_executed>=10&&q.tests_failed==0);
 CHECK(q.negative_test_executed==1&&q.negative_test_passed==1);
 CHECK(start(&mock_host)==STNLABZ_MODULE_OK);
 CHECK(ask(1,"when is the next SpaceX rocket launch?")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"Next SpaceX launch: Starlink. Rocket: Falcon 9."));
 CHECK(calls_identity==1&&calls_research==1&&calls_llm==0);
 CHECK(ask(2,"Explain TCP")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"The answer comes from the language service."));
 CHECK(calls_research==1&&calls_llm==1);
 CHECK(ask(3,"current stock price")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"Search completed but returned no relevant sources.")&&calls_llm==0);
 CHECK(ask(4,"prohibited query")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"can't help")!=NULL&&calls_llm==0);
 CHECK(ask(5,"what is your name?")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(reply_buffer,"I am Digit.")&&calls_research==0&&calls_llm==0);
 CHECK(ask(6,"ask rictus how are you")==STNLABZ_MODULE_OK);
 CHECK(!strcmp(peer_buffer,"rictus: hello")&&calls_peer==1);
 CHECK(calls_research==0&&calls_llm==1&&reply_buffer[0]==0);
 CHECK(ask(7,"hello")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"identity service")!=NULL&&calls_llm==0);
 CHECK(ask(8,"hello")==STNLABZ_MODULE_OK);
 CHECK(strstr(reply_buffer,"can't answer")!=NULL);
 CHECK(ask(9,"hello")==STNLABZ_MODULE_OK);
 CHECK(calls_llm==0&&strstr(reply_buffer,"Research is temporarily unavailable")!=NULL);
 CHECK(stop()==STNLABZ_MODULE_OK);
 printf("Ask controller: %d/%d checks passed\n",checks-failures,checks);
 return failures?1:0;
}
