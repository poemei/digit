/* Operator-initiated learning service probe.
 * Explicitly submitted reviewed material only; never crawls observations.
 * A 202 response means queued for review, NOT trained. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "gru_bridge.h"

static stnlabz_module_service_handler_fn learning;
static void *learning_context;
static int register_service(const char *name,
 stnlabz_module_service_handler_fn fn,void *ctx){
 if(!name||!fn)return 0;
 if(!strcmp(name,DIGIT_GRU_LEARN_SERVICE)){
  if(learning)return 0;
  learning=fn;learning_context=ctx;return 1;
 }
 if(!strcmp(name,DIGIT_GRU_GENERATE_SERVICE))return 1;
 return 0;
}
static int unregister_service(const char *name,void *ctx){
 (void)ctx;
 if(!strcmp(name,DIGIT_GRU_LEARN_SERVICE)){
  if(!learning)return 0;
  learning=NULL;learning_context=NULL;return 1;
 }
 if(!strcmp(name,DIGIT_GRU_GENERATE_SERVICE))return 1;
 return 0;
}
static int copy_field(char *to,size_t capacity,const char *from){
 size_t count;
 if(!from)return 0;
 count=strlen(from);
 if(!count||count>=capacity)return 0;
 memcpy(to,from,count+1);return 1;
}
int main(int argc,char **argv){
 const stnlabz_module_descriptor_t *(*get_descriptor)(void)=NULL;
 const stnlabz_module_descriptor_t *descriptor=NULL;
 const stnlabz_module_host_t host={
  .register_service=register_service,
  .unregister_service=unregister_service
 };
 digit_gru_learn_request_t request={0};
 digit_gru_learn_result_t response={0};
 stnlabz_module_qualification_result_t qualification={0};
 stnlabz_module_result_t result;
 size_t used=0;int exit_status=1,started=0;
 void *handle;
 if(argc!=6||strcmp(argv[5],"--approved")!=0){
  fprintf(stderr,"Usage: %s MODULE_SO INPUT RESPONSE SOURCE --approved\n",argv[0]);
  fprintf(stderr,"Use only genuinely reviewed and explicitly approved material.\n");
  return 2;
 }
 if(!getenv("STN_GRU_LEARN_TOKEN")||!getenv("STN_GRU_LEARN_TOKEN")[0]){
  fprintf(stderr,"Missing STN_GRU_LEARN_TOKEN in process environment.\n");return 2;
 }
 if(!copy_field(request.input,sizeof request.input,argv[2])||
    !copy_field(request.response,sizeof request.response,argv[3])||
    !copy_field(request.source,sizeof request.source,argv[4])){
  fprintf(stderr,"Invalid field length or empty input/response/source.\n");
  return 2;
 }
 request.approved=1;
 handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
 *(void **)(&get_descriptor)=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!get_descriptor||(descriptor=get_descriptor())==NULL||
    !descriptor->qualify||!descriptor->start||!descriptor->stop){
  fprintf(stderr,"Missing module lifecycle.\n");goto done;
 }
 result=descriptor->qualify(&qualification);
 if(result!=STNLABZ_MODULE_OK){
  fprintf(stderr,"Qualification failed (%u/%u).\n",
   qualification.tests_passed,qualification.tests_executed);goto done;
 }
 result=descriptor->start(&host);
 if(result!=STNLABZ_MODULE_OK){
  fprintf(stderr,"Module startup failed (%d).\n",(int)result);goto done;
 }
 started=1;
 if(!learning){
  fprintf(stderr,"Learning service was not registered.\n");goto done;
 }
 result=learning(&request,sizeof request,&response,sizeof response,
                 &used,learning_context);
 if(result!=STNLABZ_MODULE_OK||used!=sizeof response||
    response.accepted!=1||response.trained!=0||
    strcmp(response.status,"queued_for_review")){
  fprintf(stderr,"Learning submission failed (result=%d, used=%zu).\n",
   (int)result,used);goto done;
 }
 printf("Learning example accepted and queued for review. Trained: NO.\n");
 exit_status=0;
done:
 if(started&&descriptor->stop()!=STNLABZ_MODULE_OK){
  fprintf(stderr,"Module stop failed.\n");exit_status=1;
 }
 dlclose(handle);
 return exit_status;
}
