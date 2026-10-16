#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include "corpus_store.h"
/* Operator-side utility: never exposed as an IRC/Slack or public service. */
static int field_ok(const char *s,size_t max){
 size_t n=0;
 if(!s||!*s)return 0;
 for(;s[n];n++)if((unsigned char)s[n]<32 || (unsigned char)s[n]==127 || s[n]=='\t')return 0;
 return n>0&&n<max;
}
static char *trim(char *s){
 size_t n;
 while(*s==' '||*s=='\r')s++;
 n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;
 return s;
}
static int parse_line(char *line,const char *source,char **category,char **origin,char **text){
 char *p=trim(line),*a,*b;
 if(!*p||*p=='#')return 0;
 if(!strncmp(p,"digit learn ",11))p+=11;
 else if(!strncmp(p,"learn ",6))p+=6;
 else { *category="DOCUMENT";*origin=(char *)source;*text=p;return 1; }
 a=strchr(p,'|');if(!a)return -1;*a++=0;
 b=strchr(a,'|');if(!b)return -1;*b++=0;
 *category=trim(p);*origin=trim(a);*text=trim(b);
 return 1;
}
static int append_one(int fd,const char *category,const char *source,const char *text){
 unsigned char bytes[16];char id[33],line[8192];size_t n=0,i;
 int rnd=open("/dev/urandom",O_RDONLY|O_CLOEXEC),size;
 if(rnd<0)return 0;
 while(n<sizeof bytes){ssize_t x=read(rnd,bytes+n,sizeof bytes-n);
  if(x<=0){close(rnd);return 0;}n+=(size_t)x;}
 close(rnd);
 for(i=0;i<sizeof bytes;i++)snprintf(id+2*i,3,"%02x",bytes[i]);
 id[32]=0;
 size=snprintf(line,sizeof line,"LEARN-%s\t%s\t%s\t%s\n",id,category,source,text);
 if(size<0||(size_t)size>=sizeof line)return 0;
 for(n=0;n<(size_t)size;){ssize_t w=write(fd,line+n,(size_t)size-n);
  if(w<0&&errno==EINTR)continue;
  if(w<=0)return 0;n+=(size_t)w;}
 return 1;
}
int main(int argc,char **argv){
 int sourcefd=-1,storefd=-1,status=1,accepted=0,rejected=0;
 struct stat st;FILE *in=NULL;char *line=NULL;size_t cap=0,total=0;
 off_t before=0;ssize_t n;
 const char *origin;
 if(argc!=2){fprintf(stderr,"Usage: corpus-import FILE\n");return 2;}
 origin=strrchr(argv[1],'/');origin=origin?origin+1:argv[1];
 if(!field_ok(origin,DIGIT_CORPUS_SOURCE_MAX))return 2;
 sourcefd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
 if(sourcefd<0||fstat(sourcefd,&st)||!S_ISREG(st.st_mode)||
    st.st_size>8388608){perror("input rejected");goto done;}
 in=fdopen(sourcefd,"r");if(!in)goto done;sourcefd=-1;
 /* Validate entire input before touching the append-only store. */
 while((n=getline(&line,&cap,in))>=0){
  char *category,*source,*text;int parsed;
  if((total+=(size_t)n)>8388608||n>8192){fprintf(stderr,"Input too large\n");goto done;}
  parsed=parse_line(line,origin,&category,&source,&text);
  if(parsed<0|| (parsed>0&&(!field_ok(category,DIGIT_CORPUS_CATEGORY_MAX)||
    !field_ok(source,DIGIT_CORPUS_SOURCE_MAX)||
    !field_ok(text,DIGIT_CORPUS_TEXT_MAX)))){rejected++;goto done;}
  if(parsed>0)accepted++;
 }
 if(ferror(in)||!accepted)goto done;
 rewind(in);free(line);line=NULL;cap=0;
 storefd=open(DIGIT_CORPUS_PATH,O_WRONLY|O_APPEND|O_CLOEXEC|O_NOFOLLOW);
 if(storefd<0||fstat(storefd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||
    flock(storefd,LOCK_EX)){perror("corpus store unavailable");goto done;}
 before=lseek(storefd,0,SEEK_END);if(before<0)goto done;
 accepted=0;
 while((n=getline(&line,&cap,in))>=0){
  char *category,*source,*text;int parsed=parse_line(line,origin,&category,&source,&text);
  if(parsed>0){
   if(!append_one(storefd,category,source,text))goto rollback;
   accepted++;
  }
 }
 if(ferror(in)||fsync(storefd))goto rollback;
 printf("Imported: %d; rejected: 0; source file: %s\n",accepted,origin);
 status=0;goto done;
rollback:
 /* Keep the original corpus untouched on a partial batch failure. */
 (void)ftruncate(storefd,before);
 (void)fsync(storefd);
done:
 if(status)fprintf(stderr,"Import failed: no successful batch commit (invalid lines: %d)\n",rejected);
 free(line);if(in)fclose(in);if(sourcefd>=0)close(sourcefd);
 if(storefd>=0){(void)flock(storefd,LOCK_UN);close(storefd);}
 return status;
}
