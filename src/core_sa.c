#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core_sa.h"

/* [AI:GPT-6 | 2026-10-08] Core SA authorization; reject malformed, duplicate,
 * unprotected, or unauthorized SA records. General administrators are never
 * implicitly SAs. Read on each decision so deassignment takes effect.
 */
static int sa_ident(const char *s) {
    size_t i,n;
    if(!s)return 0;
    n=strnlen(s,64);
    if(!n||n>=64)return 0;
    for(i=0;i<n;++i)
        if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||
             (s[i]>='0'&&s[i]<='9')||s[i]=='-'||s[i]=='_'||s[i]=='.'))return 0;
    return 1;
}
int digit_core_sa_verify(const char *registry,const char *organization,
                             const char *identity) {
    FILE *f;
    struct stat st,after,named;
    int fd;
    char line[512];
    int found=0,authorized=0,invalid=0;
    if(!registry||!sa_ident(organization)||!sa_ident(identity)||
       strcmp(identity,"digit")==0)return 0;
    /* [AI:GPT-6 | 2026-10-08] 1.3.7: reject roster symlinks. */
    fd=open(registry,O_RDONLY|O_NOFOLLOW);
    if(fd<0)return 0;
    f=fdopen(fd,"r");
    if(!f){close(fd);return 0;}
    if(fstat(fileno(f),&st)!=0||!S_ISREG(st.st_mode)||st.st_nlink!=1||
       st.st_size<=0||st.st_size>8*1024*1024||
       (st.st_mode&077)!=0||(st.st_uid!=0 && st.st_uid!=geteuid())) {
        fclose(f);return 0;
    }
    while(fgets(line,sizeof(line),f)) {
        char *field[6],*p=line,*tab;
        size_t i,n=strlen(line);
        if(n==0||line[n-1]!='\n'){invalid=1;break;}
        line[n-1]='\0';
        for(i=0;i<6;++i) {
            field[i]=p;
            if(i==5)break;
            tab=strchr(p,'\t');
            if(!tab)break;
            *tab='\0';p=tab+1;
        }
        if(i!=5||strchr(field[5],'\t')||
           !sa_ident(field[0])||!sa_ident(field[1])||
           (strcmp(field[2],"SA")!=0 && strcmp(field[2],"ADMIN")!=0)||
           (strcmp(field[3],"0")!=0&&strcmp(field[3],"1")!=0)||
           (strcmp(field[4],"0")!=0&&strcmp(field[4],"1")!=0)||
           (strcmp(field[5],"0")!=0&&strcmp(field[5],"1")!=0)){
            invalid=1;break;
        }
        if(strcmp(field[0],identity)!=0 || strcmp(field[1],organization)!=0)continue;
        if(++found>1){invalid=1;break;}
        authorized=strcmp(field[2],"SA")==0 &&
                   strcmp(field[3],"1")==0 &&
                   strcmp(field[4],"1")==0 &&
                   strcmp(field[5],"1")==0;
    }
    /* [AI:GPT-6 | 2026-10-08] 1.3.7: roster replacement or
     * observable in-place change during evaluation denies access. */
    if(ferror(f) || fstat(fileno(f),&after)!=0 ||
       lstat(registry,&named)!=0 ||
       !S_ISREG(after.st_mode) || !S_ISREG(named.st_mode) ||
       after.st_nlink!=1 || named.st_nlink!=1 ||
       (after.st_mode&077)!=0 || (named.st_mode&077)!=0 ||
       (after.st_uid!=0 && after.st_uid!=geteuid()) ||
       (named.st_uid!=0 && named.st_uid!=geteuid()) ||
       after.st_dev!=st.st_dev || after.st_ino!=st.st_ino ||
       named.st_dev!=st.st_dev || named.st_ino!=st.st_ino ||
       after.st_size!=st.st_size || named.st_size!=st.st_size ||
       after.st_mtim.tv_sec!=st.st_mtim.tv_sec ||
       after.st_mtim.tv_nsec!=st.st_mtim.tv_nsec ||
       named.st_mtim.tv_sec!=st.st_mtim.tv_sec ||
       named.st_mtim.tv_nsec!=st.st_mtim.tv_nsec ||
       after.st_ctim.tv_sec!=st.st_ctim.tv_sec ||
       after.st_ctim.tv_nsec!=st.st_ctim.tv_nsec ||
       named.st_ctim.tv_sec!=st.st_ctim.tv_sec ||
       named.st_ctim.tv_nsec!=st.st_ctim.tv_nsec)invalid=1;
    if(fclose(f)!=0)invalid=1;
    return !invalid && found==1 && authorized;
}
