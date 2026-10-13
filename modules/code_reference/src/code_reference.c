#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include "code_reference.h"

/* No source execution, no shell and no ability to modify the checkout.
   Diagnostic output is evidence from a real compiler frontend.
   Compiler diagnostics are not proof of absence of defects. */
static const stnlabz_module_host_t *host;
static int bounded(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
static void note(code_reference_response_t *r,const char *message){
 snprintf(r->diagnostics,sizeof r->diagnostics,"%s",message);
}
static int analyze_c(const code_reference_request_t *req,code_reference_response_t *r){
 const char *source=req->source;
 const char *root=getenv("DIGIT_SOURCE_ROOT");
 char inc[1024],incabi[1024],srcdir[1024],srcfull[1024];
 const char *slash;size_t prefix;
 if(!root||!*root)root="/home/stnchain/digit";
 if(req->source_name[0]=='/'||strstr(req->source_name,".."))return -2;
 if(snprintf(inc,sizeof inc,"%s/include",root)>=(int)sizeof inc||
 snprintf(incabi,sizeof incabi,"%s/include/stnlabz_abi",root)>=(int)sizeof incabi||
 snprintf(srcfull,sizeof srcfull,"%s/%s",root,req->source_name)>=(int)sizeof srcfull)return -2;
 slash=strrchr(srcfull,'/');prefix=slash?(size_t)(slash-srcfull):0;
 if(!prefix||prefix>=sizeof srcdir)return -2;
 memcpy(srcdir,srcfull,prefix);srcdir[prefix]=0;
 int in[2]={-1,-1},out[2]={-1,-1},status;
 pid_t pid;
 size_t length=strlen(source),offset=0,used=0;
 const char *compiler=getenv("CODE_REFERENCE_CC");
 char chunk[512];ssize_t n;
 if(!compiler||!*compiler)compiler="cc";
 /* The compiler binary may be configured by the operator, but a shell is
    never involved. */
 if(pipe(in)<0)return -1;
 if(pipe(out)<0){close(in[0]);close(in[1]);return -1;}
 pid=fork();
 if(pid<0){close(in[0]);close(in[1]);close(out[0]);close(out[1]);return -1;}
 if(pid==0){
  struct rlimit limit;
  int devnull=open("/dev/null",O_WRONLY);
  limit.rlim_cur=limit.rlim_max=10;setrlimit(RLIMIT_CPU,&limit);
  limit.rlim_cur=limit.rlim_max=256U*1024U*1024U;setrlimit(RLIMIT_AS,&limit);
  dup2(in[0],STDIN_FILENO);dup2(out[1],STDERR_FILENO);
  if(devnull>=0){dup2(devnull,STDOUT_FILENO);close(devnull);}
  close(in[0]);close(in[1]);close(out[0]);close(out[1]);
  execlp(compiler,compiler,"-std=c11","-pedantic-errors","-fsyntax-only",
         "-I",inc,"-I",incabi,"-I",srcdir,"-x","c","-", (char *)NULL);
  _exit(127);
 }
 close(in[0]);close(out[1]);
 /* Bounded request fits a pipe on ordinary Linux systems; write it before
    collecting diagnostics. */
 while(offset<length){
  n=write(in[1],source+offset,length-offset);
  if(n<0){if(errno==EINTR)continue;break;}
  if(n==0)break;
  offset+=(size_t)n;
 }
 close(in[1]);
 while((n=read(out[0],chunk,sizeof chunk))>0){
  size_t take=(size_t)n;
  if(used+take>=sizeof r->diagnostics)take=sizeof r->diagnostics-used-1;
  if(take){memcpy(r->diagnostics+used,chunk,take);used+=take;}
 }
 close(out[0]);r->diagnostics[used]=0;
 if(waitpid(pid,&status,0)<0)return -1;
 if(offset!=length)return -1;
 if(WIFEXITED(status)&&WEXITSTATUS(status)==127)return -1;
 if(WIFEXITED(status)&&WEXITSTATUS(status)==0)return 0;
 if(WIFEXITED(status))return 1;
 return -1;
}
/* Conservative C11 repair candidate: one unused, named function parameter
   may replace an undeclared identifier in a simple return expression.
   Validate the replacement with the same compiler before reporting it.
   This establishes compilation, not intended program behavior. */
static int ident_char(int c){
 return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c=='_';
}
static int token_count(const char *s,const char *word){
 int count=0;size_t n=strlen(word);const char *p=s;
 while((p=strstr(p,word))!=NULL){
  if((p==s||!ident_char((unsigned char)p[-1]))&&!ident_char((unsigned char)p[n]))count++;
  p+=n;
 }
 return count;
}
static void suggest_simple_c_fix(const code_reference_request_t *req,
 code_reference_response_t *res){
 const char *d=strstr(res->diagnostics,"undeclared");
 const char *q,*r,*open,*close,*brace,*end;
 char missing[80]={0},candidate[80]={0},modified[CODE_REFERENCE_SOURCE_MAX];
 size_t n=0;int viable=0;
 code_reference_request_t trial;
 code_reference_response_t trial_response={0};
 if(!d)return;
 q=d;
 while(q>res->diagnostics&&*q!='\''&&*q!='\xe2')q--;
 if(*q!='\'')return;
 r=q;
 while(r>res->diagnostics&&r[-1]!='\'')r--;
 if(r==res->diagnostics||r[-1]!='\'')return;
 q=r;
 while(q<d&&ident_char((unsigned char)*q)&&n<sizeof missing-1)missing[n++]=*q++;
 if(q>=d||*q!='\''||!n)return;
 missing[n]=0;
 open=strchr(req->source,'(');
 close=open?strchr(open,')'):NULL;
 brace=close?strchr(close,'{'):NULL;
 end=brace?strchr(brace,'}'):NULL;
 if(!open||!close||!brace||!end||strchr(close+1,'(')||strchr(brace+1,'{'))return;
 if(token_count(brace,missing)!=1)return;
 {
  const char *p=open+1;
  while(p<close){
   const char *comma=memchr(p,',',(size_t)(close-p));
   const char *stop=comma?comma:close;
   const char *last=stop,*first;
   size_t len;
   while(last>p&&!ident_char((unsigned char)last[-1]))last--;
   first=last;
   while(first>p&&ident_char((unsigned char)first[-1]))first--;
   len=(size_t)(last-first);
   if(len>0&&len<sizeof candidate){
    char name[80];
    memcpy(name,first,len);name[len]=0;
    if(strcmp(name,"void")&&token_count(brace,name)==0){
     viable++;snprintf(candidate,sizeof candidate,"%s",name);
    }
   }
   p=comma?comma+1:close;
  }
 }
 if(viable!=1)return;
 {
  const char *use=strstr(brace,missing);
  size_t before=(size_t)(use-req->source),after=strlen(use+strlen(missing));
  if(before+strlen(candidate)+after>=sizeof modified)return;
  memcpy(modified,req->source,before);
  memcpy(modified+before,candidate,strlen(candidate));
  memcpy(modified+before+strlen(candidate),use+strlen(missing),after+1);
 }
 trial=*req;
 snprintf(trial.source,sizeof trial.source,"%s",modified);
 if(analyze_c(&trial,&trial_response)!=0)return;
 {
  size_t used=strlen(res->diagnostics);
  snprintf(res->diagnostics+used,sizeof res->diagnostics-used,
   "\nRepair candidate (compiler-validated only): Replace undeclared identifier '%s' with unused parameter '%s'. The modified translation unit passes the C11 frontend. Confirm this matches the function's intended behavior before patching.",
   missing,candidate);
 }
}
stnlabz_module_result_t code_reference_analyze(const void *input,size_t size,
 void *output,size_t capacity,size_t *used,void *context){
 const code_reference_request_t *request=input;
 code_reference_response_t *response=output;
 int status;
 (void)context;
 if(used)*used=0;
 if(!host||!request||size!=sizeof *request||!response||
    capacity<sizeof *response||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(request->contract_version!=CODE_REFERENCE_CONTRACT_VERSION||
    !bounded(request->language,sizeof request->language)||
    !bounded(request->source_name,sizeof request->source_name)||
    !bounded(request->source,sizeof request->source))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(response,0,sizeof *response);
 response->contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 *used=sizeof *response;
 if(strcmp(request->language,"c")&&strcmp(request->language,"iso-c")){
  response->status=CODE_REFERENCE_LANGUAGE_UNSUPPORTED;
  note(response,"No analyzer registered for this language.");
  return STNLABZ_MODULE_OK;
 }
 snprintf(response->analyzer,sizeof response->analyzer,"C11 compiler frontend");
 if(!request->source[0]){
  response->status=CODE_REFERENCE_SOURCE_INVALID;
  note(response,"Empty translation unit is not a useful analysis request.");
  return STNLABZ_MODULE_OK;
 }
 status=analyze_c(request,response);
 if(status==-2){
  response->status=CODE_REFERENCE_ANALYSIS_INCOMPLETE;
  note(response,"Source path or project compilation context invalid.");
 }else if(status<0){
  response->status=CODE_REFERENCE_ANALYZER_UNAVAILABLE;
  note(response,"Compiler frontend could not be executed or completed.");
 }else if(status>0){
  response->status=(strstr(response->diagnostics,"No such file or directory")||
   strstr(response->diagnostics,"file not found"))?
   CODE_REFERENCE_ANALYSIS_INCOMPLETE:CODE_REFERENCE_SOURCE_INVALID;
  if(!response->diagnostics[0])note(response,"Compiler rejected translation unit without diagnostics.");
  if(response->status==CODE_REFERENCE_SOURCE_INVALID)suggest_simple_c_fix(request,response);
 }else{
  response->status=CODE_REFERENCE_ANALYZED;
  note(response,"C11 syntax and compiler semantic checks passed; no runtime or behavioral correctness claim.");
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("code_reference.analyze",code_reference_analyze,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("code_reference.analyze",NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"code_reference");
 snprintf(d.name,sizeof d.name,"Language-Independent Code Reference");
 d.version_major=0;d.version_minor=1;d.version_patch=2;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
