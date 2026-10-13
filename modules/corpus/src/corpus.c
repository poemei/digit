#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "corpus_store.h"
#include "corpus_lookup.h"
static const stnlabz_module_host_t *host;
static stnlabz_module_result_t search(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *ctx,void *unused){
 digit_corpus_record_t r;char msg[4608];int result;
 (void)unused;
 if(!host||!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result=digit_corpus_find(DIGIT_CORPUS_PATH,cmd->arguments,&r);
 if(result<0)return reply(ctx,"Corpus unavailable or invalid query.")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!result)return reply(ctx,"Corpus: no matching knowledge record.")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(msg,sizeof msg,"Corpus: %s [id=%s category=%s source=%s]",r.text,r.id,r.category,r.source);
 return reply(ctx,msg)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
/* Corpus owns lookup and source attribution; Ask receives only bounded evidence. */
static stnlabz_module_result_t lookup(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *context){
 const digit_corpus_lookup_request_t *q=request;
 digit_corpus_lookup_response_t *out=response;
 digit_corpus_record_t record;int result;
 (void)context;
 if(!host||!q||size!=sizeof *q||!out||capacity<sizeof *out||!used||
    !memchr(q->term,0,sizeof q->term)||!q->term[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result=digit_corpus_find(DIGIT_CORPUS_PATH,q->term,&record);
 if(result<0)return STNLABZ_MODULE_ERR_INVALID_STATE;
 memset(out,0,sizeof *out);
 if(result>0){
  out->found=1;
  snprintf(out->id,sizeof out->id,"%s",record.id);
  snprintf(out->source,sizeof out->source,"%s",record.source);
  snprintf(out->text,sizeof out->text,"%s",record.text);
 }
 *used=sizeof *out;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 return digit_corpus_selftest(q);
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_command||!h->unregister_command||
    !h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_command("corpus",search,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service("corpus.lookup",lookup,NULL)){
  (void)h->unregister_command("corpus",NULL);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("corpus.lookup",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_command("corpus",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"corpus");
 (void)snprintf(d.name,sizeof d.name,"Corpus");
 d.version_major=0;d.version_minor=1;d.version_patch=2;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
