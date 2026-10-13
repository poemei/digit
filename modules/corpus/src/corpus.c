#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "corpus_store.h"
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
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 return digit_corpus_selftest(q);
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_command||!h->unregister_command)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_command("corpus",search,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_command("corpus",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"corpus");
 (void)snprintf(d.name,sizeof d.name,"Corpus");
 d.version_major=0;d.version_minor=1;d.version_patch=1;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
