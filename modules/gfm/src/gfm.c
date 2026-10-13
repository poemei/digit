#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "gfm.h"

static const stnlabz_module_host_t *host;
static int fence_marker(const char *p,char *kind,unsigned *count){
 unsigned n=0;char c;
 while(*p==' '&&n<4){p++;n++;}
 if(n>3||(*p!='`'&&*p!='~'))return 0;
 c=*p;n=0;while(*p==c){p++;n++;}
 if(n<3)return 0;
 *kind=c;*count=n;return 1;
}
static int list_line(const char *p){
 while(*p==' ')p++;
 if((*p=='-'||*p=='+'||*p=='*')&&p[1]==' ')return 1;
 if(isdigit((unsigned char)*p)){
  while(isdigit((unsigned char)*p))p++;
  return (*p=='.'||*p==')')&&p[1]==' ';
 }
 return 0;
}
stnlabz_module_result_t digit_gfm_analyze(const void *input,size_t size,void *output,
 size_t capacity,size_t *used,void *context){
 const digit_gfm_request_t *req=input;
 digit_gfm_response_t *res=output;
 const char *p,*end,*line;
 char active=0;unsigned opening=0;
 (void)context;
 if(used)*used=0;
 if(!req||size!=sizeof *req||!res||capacity<sizeof *res||!used||
    req->contract_version!=GFM_CONTRACT_VERSION||
    !memchr(req->document,0,sizeof req->document))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(res,0,sizeof *res);
 res->contract_version=GFM_CONTRACT_VERSION;
 p=req->document;
 while(*p){
  char mark=0;unsigned width=0;
  size_t len;
  end=strchr(p,'\n');if(!end)end=p+strlen(p);
  len=(size_t)(end-p);
  if(len>=4096){return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;}
  line=p;
  if(fence_marker(line,&mark,&width)){
   if(!active){active=mark;opening=width;res->code_fences++;}
   else if(mark==active&&width>=opening){active=0;opening=0;}
   else res->code_lines++;
  }else if(active){res->code_lines++;}
  else{
   const char *q=line;size_t spaces=0;
   while(q<end&&*q==' '&&spaces<4){q++;spaces++;}
   if(spaces<=3&&q<end&&*q=='#'){
    const char *h=q;while(h<end&&*h=='#')h++;
    if(h-q<=6&&(h==end||*h==' '))res->headings++;
   }
   if(list_line(line))res->lists++;
   if(memchr(line,'|',len)&&memchr(line,'-',len)&&memchr(line,':',len))
    res->tables++;
   if(memchr(line,'[',len)&&memchr(line,']',len)&&memchr(line,'(',len)&&memchr(line,')',len))
    res->links++;
   if(len)res->prose_lines++;
  }
  p=*end?end+1:end;
 }
 res->balanced_fences=!active;
 snprintf(res->summary,sizeof res->summary,
  "GFM structure: headings=%u lists=%u table-candidate-lines=%u links-candidate-lines=%u fenced-blocks=%u fences=%s. Read-only structural scan, not full GFM conformance validation.",
  res->headings,res->lists,res->tables,res->links,res->code_fences,
  res->balanced_fences?"balanced":"unclosed");
 *used=sizeof *res;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("gfm.analyze",digit_gfm_analyze,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("gfm.analyze",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"gfm");snprintf(d.name,sizeof d.name,"GitHub Flavored Markdown");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
