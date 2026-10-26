#define _POSIX_C_SOURCE 200809L
/* [AI:GPT-6 | 2026-10-25 18:28:00 UTC] */
#define ST1_CLI
#define ST1_TEST
#define main unused_st1_cli_main
#include "../src/st1.c"
#undef main
#include <assert.h>
#include <dlfcn.h>

static stnlabz_module_service_handler_fn registered_service;
static stnlabz_module_service_handler_fn registered_report;
static int reg_service(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(!strcmp(name,DIGIT_ST1_SERVICE)){registered_service=fn;return 1;}
 if(!strcmp(name,DIGIT_ST1_REPORT_SERVICE)){registered_report=fn;return 1;}
 return 0;
}
static int unreg_service(const char *name,void *ctx){
 (void)ctx;return !strcmp(name,DIGIT_ST1_SERVICE)||!strcmp(name,DIGIT_ST1_REPORT_SERVICE);
}
static void sample(digit_st1_record_t *r){
 memset(r,0,sizeof *r);
 strcpy(r->id,"ST1-1");strcpy(r->kind,"MISSION");
 strcpy(r->title,"Relay demonstration");strcpy(r->url,"https://example.org/report");
 strcpy(r->date,"2026-10-25");strcpy(r->summary,"Published finding");
 strcpy(r->evidence,"Reported");strcpy(r->relevance,"Medium");
 strcpy(r->systems,"Ezra,SD,communications");strcpy(r->impact,"None");
 strcpy(r->milestone,"Published");
}
static int find(const char *path,const char *action,const char *term,digit_st1_query_result_t *out){
 digit_st1_query_t q={0};
 strcpy(q.action,action);strcpy(q.term,term);
 return query(path,&q,out);
}
int main(void){
 char dir[]="/tmp/digit-st1-XXXXXX",path[256],symlink_path[256];
 digit_st1_record_t r;digit_st1_query_result_t result;unsigned checks=0;
 assert(mkdtemp(dir)!=NULL);
 assert(snprintf(path,sizeof path,"%s/research.tsv",dir)>0);
 assert(snprintf(symlink_path,sizeof symlink_path,"%s/alias",dir)>0);
 sample(&r);
 assert(valid(&r));checks++;
 assert(append_record(path,&r)&&r.revision==1);checks++;
 assert(find(path,"list","",&result)&&result.count==1);checks++;
 assert(!strcmp(result.records[0].title,"Relay demonstration"));checks++;
 assert(find(path,"show","ST1-1",&result)&&result.count==1);checks++;
 assert(find(path,"watchlist","",&result)&&result.count==1);checks++;
 assert(find(path,"search","communications",&result)&&result.count==1);checks++;
 strcpy(r.title,"Updated finding");strcpy(r.evidence,"Verified");
 assert(append_record(path,&r)&&r.revision==2);checks++;
 assert(find(path,"show","ST1-1",&result)&&result.count==1&&result.records[0].revision==2);checks++;
 assert(find(path,"history","ST1-1",&result)&&result.count==2);checks++;
 assert(!strcmp(result.records[0].evidence,"Reported")&&!strcmp(result.records[1].evidence,"Verified"));checks++;
 strcpy(r.id,"ST1-2");strcpy(r.kind,"REPORT");r.milestone[0]=0;
 assert(append_record(path,&r)&&r.revision==1);checks++;
 assert(find(path,"watchlist","",&result)&&result.count==1);checks++;
 assert(find(path,"list","",&result)&&result.count==2);checks++;
 strcpy(r.url,"http://unsafe");assert(!append_record(path,&r));checks++;
 strcpy(r.url,"https://example.org/report");
 strcpy(r.date,"2026-13-01");assert(!valid(&r));checks++;
 strcpy(r.date,"2026-10-25");r.summary[0]='\n';assert(!valid(&r));checks++;
 assert(symlink(path,symlink_path)==0);
 sample(&r);assert(!append_record(symlink_path,&r));checks++;
 assert(!find(path,"unrecognized","",&result));checks++;
 assert(!find(path,"search","",&result));checks++;
 assert(unlink(symlink_path)==0&&unlink(path)==0&&rmdir(dir)==0);
 {
  void *handle=dlopen("./bin/st1.so",RTLD_NOW|RTLD_LOCAL);
  const stnlabz_module_descriptor_t *(*descriptor_fn)(void)=NULL;
  const stnlabz_module_descriptor_t *descriptor;
  stnlabz_module_qualification_result_t q;
  stnlabz_module_host_t host={0};

  assert(handle!=NULL);
  *(void **)(&descriptor_fn)=dlsym(handle,"stnlabz_module_get_descriptor");
  assert(descriptor_fn!=NULL);
  descriptor=descriptor_fn();
  assert(!strcmp(descriptor->id,"st1")&&descriptor->version_major==0&&
         descriptor->version_minor==1&&descriptor->version_patch==3);checks++;
  assert(descriptor->qualify(&q)==STNLABZ_MODULE_OK&&
         q.tests_executed>=STNLABZ_MODULE_MIN_TESTS&&
         q.tests_passed==q.tests_executed&&q.negative_test_passed);checks++;
  host.register_service=reg_service;host.unregister_service=unreg_service;
  assert(descriptor->start(&host)==STNLABZ_MODULE_OK&&registered_service&&registered_report);checks++;
  assert(registered_service(NULL,0,NULL,0,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
  assert(registered_report(NULL,0,NULL,0,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
  assert(descriptor->stop()==STNLABZ_MODULE_OK);checks++;
  assert(dlclose(handle)==0);
 }
 printf("ST-1 registry and ABI fixture: %u/%u PASS\n",checks,checks);
 return 0;
}
/* [End AI:GPT-6] */
