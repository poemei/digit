/* Operator-run GRU bridge integration probe.
 * Loads the actual module, registers services in a test host, and invokes
 * generate through the published ABI. Never submits learning material. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
#include "module.h"
#include "gru_bridge.h"
static stnlabz_module_service_handler_fn generation;
static void *generation_context;
static stnlabz_module_service_handler_fn learning;
static int register_service(const char *name,
 stnlabz_module_service_handler_fn fn,void *ctx){
 if(!name||!fn)return 0;
 if(!strcmp(name,DIGIT_GRU_GENERATE_SERVICE)){
  if(generation)return 0;generation=fn;generation_context=ctx;return 1;
 }
 if(!strcmp(name,DIGIT_GRU_LEARN_SERVICE)){
  if(learning)return 0;learning=fn;return 1;
 }
 return 0;
}
static int unregister_service(const char *name,void *ctx){
 (void)ctx;
 if(!strcmp(name,DIGIT_GRU_GENERATE_SERVICE)&&generation){
  generation=NULL;generation_context=NULL;return 1;
 }
 if(!strcmp(name,DIGIT_GRU_LEARN_SERVICE)&&learning){
  learning=NULL;return 1;
 }
 return 0;
}
int main(int argc,char **argv){
 const char *library=argc>1?argv[1]:"./bin/gru_bridge.so";
 const char *prompt=argc>2?argv[2]:"The engineer ";
 const stnlabz_module_descriptor_t *(*get_descriptor)(void);
 const stnlabz_module_descriptor_t *descriptor=NULL;
 const stnlabz_module_host_t host={
  .register_service=register_service,
  .unregister_service=unregister_service
 };
 digit_gru_generate_request_t request={0};
 digit_gru_generate_result_t response={0};
 stnlabz_module_qualification_result_t qualification={0};
 size_t used=0;
 stnlabz_module_result_t result;
 void *handle;
 int status=1;
 if(strlen(prompt)>=sizeof request.prompt||!prompt[0]){
  fprintf(stderr,"Prompt must contain 1..1024 bytes.\n");return 2;
 }
 handle=dlopen(library,RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
 *(void **)(&get_descriptor)=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!get_descriptor||(descriptor=get_descriptor())==NULL||
    !descriptor->qualify||!descriptor->start||!descriptor->stop){
  fprintf(stderr,"Missing module lifecycle.\n");goto done;
 }
 result=descriptor->qualify(&qualification);
 if(result!=STNLABZ_MODULE_OK){
  fprintf(stderr,"Qualification failed: %u/%u\n",
   qualification.tests_passed,qualification.tests_executed);goto done;
 }
 result=descriptor->start(&host);
 if(result!=STNLABZ_MODULE_OK||!generation||!learning){
  fprintf(stderr,"Service registration failed (%d).\n",(int)result);goto done;
 }
 memcpy(request.prompt,prompt,strlen(prompt)+1);
 request.length=128;request.seed=913;request.temperature=0.65;
 result=generation(&request,sizeof request,&response,
                   sizeof response,&used,generation_context);
 if(result!=STNLABZ_MODULE_OK||used!=sizeof response||
    !memchr(response.text,0,sizeof response.text)||
    !response.experimental||response.generated_bytes!=strlen(response.text)){
  fprintf(stderr,"Generation failed: result=%d, bytes=%zu\n",(int)result,used);
 }else{
  printf("GRU bridge generation succeeded; %u bytes, experimental=%u\n",
   response.generated_bytes,response.experimental);
  printf("Candidate (NOT VERIFIED):\n%s\n",response.text);
  status=0;
 }
 if(descriptor->stop()!=STNLABZ_MODULE_OK){
  fprintf(stderr,"Module stop failed.\n");status=1;
 }
done:
 dlclose(handle);
 return status;
}
