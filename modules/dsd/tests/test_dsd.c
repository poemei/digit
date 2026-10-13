#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dsd.h"
#include "code_reference.h"

static stnlabz_module_command_handler_fn handler;
static stnlabz_module_service_handler_fn service;
static char output[8192];
static size_t used;
static unsigned analysis_calls;
static unsigned tests, failures;
static void check(int ok,const char *name){
 ++tests;if(!ok){++failures;fprintf(stderr,"FAIL DSD: %s\n",name);}
}
static int reg_command(const char *name,stnlabz_module_command_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(name,"diagnose"))return 0;handler=fn;return 1;
}
static int unreg_command(const char *name,void *ctx){
 (void)ctx;if(strcmp(name,"diagnose"))return 0;handler=NULL;return 1;
}
static int reg_service(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(name,"dsd.report"))return 0;service=fn;return 1;
}
static int unreg_service(const char *name,void *ctx){
 (void)ctx;if(strcmp(name,"dsd.report"))return 0;service=NULL;return 1;
}
static stnlabz_module_result_t invoke_analysis(const char *name,const void *input,
 size_t size,void *output,size_t capacity,size_t *response_used){
 const code_reference_request_t *request=input;
 code_reference_response_t *response=output;
 if(strcmp(name,"code_reference.analyze"))return STNLABZ_MODULE_ERR_NOT_FOUND;
 ++analysis_calls;
 if(size!=sizeof *request||capacity<sizeof *response)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(response,0,sizeof *response);
 response->contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 response->status=CODE_REFERENCE_ANALYZED;
 snprintf(response->analyzer,sizeof response->analyzer,"fixture");
 snprintf(response->diagnostics,sizeof response->diagnostics,"fixture evidence only");
 *response_used=sizeof *response;
 return STNLABZ_MODULE_OK;
}
static int capture(void *ctx,const char *s){
 size_t n=strlen(s);(void)ctx;
 if(used+n+2>=sizeof output)return 0;
 memcpy(output+used,s,n);used+=n;output[used++]='\n';output[used]=0;return 1;
}
static void diagnose(const char *arg){
 stnlabz_module_command_t cmd;
 memset(&cmd,0,sizeof cmd);
 snprintf(cmd.name,sizeof cmd.name,"diagnose");
 snprintf(cmd.arguments,sizeof cmd.arguments,"%s",arg);
 used=0;output[0]=0;
 check(handler&&handler(&cmd,capture,NULL,NULL)==STNLABZ_MODULE_OK,"diagnose command executes");
}
int main(void){
 char directory[]="/tmp/dsd-test-XXXXXX",cwd[1024],logs[1100],path[1200];
 FILE *fp;
 stnlabz_module_descriptor_t desc=digit_dsd_descriptor();
 stnlabz_module_host_t host={0};
 stnlabz_module_qualification_result_t qualification;
 check(!strcmp(desc.id,"dsd"),"module identity");
 check(desc.version_major==0&&desc.version_minor==3&&desc.version_patch==7,"module version");
 check(desc.qualify(&qualification)==STNLABZ_MODULE_OK&&qualification.tests_failed==0,"qualifies");
 check(mkdtemp(directory)!=NULL,"fixture directory");
 if(!getcwd(cwd,sizeof cwd)||chdir(directory)!=0)return 2;
 snprintf(logs,sizeof logs,"%s/logs",directory);
 if(mkdir(logs,0700)!=0)return 2;
 snprintf(path,sizeof path,"%s/modules.log",logs);
 fp=fopen(path,"w");if(!fp)return 2;
 fputs("2026-10-13T03:51:47Z MODULE FAILED id=dsd file=modules/dsd/src/dsd.c line=42 reason=fixture_failure\n",fp);
 fclose(fp);
 snprintf(path,sizeof path,"%s/digit.log",logs);
 fp=fopen(path,"w");if(!fp)return 2;
 fputs("2026-10-13T15:09:43Z ROUTER DISPATCH request=console-1 route=/Digit, service=none outcome=2 service_result=1\n",fp);
 fclose(fp);
 host.register_command=reg_command;host.unregister_command=unreg_command;
 host.register_service=reg_service;host.unregister_service=unreg_service;
 host.invoke_service=invoke_analysis;
 check(desc.start(&host)==STNLABZ_MODULE_OK,"starts");
 check(handler!=NULL&&service!=NULL,"ABI registrations");
 diagnose("fixture_failure");
 check(strstr(output,"Module: dsd")!=NULL,"identifies failed module");
 check(strstr(output,"/modules/dsd/src/dsd.c:42")!=NULL,"verifies module source against checkout");
 check(strstr(output,"Verified source line:")!=NULL,"inspects recorded source line instead of repeating log");
 /* Malformed or stale line evidence must not become a claimed root cause. */
 snprintf(path,sizeof path,"%s/modules.log",logs);
 fp=fopen(path,"a");if(!fp)return 2;
 fputs("2026-10-13T17:00:00Z MODULE FAILED id=dsd file=modules/dsd/src/dsd.c line=999999 reason=stale_trace\n",fp);
 fclose(fp);
 diagnose("stale_trace");
 check(strstr(output,"SOURCE LINE NOT PRESENT")!=NULL,"stale line rejected");
 check(strstr(output,"diagnosis unresolved")!=NULL,"stale source does not establish cause");
 /* Analyze a complete, independently prepared translation unit through
    the Core-mediated ABI callback rather than loading Code Reference. */
 {
  char tinyroot[1200],tinydir[1300],tinyfile[1400],oldroot[1100];
  const char *before=getenv("DIGIT_SOURCE_ROOT");
  snprintf(oldroot,sizeof oldroot,"%s",before?before:"");
  snprintf(tinyroot,sizeof tinyroot,"%s/source",directory);
  snprintf(tinydir,sizeof tinydir,"%s/modules",tinyroot);
  if(mkdir(tinyroot,0700)!=0||mkdir(tinydir,0700)!=0)return 2;
  snprintf(tinydir,sizeof tinydir,"%s/modules/mock",tinyroot);
  if(mkdir(tinydir,0700)!=0)return 2;
  snprintf(tinydir,sizeof tinydir,"%s/modules/mock/src",tinyroot);
  if(mkdir(tinydir,0700)!=0)return 2;
  snprintf(tinyfile,sizeof tinyfile,"%s/mock.c",tinydir);
  fp=fopen(tinyfile,"w");if(!fp)return 2;
  fputs("int answer(void){return 42;}\n",fp);fclose(fp);
  snprintf(path,sizeof path,"%s/modules.log",logs);
  fp=fopen(path,"a");if(!fp)return 2;
  fputs("2026-10-13T18:00:00Z MODULE FAILED id=mock file=modules/mock/src/mock.c line=1 reason=fixture_analysis\n",fp);
  fclose(fp);
  setenv("DIGIT_SOURCE_ROOT",tinyroot,1);
  diagnose("fixture_analysis");
  check(analysis_calls>0,"Core-mediated analysis invoked");
  check(strstr(output,"Code Reference: C11 frontend accepted")!=NULL,"analysis evidence included");
  check(strstr(output,"Module: mock")!=NULL,"diagnostic retains module");
  if(before)setenv("DIGIT_SOURCE_ROOT",oldroot,1);else unsetenv("DIGIT_SOURCE_ROOT");
  unlink(tinyfile);rmdir(tinydir);
  snprintf(tinydir,sizeof tinydir,"%s/modules/mock",tinyroot);rmdir(tinydir);
  snprintf(tinydir,sizeof tinydir,"%s/modules",tinyroot);rmdir(tinydir);
  rmdir(tinyroot);
 }
 diagnose("my last request");
 check(strstr(output,"Module: router")!=NULL,"identifies router");
 check(strstr(output,"route=/Digit,")!=NULL,"preserves route evidence");
 check(strstr(output,"No registration for /Digit,")!=NULL,"assesses command through module source inventory");
 check(strstr(output,"parse=")!=NULL&&strstr(output,"bind=")!=NULL&&strstr(output,"lookup=")!=NULL&&strstr(output,"reject=")!=NULL,"traces console registration and Router lookup");
 check(strstr(output,"src/router.c:")!=NULL&&strstr(output,"src/mvc_console.c:")!=NULL,"locates both code paths in checkout");
 diagnose("my last request");
 check(strstr(output,"not a proven Router defect")!=NULL||strstr(output,"scan was incomplete")!=NULL,"does not invent Router defect when evidence is incomplete");
 /* Two requests within one timestamp second must resolve to the latest
    appended console request, not the first matching timestamp. */
 snprintf(path,sizeof path,"%s/digit.log",logs);
 fp=fopen(path,"a");if(!fp)return 2;
 fputs("2026-10-13T15:09:43Z ROUTER DISPATCH request=console-2 route=/help service=none outcome=2 service_result=1\n",fp);
 fclose(fp);
 diagnose("my last request");
 check(strstr(output,"route=/help")!=NULL,"last request selected by append order");
 check(strstr(output,"No registration for /help")!=NULL,"missing help registration assessed");
 {
  const char *saved=getenv("DIGIT_SOURCE_ROOT");
  char root_copy[1024];
  snprintf(root_copy,sizeof root_copy,"%s",saved?saved:"");
  setenv("DIGIT_SOURCE_ROOT","/nonexistent/dsd-test-source",1);
  diagnose("my last request");
  check(strstr(output,"SOURCE ACCESS FAILED")!=NULL,"reports source access failure");
  check(strstr(output,"diagnosis incomplete")!=NULL,"does not fabricate a source finding");
  if(saved)setenv("DIGIT_SOURCE_ROOT",root_copy,1);
  else unsetenv("DIGIT_SOURCE_ROOT");
 }
 diagnose("nonexistent_record");
 check(strstr(output,"no matching recorded event")!=NULL,"negative evidence handled");
 check(desc.stop()==STNLABZ_MODULE_OK,"stops");
 check(handler==NULL&&service==NULL,"ABI unregistrations");
 if(chdir(cwd)!=0)return 2;
 snprintf(path,sizeof path,"%s/digit.log",logs);unlink(path);
 snprintf(path,sizeof path,"%s/modules.log",logs);unlink(path);
 rmdir(logs);rmdir(directory);
 printf("DSD tests: %u executed, %u failed\n",tests,failures);
 printf("DSD TEST SUMMARY\nAssertions executed: %u\nAssertions passed: %u\nAssertions failed: %u\nOVERALL RESULT: %s\n",tests,tests-failures,failures,failures?"FAIL":"PASS");
 return failures?1:0;
}
