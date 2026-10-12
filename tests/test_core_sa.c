#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core_sa.h"

/* [AI:GPT-6 | 2026-10-08] SA verification negative qualification. */
static int tests=0,failed=0;
static void check(int ok,const char *name) {
    ++tests;
    if(ok)printf("PASS %02d - %s\n",tests,name);
    else {++failed;printf("FAIL %02d - %s\n",tests,name);}
}
static int write_roles(const char *path,const char *data) {
    FILE *f=fopen(path,"w");int ok;
    if(!f)return 0;
    ok=fputs(data,f)>=0;
    if(fclose(f)!=0)return 0;
    return ok;
}
int main(void) {
    char path[]="/tmp/digit-sa-XXXXXX";
    int fd=mkstemp(path);
    if(fd<0)return 1;
    close(fd);
    chmod(path,0600);
    check(write_roles(path,"sysadmin\tstn-labz\tSA\t1\t1\t1\nregular\tstn-labz\tADMIN\t1\t1\t1\n"),"valid roster created");
    check(digit_core_sa_verify(path,"stn-labz","sysadmin"),"assigned qualified SA allowed");
    check(!digit_core_sa_verify(path,"stn-labz","regular"),"regular admin denied");
    check(!digit_core_sa_verify(path,"other-org","sysadmin"),"other organization denied");
    check(!digit_core_sa_verify(path,"stn-labz","missing"),"unknown user denied");
    check(!digit_core_sa_verify(path,"stn-labz","digit"),"Digit not human SA");
    check(!digit_core_sa_verify(NULL,"stn-labz","sysadmin"),"missing registry denied");
    chmod(path,0644);
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"unsafe permissions denied");
    chmod(path,0600);
    write_roles(path,"sysadmin\tstn-labz\tSA\t0\t1\t1\n");
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"unqualified SA denied");
    write_roles(path,"sysadmin\tstn-labz\tSA\t1\t0\t1\n");
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"unassigned SA denied");
    write_roles(path,"sysadmin\tstn-labz\tSA\t1\t1\t0\n");
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"mission unqualified SA denied");
    write_roles(path,"sysadmin\tstn-labz\tSA\t1\t1\t1\nsysadmin\tstn-labz\tSA\t1\t1\t1\n");
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"duplicate SA assignment denied");
    write_roles(path,"sysadmin\tstn-labz\tSA\t1\t1\t1\ninvalid\trow\n");
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"malformed roster denied");
    unlink(path);
    check(!digit_core_sa_verify(path,"stn-labz","sysadmin"),"missing roster denied");
    printf("Core SA verification tests: %d executed, %d failed\n",tests,failed);
    return failed?1:0;
}
