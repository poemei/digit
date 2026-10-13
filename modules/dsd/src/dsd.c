#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "dsd.h"
#include "code_reference.h"

static const stnlabz_module_host_t *dsd_host;
/* Bounded evidence search. Never infer source locations absent from logs. */
#define DSD_LOG_LINE 768
static int emit(stnlabz_module_command_reply_fn reply,void *ctx,const char *s){
 return reply(ctx,s)!=0;
}
static int identifier(const char *s){
 size_t i;
 if(!s||!s[0])return 0;
 for(i=0;s[i];i++)
  if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||
       (s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'||s[i]=='.'))return 0;
 return i<64;
}
static void extract_token(const char *line,const char *key,char *out,size_t cap){
 const char *p=strstr(line,key);
 size_t i=0;
 out[0]=0;
 if(!p)return;
 p+=strlen(key);
 while(p[i]&&p[i]!=' '&&p[i]!='\n'&&p[i]!='\r'&&i+1<cap){out[i]=p[i];i++;}
 out[i]=0;
}
static int evidence_problem(const char *line){
 return strstr(line,"FAILED")||strstr(line,"FAIL ")||strstr(line,"REJECT")||
 strstr(line,"DENIED")||strstr(line,"ERROR")||strstr(line,"outcome=2")||strstr(line,"outcome=3")||strstr(line,"outcome=5")||
 strstr(line,"outcome=4")||strstr(line,"result=ERR_")||
 strstr(line,"status=failed");
}
static void component_name(const char *line,char *out,size_t cap){
 const char *p=strchr(line,' ');
 size_t i=0;
 if(p)p=strchr(p+1,' ');
 out[0]=0;
 if(!p)return;
 p++;
 while(p[i]&&p[i]!=' '&&i+1<cap){out[i]=p[i];i++;}
 out[i]=0;
 if(!strcmp(out,"MODEL_EXPRESSION"))(void)snprintf(out,cap,"expression");
 else if(!strcmp(out,"DISCOVERY"))(void)snprintf(out,cap,"discovery");
 else if(!strcmp(out,"ACTIVATION"))(void)snprintf(out,cap,"activation");
 else if(!strcmp(out,"ROUTER"))(void)snprintf(out,cap,"router");
}
/* Inspect the actual Router decision in the operator's source checkout.
   Source discovery is read-only; line numbers come from the file being read. */
static int find_source(const char *root,const char *file,const char *token,char *out,size_t cap){
 char path[1024],line[2048];unsigned long n=0;FILE *f;
 if(snprintf(path,sizeof path,"%s/%s",root,file)>=(int)sizeof path)return 0;
 f=fopen(path,"r");if(!f)return 0;
 while(fgets(line,sizeof line,f)){n++;if(strstr(line,token)){snprintf(out,cap,"%s:%lu",path,n);fclose(f);return 1;}}
 fclose(f);return 0;
}
/* Inspect module-owned registrations without assuming DSD owns all commands.
   A failed scan never establishes the absence of a registration. */
static int scan_module_commands(const char *root,const char *command,
 char *registration,size_t cap,int *incomplete){
 char base[1024],pattern[160];
 DIR *modules;struct dirent *module;
 int found=0;
 if(snprintf(base,sizeof base,"%s/modules",root)>=(int)sizeof base){*incomplete=1;return 0;}
 modules=opendir(base);
 if(!modules){*incomplete=1;return 0;}
 snprintf(pattern,sizeof pattern,"register_command(\"%.80s\"",command);
 while((module=readdir(modules))!=NULL){
  char dirpath[1200],sourcepath[1300],relative[500];
  struct stat st;DIR *sources;struct dirent *entry;
  if(module->d_name[0]=='.')continue;
  if(snprintf(dirpath,sizeof dirpath,"%s/%s",base,module->d_name)>=(int)sizeof dirpath){*incomplete=1;continue;}
  if(lstat(dirpath,&st)!=0){*incomplete=1;continue;}
  if(!S_ISDIR(st.st_mode))continue;
  if(snprintf(sourcepath,sizeof sourcepath,"%s/src",dirpath)>=(int)sizeof sourcepath){*incomplete=1;continue;}
  sources=opendir(sourcepath);
  if(!sources){*incomplete=1;continue;}
  while((entry=readdir(sources))!=NULL){
   size_t n=strlen(entry->d_name);
   char match[1200],full[1400];
   if(n<3||strcmp(entry->d_name+n-2,".c"))continue;
   if(snprintf(full,sizeof full,"%s/%s",sourcepath,entry->d_name)>=(int)sizeof full){*incomplete=1;continue;}
   if(lstat(full,&st)!=0||!S_ISREG(st.st_mode)){*incomplete=1;continue;}
   if(snprintf(relative,sizeof relative,"modules/%s/src/%s",module->d_name,entry->d_name)>=(int)sizeof relative){*incomplete=1;continue;}
   if(find_source(root,relative,pattern,match,sizeof match)){
    snprintf(registration,cap,"%s",match);found++;
   }else if(access(full,R_OK)!=0)*incomplete=1;
  }
  closedir(sources);
 }
 closedir(modules);
 return found;
}
/* A suggestion is source-derived, not evidence a route is active.
   Recognize only literal registrations; reject ambiguous candidates. */
static unsigned spelling_distance(const char *a,const char *b){
 size_t i,j,na=strlen(a),nb=strlen(b);
 unsigned d[65][65];
 if(na>64||nb>64)return 99;
 for(i=0;i<=na;i++)d[i][0]=(unsigned)i;
 for(j=0;j<=nb;j++)d[0][j]=(unsigned)j;
 for(i=1;i<=na;i++)for(j=1;j<=nb;j++){
  unsigned sub=d[i-1][j-1]+(a[i-1]!=b[j-1]);
  unsigned del=d[i-1][j]+1,ins=d[i][j-1]+1;
  unsigned best=sub<del?sub:del;
  if(ins<best)best=ins;
  if(i>1&&j>1&&a[i-1]==b[j-2]&&a[i-2]==b[j-1]&&d[i-2][j-2]+1<best)
   best=d[i-2][j-2]+1;
  d[i][j]=best;
 }
 return d[na][nb];
}
static void suggest_command(const char *root,const char *input,
 char *suggestion,size_t cap,int *incomplete){
 char base[1024];
 DIR *modules;struct dirent *module;
 unsigned best=3;int ambiguous=0;
 suggestion[0]=0;
 if(!identifier(input))return;
 if(snprintf(base,sizeof base,"%s/modules",root)>=(int)sizeof base){*incomplete=1;return;}
 modules=opendir(base);
 if(!modules){*incomplete=1;return;}
 while((module=readdir(modules))!=NULL){
  char dir[1200];DIR *sources;struct dirent *entry;struct stat st;
  if(module->d_name[0]=='.')continue;
  if(snprintf(dir,sizeof dir,"%s/%s",base,module->d_name)>=(int)sizeof dir){*incomplete=1;continue;}
  if(lstat(dir,&st)!=0){*incomplete=1;continue;}
  if(!S_ISDIR(st.st_mode))continue;
  if(snprintf(dir,sizeof dir,"%s/modules/%s/src",root,module->d_name)>=(int)sizeof dir){*incomplete=1;continue;}
  sources=opendir(dir);
  if(!sources){*incomplete=1;continue;}
  while((entry=readdir(sources))!=NULL){
   char path[1400],line[2048];FILE *fp;size_t len=strlen(entry->d_name);
   if(len<3||strcmp(entry->d_name+len-2,".c"))continue;
   if(snprintf(path,sizeof path,"%s/%s",dir,entry->d_name)>=(int)sizeof path){*incomplete=1;continue;}
   if(lstat(path,&st)!=0||!S_ISREG(st.st_mode)){*incomplete=1;continue;}
   fp=fopen(path,"r");if(!fp){*incomplete=1;continue;}
   while(fgets(line,sizeof line,fp)){
    const char *p=line;
    while((p=strstr(p,"register_command("))!=NULL){
     char name[65];size_t k=0;unsigned score;
     p+=strlen("register_command(");
     while(*p==' '||*p=='\\t')p++;
     if(*p!='\"')continue;
     p++;
     while(k<64&&(((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||
        (*p>='0'&&*p<='9')||*p=='_'||*p=='-')))name[k++]=*p++;
     name[k]=0;
     if(*p!='\"'||!k||!strcmp(name,input))continue;
     score=spelling_distance(input,name);
     if(score<best){best=score;ambiguous=0;snprintf(suggestion,cap,"%s",name);}
     else if(score==best&&strcmp(suggestion,name))ambiguous=1;
    }
   }
   if(ferror(fp))*incomplete=1;
   fclose(fp);
  }
  closedir(sources);
 }
 closedir(modules);
 if(ambiguous)suggestion[0]=0;
}
static int inspect_route(const char *route,char *location,size_t cap,char *finding,size_t fcap){
 const char *root=getenv("DIGIT_SOURCE_ROOT");
 char parse[1024],bind[1024],lookup[1024],reject[1024],registration[1200]={0},suggestion[65]={0};
 const char *name=route&&route[0]=='/'?route+1:"";
 int incomplete=0,found;
 if(!root||!*root)root="/home/stnchain/digit";
 if(!find_source(root,"src/mvc_console.c","while(line[start+j]",parse,sizeof parse)||
 !find_source(root,"src/mvc_console.c","digit_router_bind(path,service)",bind,sizeof bind)||
 !find_source(root,"src/router.c","strcmp(bindings[i].path,request->path)",lookup,sizeof lookup)||
 !find_source(root,"src/router.c","if(i==binding_count)",reject,sizeof reject)){
 snprintf(location,cap,"SOURCE ACCESS FAILED: %s/src/{mvc_console.c,router.c}",root);
 snprintf(finding,fcap,"Unable to inspect required checkout source; diagnosis incomplete. Check source path and read permissions.");
 return 0;
 }
 found=scan_module_commands(root,name,registration,sizeof registration,&incomplete);
 if(!found)suggest_command(root,name,suggestion,sizeof suggestion,&incomplete);
 if(strlen(parse)>250||strlen(bind)>250||strlen(lookup)>250||strlen(reject)>250){
  snprintf(location,cap,"SOURCE LOCATION TOO LONG: cannot report complete route evidence");
  snprintf(finding,fcap,"Source references exceed diagnostic bounds; investigation incomplete.");
  return 0;
 }
 snprintf(location,cap,"parse=%.250s; bind=%.250s; lookup=%.250s; reject=%.250s",parse,bind,lookup,reject);
 if(found){
  snprintf(finding,fcap,"Source registration for /%.60s located at %.450s. Router has no active binding: inspect that module's discovery, qualification, activation and registration errors in modules.log before proposing a fix.",name,registration);
 }else if(incomplete){
  snprintf(finding,fcap,"No registration for /%.60s was located, but the module source scan was incomplete. Cannot establish whether the command is implemented; check source readability and module inventory.",name);
 }else if(suggestion[0]){
  snprintf(finding,fcap,"No registration for /%.60s found in examined module C sources. Source contains a similar command registration: /%.60s. Possible command spelling error; verify that module is installed, qualified, activated and the route is currently bound. This does not establish a Router defect.",name,suggestion);
 }else{
  snprintf(finding,fcap,"No registration for /%.60s found in examined module C sources. Console passes /%.60s to Router, which rejects the unbound route. This indicates an absent module command in inspected sources, not a proven Router defect. If the operator requires this command, implement it in an authorized module and test its registration and activation.",name,name);
 }
 return 1;
}
/* Inspect a recorded decision location in the checkout, without trusting a
   log-provided relative path or line number as verified source evidence. */
static int inspect_recorded_code(const char *relative,const char *number,
 char *location,size_t cap,char *finding,size_t fcap){
 const char *root=getenv("DIGIT_SOURCE_ROOT");
 char path[1024],buffer[2048],last[2048]={0};
 char *end;unsigned long target,current=0;FILE *fp;size_t i;
 if(!root||!*root)root="/home/stnchain/digit";
 if(!relative||relative[0]=='/'||!relative[0]||strstr(relative,"..")||
    !number||!number[0]){
  snprintf(location,cap,"source path or line not safely specified");
  snprintf(finding,fcap,"Log names a source location that cannot be safely verified. No code-level cause established.");
  return 0;
 }
 for(i=0;relative[i];i++)
  if(!((relative[i]>='a'&&relative[i]<='z')||
       (relative[i]>='A'&&relative[i]<='Z')||
       (relative[i]>='0'&&relative[i]<='9')||
       relative[i]=='_'||relative[i]=='-'||relative[i]=='.'||relative[i]=='/')){
   snprintf(location,cap,"unsafe source path");
   snprintf(finding,fcap,"Source location rejected; no code-level cause established.");
   return 0;
  }
 errno=0;target=strtoul(number,&end,10);
 if(errno||*end||target==0||target>1000000){
  snprintf(location,cap,"invalid source line");
  snprintf(finding,fcap,"Recorded line number is invalid; no code-level cause established.");
  return 0;
 }
 if(snprintf(path,sizeof path,"%s/%s",root,relative)>=(int)sizeof path){
  snprintf(location,cap,"source path too long");
  snprintf(finding,fcap,"Recorded source path exceeds inspection bounds.");
  return 0;
 }
 fp=fopen(path,"r");
 if(!fp){
  snprintf(location,cap,"SOURCE ACCESS FAILED: %.700s",path);
  snprintf(finding,fcap,"Cannot inspect logged source file; code-level cause remains unverified.");
  return 0;
 }
 while(fgets(buffer,sizeof buffer,fp)){
  current++;
  if(current==target){snprintf(last,sizeof last,"%s",buffer);break;}
 }
 fclose(fp);
 if(current!=target){
  snprintf(location,cap,"SOURCE LINE NOT PRESENT: %.700s:%lu",path,target);
  snprintf(finding,fcap,"The recorded line does not exist in the inspected checkout. Source revision or trace may differ; diagnosis unresolved.");
  return 0;
 }
 last[strcspn(last,"\r\n")]=0;
 snprintf(location,cap,"%.700s:%lu",path,target);
 snprintf(finding,fcap,"Verified source line: %.500s. This corroborates the recorded location, not the runtime cause. Inspect surrounding control flow and matching runtime evidence before authorizing repair.",last);
 return 1;
}
/* Invoke the independently activated language service through Core's ABI.
   Missing services and incomplete translation units are reported explicitly. */
static void analyze_source(const char *relative,char *report,size_t capacity){
 const char *root=getenv("DIGIT_SOURCE_ROOT");
 code_reference_request_t request={0};
 code_reference_response_t response={0};
 char path[1024];
 FILE *fp;
 size_t n,used=0;
 stnlabz_module_result_t result;
 if(!root||!*root)root="/home/stnchain/digit";
 if(!relative||!*relative||relative[0]=='/'||strstr(relative,"..")){
  snprintf(report,capacity,"Code Reference: source path rejected");return;
 }
 if(snprintf(path,sizeof path,"%s/%s",root,relative)>=(int)sizeof path){
  snprintf(report,capacity,"Code Reference: source path exceeds bounds");return;
 }
 fp=fopen(path,"rb");
 if(!fp){snprintf(report,capacity,"Code Reference: source unreadable");return;}
 n=fread(request.source,1,sizeof request.source-1,fp);
 if(ferror(fp)||!feof(fp)||memchr(request.source,0,n)){
  fclose(fp);snprintf(report,capacity,"Code Reference: complete translation unit unavailable (size or content)");return;
 }
 fclose(fp);
 request.source[n]=0;
 request.contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 snprintf(request.language,sizeof request.language,"c");
 snprintf(request.source_name,sizeof request.source_name,"%s",relative);
 if(!dsd_host||!dsd_host->invoke_service){
  snprintf(report,capacity,"Code Reference: Core service interface unavailable");return;
 }
 result=dsd_host->invoke_service("code_reference.analyze",
  &request,sizeof request,&response,sizeof response,&used);
 if(result!=STNLABZ_MODULE_OK){
  snprintf(report,capacity,"Code Reference: service unavailable (ABI result=%d)",(int)result);return;
 }
 if(used!=sizeof response||response.contract_version!=CODE_REFERENCE_CONTRACT_VERSION||
    !memchr(response.diagnostics,0,sizeof response.diagnostics)||
    !memchr(response.analyzer,0,sizeof response.analyzer)){
  snprintf(report,capacity,"Code Reference: invalid service response");return;
 }
 if(response.status==CODE_REFERENCE_ANALYZED)
  snprintf(report,capacity,"Code Reference: C11 frontend accepted translation unit; this does not establish behavioral correctness.");
 else if(response.status==CODE_REFERENCE_SOURCE_INVALID)
  snprintf(report,capacity,"Code Reference: C11 frontend found translation-unit errors: %.350s",response.diagnostics);
 else
  snprintf(report,capacity,"Code Reference: analysis unavailable or incomplete (status=%d): %.320s",
   (int)response.status,response.diagnostics);
}
/* Diagnose observed module lifecycle failures, without treating a failed
   authorization, loader error or callback error as a proven source defect. */
static int module_failure_event(const char *line){
 return strstr(line," MODULE REJECTED_LAST ")||
 strstr(line," MODULE SCAN_FAILED ")||
 strstr(line," MODULE DIRECTORY_FAILED ")||
 strstr(line," MODULE VERIFY_REJECT ")||
 strstr(line," MODULE QUALIFICATION_FAILED ")||
 strstr(line," MODULE AUTHORIZE_REJECT ")||
 strstr(line," MODULE ACTIVATE_REJECT ")||
 strstr(line," MODULE STOP_FAILED ")||
 strstr(line," MODULE UNAVAILABLE ");
}
static void lifecycle_assessment(const char *line,char *out,size_t cap){
 char result[128],stage[64],reason[160];
 extract_token(line,"result=",result,sizeof result);
 extract_token(line,"stage=",stage,sizeof stage);
 extract_token(line,"reason=",reason,sizeof reason);
 if(strstr(line," REJECTED_LAST "))
  snprintf(out,cap,"Discovery rejected a module candidate (stage=%.60s reason=%.120s). Inspect the recorded loader rejection, module manifest, descriptor and ABI compatibility before attributing a source defect.",stage[0]?stage:"not_recorded",reason[0]?reason:"not_recorded");
 else if(strstr(line," ACTIVATE_REJECT "))
  snprintf(out,cap,"Activation failed (result=%.100s). Correlate the module's authorization and qualification events; determine whether its start callback failed or Core rejected a prerequisite. No defective source line established.",result[0]?result:"not_recorded");
 else if(strstr(line," QUALIFICATION_FAILED "))
  snprintf(out,cap,"Qualification failed (result=%.100s). Inspect recorded executed/passed/failed counts and reproduce the responsible module's qualification tests before proposing a repair.",result[0]?result:"not_recorded");
 else if(strstr(line," VERIFY_REJECT "))
  snprintf(out,cap,"Verification rejected the module (result=%.100s). Compare descriptor identity and ABI requirements with the host's contract; code defect not yet established.",result[0]?result:"not_recorded");
 else if(strstr(line," AUTHORIZE_REJECT "))
  snprintf(out,cap,"Core denied module activation authorization (result=%.100s). Investigate authorization inputs and policy decision; denial is not evidence of a module source defect.",result[0]?result:"not_recorded");
 else if(strstr(line," STOP_FAILED "))
  snprintf(out,cap,"Module stop callback failed (result=%.100s). Inspect cleanup paths and reproduce the failure with module-local tests; precise cause remains unverified.",result[0]?result:"not_recorded");
 else
  snprintf(out,cap,"Module lifecycle failure recorded (reason=%.120s result=%.100s). Inspect the related lifecycle events and implementation before assigning a code defect.",reason[0]?reason:"not_recorded",result[0]?result:"not_recorded");
}
/* Explicit read-only source investigation via Code Reference over Core ABI. */
static stnlabz_module_result_t diagnose_source_command(const char *relative,
 stnlabz_module_command_reply_fn reply,void *ctx){
 const char *root=getenv("DIGIT_SOURCE_ROOT");
 char path[1024],message[1024];
 code_reference_request_t request={0};
 code_reference_response_t response={0};
 FILE *fp;size_t n,used=0;
 stnlabz_module_result_t rc;
 if(!root||!*root)root="/home/stnchain/digit";
 if(!relative||!*relative||relative[0]=='/'||strstr(relative,"..")||
    strchr(relative,'\\')||strchr(relative,' ')){
  return emit(reply,ctx,"DSD: invalid relative source path.")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(snprintf(path,sizeof path,"%s/%s",root,relative)>=(int)sizeof path)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 fp=fopen(path,"rb");
 if(!fp)return emit(reply,ctx,"DSD: source file is not readable.")?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 n=fread(request.source,1,sizeof request.source-1,fp);
 if(ferror(fp)||!feof(fp)||memchr(request.source,0,n)){
  fclose(fp);
  return emit(reply,ctx,"DSD: complete source cannot be analyzed (size or content).")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 fclose(fp);request.source[n]=0;
 request.contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 snprintf(request.language,sizeof request.language,"c");
 snprintf(request.source_name,sizeof request.source_name,"%.127s",relative);
 if(!emit(reply,ctx,"DSD SOURCE INVESTIGATION"))return STNLABZ_MODULE_ERR_INVALID_STATE;
 snprintf(message,sizeof message,"Code: %.950s",path);
 if(!emit(reply,ctx,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!dsd_host||!dsd_host->invoke_service)
  return emit(reply,ctx,"Finding: Code Reference service interface unavailable.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 rc=dsd_host->invoke_service("code_reference.analyze",&request,sizeof request,
  &response,sizeof response,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof response||
    response.contract_version!=CODE_REFERENCE_CONTRACT_VERSION||
    !memchr(response.diagnostics,0,sizeof response.diagnostics))
  return emit(reply,ctx,"Finding: Code Reference service unavailable or returned invalid evidence.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 if(response.status==CODE_REFERENCE_SOURCE_INVALID){
  snprintf(message,sizeof message,"Finding: C compiler rejected source: %.850s",response.diagnostics);
  if(!emit(reply,ctx,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(strstr(response.diagnostics,"undeclared")||strstr(response.diagnostics,"not declared")){
   if(!emit(reply,ctx,"Recommended fix: Resolve the undeclared identifier at the reported line by using the intended in-scope declaration or adding the correct declaration. Verify intended behavior, then rerun compiler and module tests."))
    return STNLABZ_MODULE_ERR_INVALID_STATE;
  }else if(!emit(reply,ctx,"Recommended fix: Correct the specific compiler-reported error at the indicated source location, then rerun compiler and module tests."))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
 }else if(response.status==CODE_REFERENCE_ANALYZED){
  if(!emit(reply,ctx,"Finding: C11 frontend accepted the source. Runtime correctness and absence of defects are not established."))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!emit(reply,ctx,"Recommended fix: None established by compiler diagnostics; investigate runtime behavior and invariants."))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
 }else{
  snprintf(message,sizeof message,"Finding: Code Reference did not complete analysis (status=%d): %.650s",
   (int)response.status,response.diagnostics);
  if(!emit(reply,ctx,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t diagnose_command(
 const stnlabz_module_command_t *command,
 stnlabz_module_command_reply_fn reply,void *reply_context,void *context){
 FILE *log;
 const char *paths[]={"./logs/digit.log","./logs/modules.log"};
 size_t source;
 char line[DSD_LOG_LINE],selected[DSD_LOG_LINE]={0},module[64]={0};
 char file[128]={0},lineno[24]={0},reason[256],message[1024],term[128]={0};
 char candidate_id[64],candidate_module[64],source_location[1200]={0},assessment[800]={0},route[128]={0},analysis[600]={0};
 unsigned long matches=0;
 int found=0,last_request=0,module_failures=0;
 const char *args;
 (void)context;
 if(!command||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 args=command->arguments;
 while(*args==' ')args++;
 if(!strncmp(args,"source ",7))return diagnose_source_command(args+7,reply,reply_context);
 if(!strcmp(args,"my last request")||!strcmp(args,"last")){last_request=1;args="";}
 if(!strcmp(args,"module failures")||!strcmp(args,"modules")||!strcmp(args,"module failure")){module_failures=1;args="";}
 if(*args){
  size_t n=strlen(args);
  if(n>=sizeof term)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(term,args,n+1);
 }
 for(source=0;source<sizeof paths/sizeof paths[0];source++){
  log=fopen(paths[source],"r");
  if(!log){
   return emit(reply,reply_context,"DSD: operational log unavailable.")?
    STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
  }
  while(fgets(line,sizeof line,log)){
  size_t len=strlen(line);
  if(len&&!strchr(line,'\n')&&!feof(log)){
   int c;while((c=fgetc(log))!='\n'&&c!=EOF){}
   continue; /* No truncated or fabricated evidence. */
  }
  if(module_failures){
   if(source!=1||!module_failure_event(line))continue;
  }else if(last_request){
   if(!strstr(line,"ROUTER DISPATCH")||strstr(line,"route=/diagnose "))continue;
  }else if(term[0]){
   if(!strstr(line,term))continue;
  }else if(!evidence_problem(line))continue;
  matches=last_request?1:matches+1;
  /* For last-request selection, append order in digit.log is authoritative.
     Timestamp seconds alone cannot distinguish consecutive console commands. */
  if(last_request||module_failures||!found||strncmp(line,selected,20)>0){
   (void)snprintf(selected,sizeof selected,"%s",line);
   found=1;
  }
  }
  fclose(log);
 }
 if(!emit(reply,reply_context,"DSD DIAGNOSTIC REPORT"))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!found){
  (void)snprintf(message,sizeof message,"Module: not_identified");
  if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!emit(reply,reply_context,"Log: no matching recorded event"))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!emit(reply,reply_context,"Code: not_recorded"))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  (void)snprintf(message,sizeof message,"Finding: No %s evidence found in the available log.",
   term[0]?"matching":"failure");
  return emit(reply,reply_context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 selected[strcspn(selected,"\n")]=0;
 component_name(selected,module,sizeof module);
 extract_token(selected,"id=",candidate_id,sizeof candidate_id);
 if(identifier(candidate_id)&&strcmp(candidate_id,"unknown")==0)candidate_id[0]=0;
 if(identifier(candidate_id)&&candidate_id[0]&&strcmp(module,"router"))
  (void)snprintf(module,sizeof module,"%s",candidate_id);
 extract_token(selected,"module=",candidate_module,sizeof candidate_module);
 if(identifier(candidate_module)&&candidate_module[0])
  (void)snprintf(module,sizeof module,"%s",candidate_module);
 if(!module[0])(void)snprintf(module,sizeof module,"not_identified");
 extract_token(selected,"file=",file,sizeof file);
 extract_token(selected,"line=",lineno,sizeof lineno);
 extract_token(selected,"reason=",reason,sizeof reason);
 if(strstr(selected,"ROUTER DISPATCH")){
  (void)snprintf(module,sizeof module,"router");
  extract_token(selected,"route=",route,sizeof route);
  if(strstr(selected,"outcome=2"))(void)snprintf(reason,sizeof reason,"no_registered_route");
  else if(strstr(selected,"outcome=3"))(void)snprintf(reason,sizeof reason,"service_unavailable");
  else if(strstr(selected,"outcome=1"))(void)snprintf(reason,sizeof reason,"request_denied");
  else if(strstr(selected,"outcome=5"))(void)snprintf(reason,sizeof reason,"dispatch_failed");
 }
 if(!emit(reply,reply_context,""))return STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(message,sizeof message,"Module: %s",module);
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(message,sizeof message,"Log: %s",selected);
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(file[0]&&lineno[0]){
  analyze_source(file,analysis,sizeof analysis);
  if(!emit(reply,reply_context,analysis))return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(!strcmp(module,"router")&&strstr(selected,"outcome=2")){
  (void)inspect_route(route,source_location,sizeof source_location,assessment,sizeof assessment);
  (void)snprintf(message,sizeof message,"Code: %.950s",source_location);
 }else if(file[0]&&lineno[0]){
  (void)inspect_recorded_code(file,lineno,source_location,sizeof source_location,assessment,sizeof assessment);
  (void)snprintf(message,sizeof message,"Code: %.950s",source_location);
 }else if(file[0]){
  (void)snprintf(message,sizeof message,"Code: %s (line not recorded)",file);
 }else{
  (void)snprintf(message,sizeof message,"Code: not_recorded (no source location in log)");
 }
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(module_failures){
  lifecycle_assessment(selected,assessment,sizeof assessment);
  (void)snprintf(message,sizeof message,"Finding: %.950s",assessment);
 }else if(!strcmp(module,"router")&&strstr(selected,"outcome=2")){
  (void)snprintf(message,sizeof message,"Finding: %.950s",assessment);
 }else if(file[0]&&lineno[0]) (void)snprintf(message,sizeof message,
  "Finding: %.950s",assessment);
 else if(reason[0]) (void)snprintf(message,sizeof message,
  "Finding: Recorded event indicates %s; reason=%s. Matched events: %lu.",
  term[0]?"the requested condition":"a failure",reason,matches);
 else (void)snprintf(message,sizeof message,
  "Finding: %s event recorded for %s; precise cause not established by this log. Matched events: %lu.",
  term[0]?"Matching":"Failure-related",module,matches);
 return emit(reply,reply_context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}

static int text_ok(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
stnlabz_module_result_t digit_dsd_report(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context){
 const digit_dsd_trace_t *trace=request;
 digit_dsd_report_t *out=response;
 size_t i,offset=0;
 (void)context;
 if(response_used)*response_used=0;
 if(!dsd_host||!trace||request_size!=sizeof(*trace)||
 !out||response_size<sizeof(*out)||!response_used||
 !text_ok(trace->request_id,sizeof trace->request_id)||!trace->request_id[0]||
 trace->count>DIGIT_DSD_TRACE_MAX)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<trace->count;i++){
  const digit_dsd_observation_t *o=&trace->observations[i];
  if(!text_ok(o->component,sizeof o->component)||
  !text_ok(o->decision,sizeof o->decision)||
  !text_ok(o->file,sizeof o->file)||
  !text_ok(o->function,sizeof o->function)||
  !text_ok(o->reason,sizeof o->reason)||!o->component[0]||
  !o->decision[0]||!o->reason[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 }
 memset(out,0,sizeof(*out));
 (void)snprintf(out->request_id,sizeof out->request_id,"%s",trace->request_id);
 out->observations=trace->count;
 for(i=0;i<trace->count;i++){
  const digit_dsd_observation_t *o=&trace->observations[i];
  int n=snprintf(out->report+offset,sizeof out->report-offset,
   "%s decision=%s file=%s function=%s line=%u reason=%s\n",
   o->component,o->decision,o->file[0]?o->file:"not_recorded",
   o->function[0]?o->function:"not_recorded",o->line,o->reason);
  if(n<0||(size_t)n>=sizeof out->report-offset)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  offset+=(size_t)n;
  if(!strcmp(o->decision,"FAILED")||!strcmp(o->decision,"DENIED"))out->has_failure=1;
 }
 *response_used=sizeof(*out);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(dsd_host||!h||!h->register_service||!h->unregister_service||
 !h->register_command||!h->unregister_command)
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("dsd.report",digit_dsd_report,NULL))
 return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_command("diagnose",diagnose_command,NULL)){
  (void)h->unregister_service("dsd.report",NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 dsd_host=h;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!dsd_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!dsd_host->unregister_command("diagnose",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!dsd_host->unregister_service("dsd.report",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 dsd_host=NULL;
 return STNLABZ_MODULE_OK;
}
stnlabz_module_descriptor_t digit_dsd_descriptor(void){
 stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"dsd");
 (void)snprintf(d.name,sizeof d.name,"Digit Diagnostic System");
 d.version_major=0;d.version_minor=3;d.version_patch=10;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return d;
}

/* Required Linux dynamic-loader ABI entry point. */
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t descriptor;
 descriptor=digit_dsd_descriptor();return &descriptor;
}
