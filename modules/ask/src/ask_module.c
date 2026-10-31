#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "digit_identity.h"
#include "digit_llm.h"
#include "research.h"

#define ASK_REPLY_CAP 448U
static const stnlabz_module_host_t *host;
static int registered;

static int peer_char(char c)
{
 return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||
  (c>='0'&&c<='9')||c=='_'||c=='-'||c=='['||c==']'||
  c=='\\'||c=='\x60'||c=='^'||c=='{'||c=='}'||c=='|';
}
static int token(const char *p,char *dest,size_t size)
{
 size_t n=0;
 while(*p==' '||*p=='\t')p++;
 while(*p&&peer_char(*p)&&n+1<size)dest[n++]=*p++;
 dest[n]=0;return n>0;
}
static int peer(const char *prompt,char *dest,size_t size)
{
 const char *p=prompt,*last=NULL;
 if(!p||!dest||size<2)return 0;
 while(*p==' '||*p=='\t')p++;
 if(!strncmp(p,"ask ",4))return token(p+4,dest,size);
 if(!strncmp(p,"tell ",5))return token(p+5,dest,size);
 if(!strncmp(p,"say ",4)){
  for(p+=4;(p=strstr(p," to "))!=NULL;p+=4)last=p+4;
  if(last)return token(last,dest,size);
 }
 return 0;
}
static int prefix_peer(const char *target,char *reply,size_t cap)
{
 char temporary[ASK_REPLY_CAP];
 size_t n;
 if(!target||!*target)return 1;
 n=strlen(target);
 if(!strncmp(reply,target,n)&&reply[n]==':')return 1;
 if(snprintf(temporary,sizeof temporary,"%s: %s",target,reply)
    >=(int)sizeof temporary)return 0;
 if(strlen(temporary)+1>cap)return 0;
 strcpy(reply,temporary);return 1;
}
static int reply_text(stnlabz_module_command_reply_fn reply,void *ctx,
                      const char *message)
{
 if(!reply||!message)return 0;
 /* Reply is one 447-byte message. Transport framing stays transport-owned. */
 char out[ASK_REPLY_CAP];
 size_t n=strlen(message);
 if(n>=sizeof out)n=sizeof out-1;
 memcpy(out,message,n);out[n]=0;
 if(n==sizeof out-1){
  while(n>1&&out[n-1]!=' '&&out[n-1]!='.')n--;
  if(n>1)out[n]=0;
 }
 return reply(ctx,out);
}
static stnlabz_module_result_t controller(
 const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *ctx,void *unused)
{
 digit_identity_question_t identity_in={{0}};
 digit_identity_answer_t identity_out={{0}};
 digit_llm_request_t llm_in={{0}};
 digit_llm_response_t llm_out={{0}};
 research_request_t research_in={{0}};
 research_response_t *research_out=NULL;
 size_t used=0,n;
 stnlabz_module_result_t rc;
 char target[128]={0},message[ASK_REPLY_CAP];
 (void)unused;
 if(!cmd||!reply||!host||!host->invoke_service||
    !memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 n=strlen(cmd->arguments);
 if(!n)return reply_text(reply,ctx,"What would you like to know?")?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 if(n>=sizeof identity_in.question||n>=sizeof research_in.query)
  return reply_text(reply,ctx,"That question is too long.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;

 /* Authoritative identity is handled by its owner, not by a generator. */
 memcpy(identity_in.question,cmd->arguments,n+1);
 rc=host->invoke_service(DIGIT_IDENTITY_ANSWER_SERVICE,
    &identity_in,sizeof identity_in,&identity_out,sizeof identity_out,&used);
 if(rc==STNLABZ_MODULE_OK){
  if(used!=sizeof identity_out||
     !memchr(identity_out.answer,0,sizeof identity_out.answer)||
     !identity_out.answer[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
  return reply_text(reply,ctx,identity_out.answer)?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(rc!=STNLABZ_MODULE_ERR_NOT_FOUND)
  return reply_text(reply,ctx,"My identity service is unavailable.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;

 /* Peer messages retain existing send-to-participant behavior. */
 if(!peer(cmd->arguments,target,sizeof target)){
  /* Research independently determines whether it claims this question.
   * Ask contains no research keywords, endpoint selection or search logic. */
  memcpy(research_in.query,cmd->arguments,n+1);
  research_out=calloc(1,sizeof *research_out);
  if(!research_out)return STNLABZ_MODULE_ERR_START_FAILED;
  used=0;
  rc=host->invoke_service(DIGIT_RESEARCH_ANSWER_SERVICE,
    &research_in,sizeof research_in,research_out,sizeof *research_out,&used);
  if(rc==STNLABZ_MODULE_OK){
   if(used!=sizeof *research_out||
      !memchr(research_out->evidence,0,sizeof research_out->evidence)){
    free(research_out);
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   }
   if(research_out->status!=RESEARCH_NOT_APPLICABLE){
    if(research_out->status==RESEARCH_EVIDENCE&&research_out->sources>0U)
     snprintf(message,sizeof message,"%s",research_out->evidence);
    else if(research_out->status==RESEARCH_DENIED)
     snprintf(message,sizeof message,"I can't help with that request.");
    else if(research_out->status==RESEARCH_UNAVAILABLE&&research_out->evidence[0])
     snprintf(message,sizeof message,"%s",research_out->evidence);
    else snprintf(message,sizeof message,
       "Research could not establish an answer.");
    free(research_out);
    return reply_text(reply,ctx,message)?
     STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
   }
  }else{
   /* An absent Research provider is NOT the same as a declared
    * NOT_APPLICABLE result. Never guess with an offline generator. */
   free(research_out);
   return reply_text(reply,ctx,"Research is temporarily unavailable.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  free(research_out);
 }

 /* Ollama is a language-generation service. It neither chooses a route
  * nor invokes the Research module or the Identity module. */
 memcpy(llm_in.prompt,cmd->arguments,n+1);
 used=0;
 rc=host->invoke_service(DIGIT_LLM_GENERATE_SERVICE,
    &llm_in,sizeof llm_in,&llm_out,sizeof llm_out,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof llm_out||
    !memchr(llm_out.text,0,sizeof llm_out.text)||!llm_out.text[0])
  return reply_text(reply,ctx,"I can't answer that right now.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 snprintf(message,sizeof message,"%s",llm_out.text);
 if(target[0]){
  if(!prefix_peer(target,message,sizeof message)||!host->send_message)
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  return host->send_message(message)?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 return reply_text(reply,ctx,message)?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static int selftest(unsigned *e,unsigned *p,int *ne,int *np)
{
 unsigned total=0,passed=0;int negative=0,negative_ok=0;
 char target[128],message[ASK_REPLY_CAP];
 #define CHECK(c) do{++total;if(c)++passed;}while(0)
 CHECK(peer("ask rictus how are you",target,sizeof target)&&!strcmp(target,"rictus"));
 CHECK(peer("tell rictus hello",target,sizeof target)&&!strcmp(target,"rictus"));
 CHECK(peer("say hello to rictus",target,sizeof target)&&!strcmp(target,"rictus"));
 CHECK(!peer("what is next SpaceX launch?",target,sizeof target));
 CHECK(!peer("hello",target,sizeof target));
 CHECK(peer("ask [friend] why",target,sizeof target)&&!strcmp(target,"[friend]"));
 strcpy(message,"hello");CHECK(prefix_peer("rictus",message,sizeof message)&&
  !strcmp(message,"rictus: hello"));
 CHECK(prefix_peer("rictus",message,sizeof message)&&!strcmp(message,"rictus: hello"));
 CHECK(prefix_peer("",message,sizeof message));
 CHECK(!peer("ask ",target,sizeof target));
 negative=1;negative_ok=!peer(NULL,target,sizeof target);
 #undef CHECK
 if(e)*e=total;if(p)*p=passed;
 if(ne)*ne=negative;if(np)*np=negative_ok;
 return total>=10&&total==passed&&negative==1&&negative_ok==1;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned e=0,p=0;int ne=0,np=0,ok;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 ok=selftest(&e,&p,&ne,&np);
 q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
 q->negative_test_executed=ne;q->negative_test_passed=np;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
 if(!h||!h->register_command||!h->unregister_command||
    !h->invoke_service||!h->send_message)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_command("chat",controller,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;registered=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(registered&&host)
  (void)host->unregister_command("chat",NULL);
 registered=0;host=NULL;
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "ask","Digit Ask Controller",1,0,1,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
