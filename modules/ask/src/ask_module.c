#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
/* Research requires an explicit operator intent. Ordinary conversation must
 * never be turned into a web search merely because it is a question. */
static const char *research_query(const char *message)
{
 const char *query=NULL;
 if(!message)return NULL;
 if(!strncmp(message,"research ",9))query=message+9;
 else if(!strncmp(message,"search ",7))query=message+7;
 if(!query)return NULL;
 while(*query==' '||*query=='\t')++query;
 return *query?query:NULL;
}
static void strip_unrequested_peer(char *text)
{
 char *colon,*p;
 size_t length;
 if(!text||!text[0])return;
 colon=strchr(text,':');
 if(!colon||colon==text||(size_t)(colon-text)>32U||colon[1]!=' ')return;
 for(p=text;p<colon;++p)if(!peer_char(*p))return;
 p=colon+2;
 length=strlen(p);
 memmove(text,p,length+1U);
}
/* Restore the historical basic greeting behavior independently of Ollama.
 * Match an entire greeting only; never intercept a technical request that
 * merely begins with hello/hi/hey (e.g. "Hey, inspect this source"). */
static const char *standalone_greeting(const char *input)
{
 char word[16];
 size_t n=0;
 const unsigned char *p=(const unsigned char *)input;
 if(!p)return NULL;
 while(*p&&isspace(*p))++p;
 while(*p&&isalpha(*p)){
  if(n+1U>=sizeof word)return NULL;
  word[n++]=(char)tolower(*p++);
 }
 if(!n)return NULL;
 word[n]=0;
 while(*p&&(*p=='!'||*p=='.'||*p==','||isspace(*p)))++p;
 if(*p)return NULL;
 if(!strcmp(word,"hello"))return "Hello!";
 if(!strcmp(word,"hi"))return "Hi!";
 if(!strcmp(word,"hey"))return "Hey!";
 return NULL;
}
static stnlabz_module_result_t controller(
 const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *ctx,void *unused)
{
 digit_identity_question_t identity_in={{0}};
 digit_identity_answer_t identity_out={{0}};
 digit_llm_request_t llm_in={{0}};
 digit_llm_response_t llm_out={{0}};
 stnlabz_module_result_t rc;
 size_t n,used=0;
 char target[128]={0},message[ASK_REPLY_CAP];
 const char *query;
 int directed;
 (void)unused;
 if(!cmd||!reply||!host||!host->invoke_service||
    !memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 n=strlen(cmd->arguments);
 if(!n)return reply_text(reply,ctx,"What would you like to know?")?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 if(n>=sizeof identity_in.question||n>=sizeof llm_in.prompt)
  return reply_text(reply,ctx,"That question is too long.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;

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

 /* Basic greetings must remain available when Ollama is down or slow. */
 if(standalone_greeting(cmd->arguments))
  return reply_text(reply,ctx,standalone_greeting(cmd->arguments))?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;

 directed=peer(cmd->arguments,target,sizeof target);
 /* Research only for explicit requests; /research remains Research-owned. */
 query=directed?NULL:research_query(cmd->arguments);
 if(query){
  research_request_t research_in={{0}};
  research_response_t *research_out;
  size_t qlen=strlen(query);
  if(qlen>=sizeof research_in.query)
   return reply_text(reply,ctx,"That research question is too long.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  research_out=calloc(1,sizeof *research_out);
  if(!research_out)return STNLABZ_MODULE_ERR_START_FAILED;
  memcpy(research_in.query,query,qlen+1U);
  used=0;
  rc=host->invoke_service(DIGIT_RESEARCH_ANSWER_SERVICE,
    &research_in,sizeof research_in,research_out,sizeof *research_out,&used);
  if(rc!=STNLABZ_MODULE_OK){
   free(research_out);
   return reply_text(reply,ctx,"Research service unavailable.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  if(used!=sizeof *research_out||
     !memchr(research_out->evidence,0,sizeof research_out->evidence)){
   free(research_out);
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  if(research_out->status==RESEARCH_DENIED){
   free(research_out);
   return reply_text(reply,ctx,"Research request denied by the safety gate.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  if(research_out->status!=RESEARCH_EVIDENCE&&
     research_out->status!=RESEARCH_UNAVAILABLE){
   free(research_out);
   return reply_text(reply,ctx,"Research returned no results.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  snprintf(message,sizeof message,"%s",research_out->evidence);
  free(research_out);
  if(!message[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
  return reply_text(reply,ctx,message)?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }

 /* Conversation and explicit peer messages use the existing lightweight
  * generator. The coder service is selected by engineering modules only. */
 memcpy(llm_in.prompt,cmd->arguments,n+1U);
 used=0;
 rc=host->invoke_service(DIGIT_LLM_GENERATE_SERVICE,
    &llm_in,sizeof llm_in,&llm_out,sizeof llm_out,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof llm_out||
    !memchr(llm_out.text,0,sizeof llm_out.text)||!llm_out.text[0])
  return reply_text(reply,ctx,"Language inference failed; no answer was generated.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 snprintf(message,sizeof message,"%s",llm_out.text);
 if(directed){
  if(!prefix_peer(target,message,sizeof message)||!host->send_message)
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  return host->send_message(message)?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 strip_unrequested_peer(message);
 if(!message[0])return reply_text(reply,ctx,"Language inference returned no usable answer.")?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
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
 CHECK(standalone_greeting("Hello")&&!strcmp(standalone_greeting("Hello"),"Hello!"));
 CHECK(standalone_greeting(" HI! ")&&!strcmp(standalone_greeting(" HI! "),"Hi!"));
 CHECK(standalone_greeting("hey,")&&!strcmp(standalone_greeting("hey,"),"Hey!"));
 CHECK(!standalone_greeting("hey what is C?"));
 CHECK(!standalone_greeting("hello can you debug this?"));
 CHECK(!standalone_greeting("hello research PHP 8.5"));
 CHECK(!standalone_greeting("hello: /github poemei/digit"));

 CHECK(!research_query("Hello"));
 CHECK(!research_query("What is PHP?"));
 CHECK(research_query("research PHP 8.5")&&
  !strcmp(research_query("research PHP 8.5"),"PHP 8.5"));
 CHECK(research_query("search SpaceX launch")&&
  !strcmp(research_query("search SpaceX launch"),"SpaceX launch"));
 CHECK(!research_query("research "));
 strcpy(message,"rictus: Hello");strip_unrequested_peer(message);
 CHECK(!strcmp(message,"Hello"));
 strcpy(message,"I checked: the source");strip_unrequested_peer(message);
 CHECK(!strcmp(message,"I checked: the source"));

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
 "ask","Digit Ask Controller",1,0,7,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
