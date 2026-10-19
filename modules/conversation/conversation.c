/* Conversation: contextual local inference over live IRC, SQLite3 observations
 * and SQLite3 corpus. Core routes ABI services; this module owns generation.
 * No keyword triggers, canned utterances, or corpus-match speech gate.
 * Runtime: local Ollama-compatible HTTP service on 127.0.0.1:11434.
 * Set DIGIT_CONVERSATION_MODEL to an installed model name.
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "module.h"
#include "../observation/observation.h"
#include "../sqlite3/corpus_search.h"
#define LISTEN "digit.conversation.listen"
#define DIRECT "digit.conversation"
#define TEXT_MAX 8192
#define HTTP_MAX 24576

typedef struct {char sender[128],account[128],target[128],message[512];} message_t;
static const stnlabz_module_host_t *api;

/* JSON escaping and decoding are transport mechanics, not language rules. */
static size_t json_escape(char *dst,size_t cap,const char *src){
 size_t n=0;
 for(;*src;src++){
  unsigned char c=(unsigned char)*src;
  const char *esc=c=='"'?"\\\"":c=='\\'?"\\\\":c=='\n'?"\\n":
                  c=='\r'?"\\r":c=='\t'?"\\t":NULL;
  size_t m=esc?strlen(esc):1;
  if(c<32&&!esc)return 0;
  if(n+m>=cap)return 0;
  if(esc){memcpy(dst+n,esc,m);n+=m;}else dst[n++]=(char)c;
 }
 if(n>=cap)return 0;
 dst[n]=0;return n;
}
static int json_value(const char *json,const char *key,char *out,size_t cap){
 char sought[80];const char *p;size_t n=0;
 if(snprintf(sought,sizeof sought,"\"%s\"",key)>= (int)sizeof sought)return 0;
 p=strstr(json,sought);if(!p)return 0;
 p+=strlen(sought);
 while(*p==' '||*p=='\t')p++;
 if(*p++!=':')return 0;
 while(*p==' '||*p=='\t')p++;
 if(*p++!='"')return 0;
 while(*p&&*p!='"'){
  unsigned char c=(unsigned char)*p++;
  if(c=='\\'){
   c=(unsigned char)*p++;
   if(c=='n')c='\n';else if(c=='r')c='\r';
   else if(c=='t')c='\t';
   else if(c=='u'){ /* Only ASCII Unicode escapes are decoded locally. */
    unsigned v=0;int j;
    for(j=0;j<4;j++){unsigned char x=(unsigned char)*p++;
     if(x>='0'&&x<='9')v=v*16+(x-'0');
     else if(x>='a'&&x<='f')v=v*16+x-'a'+10;
     else if(x>='A'&&x<='F')v=v*16+x-'A'+10;
     else return 0;
    }
    if(v>127)return 0;c=(unsigned char)v;
   }
  }
  if(n+1>=cap)return 0;
  out[n++]=(char)c;
 }
 if(*p!='"'||!n)return 0;
 out[n]=0;return 1;
}
static int local_generate(const char *prompt,char *output,size_t cap){
 const char *model=getenv("DIGIT_CONVERSATION_MODEL");
 char escaped[TEXT_MAX*2],model_json[256],body[TEXT_MAX*2+1024];
 char header[256],packet[TEXT_MAX*2+2048],response[HTTP_MAX];
 struct sockaddr_in addr;struct timeval timeout={4,0};
 int fd;size_t size=0,packet_len;ssize_t got;int n;
 if(!model||!*model||strlen(model)>110)return 0;
 if(!json_escape(model_json,sizeof model_json,model)||
    !json_escape(escaped,sizeof escaped,prompt))return 0;
 n=snprintf(body,sizeof body,
  "{\"model\":\"%s\",\"stream\":false,\"prompt\":\"%s\"}",
  model_json,escaped);
 if(n<=0||(size_t)n>=sizeof body)return 0;
 n=snprintf(header,sizeof header,
  "POST /api/generate HTTP/1.1\r\nHost: 127.0.0.1\r\n"
  "Content-Type: application/json\r\nConnection: close\r\n"
  "Content-Length: %zu\r\n\r\n",strlen(body));
 if(n<=0||(size_t)n>=sizeof header)return 0;
 packet_len=(size_t)n+strlen(body);
 if(packet_len>=sizeof packet)return 0;
 memcpy(packet,header,(size_t)n);memcpy(packet+n,body,strlen(body));
 fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 0;
 setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
 setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
 memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;
 addr.sin_port=htons(11434);
 inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
 if(connect(fd,(struct sockaddr *)&addr,sizeof addr)!=0){close(fd);return 0;}
 for(size_t off=0;off<packet_len;){
  got=send(fd,packet+off,packet_len-off,0);
  if(got<=0){close(fd);return 0;}off+=(size_t)got;
 }
 while(size+1<sizeof response){
  got=recv(fd,response+size,sizeof response-size-1,0);
  if(got<0){close(fd);return 0;}
  if(!got)break;size+=(size_t)got;
 }
 close(fd);response[size]=0;
 if(strncmp(response,"HTTP/1.1 200 ",13)&&
    strncmp(response,"HTTP/1.0 200 ",13))return 0;
 char *payload=strstr(response,"\r\n\r\n");
 if(!payload)return 0;
 return json_value(payload+4,"response",output,cap);
}
static int append(char *dest,size_t cap,const char *piece){
 size_t a=strlen(dest),b=strlen(piece);
 if(a+b>=cap)return 0;memcpy(dest+a,piece,b+1);return 1;
}
static int build_context(const message_t *m,char *prompt,size_t cap){
 digit_observation_recent_query_t q={0};
 digit_observation_recent_result_t history={0};
 size_t used=0;char line[800];
 if(snprintf(q.channel,sizeof q.channel,"%s",m->target)>=
    (int)sizeof q.channel)return 0;
 if(!append(prompt,cap,"Recent observations:\n"))return 0;
 if(api->invoke_service(DIGIT_OBSERVATION_RECENT_SERVICE,&q,sizeof q,
   &history,sizeof history,&used)==STNLABZ_MODULE_OK&&
   used==sizeof history&&history.count<=DIGIT_OBSERVATION_RECENT_MAX){
  for(unsigned i=0;i<history.count;i++){
   if(!memchr(history.items[i].text,0,sizeof history.items[i].text)||
      !memchr(history.items[i].sender,0,sizeof history.items[i].sender))return 0;
   snprintf(line,sizeof line,"%s: %.510s\n",
    history.items[i].sender[0]?history.items[i].sender:"unattributed",
    history.items[i].text);
   if(!append(prompt,cap,line))return 0;
  }
 }
 if(snprintf(line,sizeof line,"\nLive IRC message from %s: %.510s\n",
             m->sender,m->message)>=(int)sizeof line)return 0;
 return append(prompt,cap,line);
}
static stnlabz_module_result_t converse(const message_t *m,void *response,
 size_t capacity,size_t *used){
 char context[TEXT_MAX]={0},request[TEXT_MAX]={0},search[512]={0};
 char answer[2048]={0},line[700],prompt[TEXT_MAX]={0};
 digit_corpus_query_t query={0};digit_corpus_result_t hits={0};
 size_t found=0;unsigned i;
 if(used)*used=0;
 if(!m||!response||!used||capacity<2||!api||
    !memchr(m->sender,0,sizeof m->sender)||
    !memchr(m->target,0,sizeof m->target)||
    !memchr(m->message,0,sizeof m->message))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!m->message[0]||!m->sender[0])return STNLABZ_MODULE_OK;
 if(!build_context(m,context,sizeof context))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!append(request,sizeof request,
  "Read the IRC context. Write one short natural-language corpus search query "
  "about the conversation that could retrieve useful background knowledge. "
  "Return only the query, not an explanation. Do not invent a conversation.\n")||
    !append(request,sizeof request,context))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!local_generate(request,search,sizeof search))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 /* The model chooses retrieval intent; C does not select words or topics. */
 for(i=0;search[i];i++)
  if(search[i]=='\r'||search[i]=='\n')search[i]=' ';
 snprintf(query.query,sizeof query.query,"%s",search);
 if(api->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&query,sizeof query,
   &hits,sizeof hits,&found)!=STNLABZ_MODULE_OK||
   found!=sizeof hits||hits.count>DIGIT_CORPUS_RESULTS_MAX)
  memset(&hits,0,sizeof hits);
 if(!append(prompt,sizeof prompt,
   "You are Digit, a participant in this IRC channel. Interpret the "
   "surrounding conversation and speak in your own natural words. "
   "The notes are context, not lines to copy. Do not claim facts "
   "unsupported by the context. Reply with one concise IRC message, "
   "without a speaker label or formatting.\n")||
    !append(prompt,sizeof prompt,context)||
    !append(prompt,sizeof prompt,"\nRelevant corpus entries:\n"))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<hits.count;i++){
  if(!memchr(hits.hits[i].excerpt,0,sizeof hits.hits[i].excerpt))continue;
  snprintf(line,sizeof line,"Corpus %lld: %.350s\n",
           (long long)hits.hits[i].id,hits.hits[i].excerpt);
  if(!append(prompt,sizeof prompt,line))return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(!local_generate(prompt,answer,sizeof answer))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 size_t n=strcspn(answer,"\r\n");
 if(n>=capacity||n>350||!n)return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,answer,n);((char*)response)[n]=0;*used=n;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t handle(const void *request,size_t bytes,
 void *response,size_t capacity,size_t *used,void *ctx){
 (void)ctx;
 if(used)*used=0;
 if(bytes!=sizeof(message_t))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 return converse(request,response,capacity,used);
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 char escaped[100],decoded[100];const char *json="{\"response\":\"hi\\nthere\"}";
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 for(unsigned i=0;i<20;i++){
  int passed=0;
  switch(i%5){
   case 0:passed=json_escape(escaped,sizeof escaped,"cat")!=0&&!strcmp(escaped,"cat");break;
   case 1:passed=json_escape(escaped,sizeof escaped,"a\"b")!=0&&!strcmp(escaped,"a\\\"b");break;
   case 2:passed=json_value(json,"response",decoded,sizeof decoded)&&!strcmp(decoded,"hi\nthere");break;
   case 3:passed=!json_value("{}","response",decoded,sizeof decoded);break;
   default:{char s[8]="hi";passed=append(s,sizeof s,"!")&&!strcmp(s,"hi!");break;}
  }
  q->tests_executed++;if(passed)q->tests_passed++;else q->tests_failed++;
  if(i==3){q->negative_test_executed=1;q->negative_test_passed=passed;}
 }
 return !q->tests_failed?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(api||!h||!h->invoke_service||!h->register_service||!h->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service(LISTEN,handle,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(DIRECT,handle,NULL)){
  h->unregister_service(LISTEN,NULL);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 api=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 api->unregister_service(DIRECT,NULL);
 api->unregister_service(LISTEN,NULL);
 api=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="conversation",.name="Conversation contextual generation",
 .version_major=1,.version_minor=0,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
