#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <json-c/json.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "s2_research.h"
#define S2_HTTPBL_KEY "/opt/digit/auth/httpbl.key"
#define S2_HTTPBL_CONF "/opt/digit/modules/s2/httpbl.conf"
#define S2_MODULE_CONF "/opt/digit/modules/s2/module.conf"
static int read_key_file(const char *path,char *key,size_t cap)
{
 FILE *f;struct stat st;char line[256];int found=0;
 if(stat(path,&st)||!S_ISREG(st.st_mode)||(st.st_mode&077)||
    (st.st_uid!=geteuid()&&st.st_uid!=0))return 0;
 f=fopen(path,"r");if(!f)return 0;
 while(fgets(line,sizeof line,f)){
  char *s=line,*eq,*end;
  while(*s==' '||*s=='\t')s++;
  if(*s=='#'||*s=='\n'||!*s)continue;
  eq=strchr(s,'=');
  if(!eq){
   /* Root-owned OS file can contain only the plain twelve-character key. */
   char *end=s+strlen(s);size_t n,i;
   while(end>s&&(end[-1]=='\n'||end[-1]=='\r'||end[-1]==' '))*--end=0;
   n=strlen(s);
   if(found||n!=12U||n>=cap)break;
   for(i=0;i<n;++i)if(s[i]<'a'||s[i]>'z')break;
   if(i!=n)break;
   memcpy(key,s,n+1U);found=1;continue;
  }
  *eq++=0;end=eq+strlen(eq);
  while(end>eq&&(end[-1]=='\n'||end[-1]=='\r'||end[-1]==' '))*--end=0;
  if(!strcmp(s,"httpbl_key")||!strcmp(s,"key")){
   size_t n=strlen(eq),i;
   if(found||n!=12U||n>=cap)break;
   for(i=0;i<n;++i)if(eq[i]<'a'||eq[i]>'z')break;
   if(i!=n)break;
   memcpy(key,eq,n+1U);found=1;
  }
 }
 fclose(f);return found;
}
static int public_ip(const char *text,unsigned char octets[4])
{
 struct in_addr addr;unsigned long x;
 if(!text||inet_pton(AF_INET,text,&addr)!=1)return 0;
 memcpy(octets,&addr,4);
 x=((unsigned long)octets[0]<<24)|((unsigned long)octets[1]<<16)|
   ((unsigned long)octets[2]<<8)|octets[3];
 if(octets[0]==0||octets[0]==10||octets[0]==127||octets[0]>=224||
    (octets[0]==172&&octets[1]>=16&&octets[1]<=31)||
    (octets[0]==192&&octets[1]==168)||
    (octets[0]==169&&octets[1]==254)||
    (octets[0]==100&&octets[1]>=64&&octets[1]<=127)||
    (octets[0]==192&&octets[1]==0&&octets[2]==2)||
    (octets[0]==198&&octets[1]==51&&octets[2]==100)||
    (octets[0]==203&&octets[1]==0&&octets[2]==113))return 0;
 (void)x;return 1;
}
static int lookup(const char *key,const unsigned char octets[4],
                  unsigned *age,unsigned *score,unsigned *type)
{
 char host[128];struct addrinfo hints={0},*res=NULL,*it;int rc,found=0;
 snprintf(host,sizeof host,"%s.%u.%u.%u.%u.dnsbl.httpbl.org",
  key,octets[3],octets[2],octets[1],octets[0]);
 hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
 rc=getaddrinfo(host,NULL,&hints,&res);
 if(rc)return 0;
 for(it=res;it;it=it->ai_next){
  unsigned char a[4];
  if(it->ai_family!=AF_INET)continue;
  memcpy(a,&((struct sockaddr_in *)it->ai_addr)->sin_addr,4);
  if(a[0]!=127||a[3]>7)continue;
  *age=a[1];*score=a[2];*type=a[3];found=1;break;
 }
 freeaddrinfo(res);return found;
}
int s2_httpbl_review(const char *json,size_t length,char *out,size_t cap)
{
 struct json_tokener *tok=NULL;struct json_object *root=NULL,*rows=NULL;
 char key[32]={0};unsigned checked=0,listed=0,suspect=0,harvest=0,spam=0,search=0;
 size_t i,n;int written=0;
 if(!out||cap<300||!json||!length||length>8U*1024U*1024U)return 0;
 out[0]=0;
 if(!read_key_file(S2_HTTPBL_KEY,key,sizeof key)&&
    !read_key_file(S2_HTTPBL_CONF,key,sizeof key)&&
    !read_key_file(S2_MODULE_CONF,key,sizeof key)){
  snprintf(out,cap,"HTTP:BL research was not performed because the approved local access-key configuration is unavailable. No reputation verdict is inferred.");
  return 1;
 }
 tok=json_tokener_new();if(!tok)goto done;
 root=json_tokener_parse_ex(tok,json,(int)length);
 if(json_tokener_get_error(tok)!=json_tokener_success||!root||
    !json_object_object_get_ex(root,"threats",&rows)||
    !json_object_is_type(rows,json_type_array))goto done;
 n=json_object_array_length(rows);if(n>10000)goto done;
 for(i=0;i<n&&checked<3U;++i){
  struct json_object *row=json_object_array_get_idx(rows,i),*v=NULL;
  const char *ip;unsigned char oct[4];unsigned age=0,score=0,type=0;
  if(!row||!json_object_is_type(row,json_type_object))continue;
  if(!json_object_object_get_ex(row,"ip",&v)||
     !json_object_is_type(v,json_type_string))continue;
  ip=json_object_get_string(v);
  if(!public_ip(ip,oct))continue;
  checked++;
  if(!lookup(key,oct,&age,&score,&type))continue;
  listed++;
  if(type==0U)search++;
  else {if(type&1U)suspect++;if(type&2U)harvest++;if(type&4U)spam++;}
  (void)age;(void)score;
 }
 written=snprintf(out,cap,
  "Project Honey Pot HTTP:BL was checked against up to three public IPv4 addresses in this Sentinel snapshot. "
  "Addresses checked: %u; classified by HTTP:BL: %u. "
  "Of the classified responses: search-engine classification %u, suspicious-behavior indicator %u, "
  "email-harvesting indicator %u, comment-spam indicator %u. "
  "A search-engine classification is not a threat score. An unlisted or unresolved address is not a clean verdict. "
  "Shared hosting or proxy addresses require special care before attributing activity to an actor.",
  checked,listed,search,suspect,harvest,spam);
done:
 memset(key,0,sizeof key);
 if(root)json_object_put(root);
 if(tok)json_tokener_free(tok);
 return written>0&&(size_t)written<cap;
}
