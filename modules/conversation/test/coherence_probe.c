/* Offline coherence probe: intentionally NOT part of module qualification.
 * Proves whether Conversation can connect a short IRC exchange to knowledge.
 * v0.4.5 should fetch context and evidence, but coherent participation stays RED.
 * No production state, no database, no IRC connection.
 * Build: cc -I../../core/stnlabz_abi -std=c11 -Wall -Wextra -Wpedantic test/coherence_probe.c -o bin/coherence_probe -ldl
 * Run:   ./bin/coherence_probe ./bin/conversation.so
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module.h"
#include "../../sqlite3/corpus_search.h"
#include "../../observation/observation.h"

typedef struct { char sender[128],account[128],target[128],message[512]; } message_t;
static stnlabz_module_service_handler_fn listener;
static void *listener_context;
static unsigned corpus_reads,observation_reads;
static int register_service(const char *name,stnlabz_module_service_handler_fn fn,void *context){
 if(!strcmp(name,"digit.conversation.listen")){listener=fn;listener_context=context;}
 return 1;
}
static int unregister_service(const char *name,void *context){
 (void)context;
 if(!strcmp(name,"digit.conversation.listen")){listener=NULL;listener_context=NULL;}
 return 1;
}
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t req_size,
 void *resp,size_t cap,size_t *used){
 digit_corpus_result_t hits={0};
 (void)req;(void)req_size;
 if(used)*used=0;
 if(!strcmp(name,DIGIT_OBSERVATION_RECENT_SERVICE)){
  digit_observation_recent_result_t history={0};
  const digit_observation_recent_query_t *query=req;
  observation_reads++;
  if(!query||req_size!=sizeof *query||strcmp(query->channel,"#stn-labz")||
     !resp||!used||cap<sizeof history)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  history.count=3;
  snprintf(history.items[0].sender,sizeof history.items[0].sender,"Laurie_Vo");
  snprintf(history.items[0].text,sizeof history.items[0].text,"I am learning PHP");
  snprintf(history.items[1].sender,sizeof history.items[1].sender,"PoeMei");
  snprintf(history.items[1].text,sizeof history.items[1].text,
    "PHP has an echo statement, remember the semicolon");
  snprintf(history.items[2].sender,sizeof history.items[2].sender,"Laurie_Vo");
  snprintf(history.items[2].text,sizeof history.items[2].text,
    "I keep forgetting the semicolon!");
  memcpy(resp,&history,sizeof history);*used=sizeof history;return STNLABZ_MODULE_OK;
 }
 if(strcmp(name,DIGIT_CORPUS_SEARCH_SERVICE))return STNLABZ_MODULE_ERR_NOT_FOUND;
 corpus_reads++;
 if(!resp||!used||cap<sizeof hits)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 hits.count=2;
 hits.hits[0].id=1;
 snprintf(hits.hits[0].excerpt,sizeof hits.hits[0].excerpt,
  "In PHP, echo outputs text; a statement usually ends with a semicolon.");
 hits.hits[1].id=2;
 snprintf(hits.hits[1].excerpt,sizeof hits.hits[1].excerpt,
  "PHP echo 'Hello World!'; outputs the Hello World! text.");
 memcpy(resp,&hits,sizeof hits);*used=sizeof hits;return STNLABZ_MODULE_OK;
}
static int send(const char *sender,const char *message,int expect_speak){
 message_t in={0};char out[512]={0};size_t used=111;
 stnlabz_module_result_t rc;
 snprintf(in.sender,sizeof in.sender,"%s",sender);
 snprintf(in.account,sizeof in.account,"%s",sender);
 snprintf(in.target,sizeof in.target,"#stn-labz");
 snprintf(in.message,sizeof in.message,"%s",message);
 rc=listener(&in,sizeof in,out,sizeof out,&used,listener_context);
 printf("%s: %s (%s)\n",sender,message,used?out:"SILENT");
 return rc==STNLABZ_MODULE_OK && (expect_speak ? (used>0 && strstr(out,"semicolon")!=NULL) : used==0);
}
int main(int argc,char **argv){
 void *handle,*sym;
 const stnlabz_module_descriptor_t *(*descriptor)(void),*mod;
 stnlabz_module_host_t host={0};
 int passed=0,total=0;
 if(argc!=2)return 2;
 handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"%s\n",dlerror());return 2;}
 sym=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!sym){dlclose(handle);return 2;}
 memcpy(&descriptor,&sym,sizeof descriptor);mod=descriptor();
 host.register_service=register_service;host.unregister_service=unregister_service;
 host.invoke_service=invoke;
 if(!mod||!mod->start||!mod->stop||mod->start(&host)!=STNLABZ_MODULE_OK||!listener){
  fprintf(stderr,"Could not activate isolated Conversation fixture\n");dlclose(handle);return 2;
 }
 ++total;passed+=send("PoeMei","laurie where do you upload your module to?",0);
 ++total;passed+=send("Laurie_Vo","I am learning PHP",0);
 ++total;passed+=send("PoeMei","PHP has an echo statement, remember the semicolon",0);
 /* Desired acceptance case: after this exchange Digit can offer one grounded,
  * on-topic contribution. It currently cannot; this is a RED test. */
 ++total;passed+=send("Laurie_Vo","I keep forgetting the semicolon!",1);
 printf("Observation reads=%u, corpus reads=%u; coherence %d/%d\n",
  observation_reads,corpus_reads,passed,total);
 if(mod->stop()!=STNLABZ_MODULE_OK){dlclose(handle);return 2;}
 dlclose(handle);
 if(!observation_reads||!corpus_reads){fprintf(stderr,"Evidence integration missing\\n");return 3;}
 return passed==total?0:1;
}
