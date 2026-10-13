#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "logging.h"

static FILE *digit_log_stream;
static FILE *module_log_stream;

static int open_pair(const char *core_path,const char *module_path)
{
    if(digit_log_stream||module_log_stream||!core_path||!module_path)return 0;
    digit_log_stream=fopen(core_path,"a");
    if(!digit_log_stream)return 0;
    module_log_stream=fopen(module_path,"a");
    if(!module_log_stream){
        (void)fclose(digit_log_stream);digit_log_stream=NULL;
        return 0;
    }
    (void)setvbuf(digit_log_stream,NULL,_IOLBF,0);
    (void)setvbuf(module_log_stream,NULL,_IOLBF,0);
    return 1;
}
int digit_log_is_open(void){return digit_log_stream&&module_log_stream;}

int digit_log_open_path(const char *path)
{
    char module_path[1024];
    const char *slash;
    int n;
    if(!path||path[0]!='/'||!(slash=strrchr(path,'/')))return 0;
    n=snprintf(module_path,sizeof module_path,"%.*s/modules.log",
        (int)(slash-path),path);
    if(n<0||(size_t)n>=sizeof module_path)return 0;
    return open_pair(path,module_path);
}
int digit_log_open(void)
{
    if(mkdir(DIGIT_LOG_DIRECTORY,0700)!=0&&errno!=EEXIST)return 0;
    return open_pair(DIGIT_LOG_PATH,DIGIT_MODULE_LOG_PATH);
}
static int is_module_component(const char *component)
{
    return !strcmp(component,"MODULE")||!strcmp(component,"DISCOVERY")||
        !strcmp(component,"ACTIVATION");
}
int digit_log_event(const char *component,const char *event,const char *detail)
{
    time_t now;
    struct tm utc;
    char stamp[32];
    FILE *destination;
    int written;
    if(!digit_log_is_open()||!component||!event)return 0;
    destination=is_module_component(component)?module_log_stream:digit_log_stream;
    now=time(NULL);
    if(now==(time_t)-1||!gmtime_r(&now,&utc)||
       !strftime(stamp,sizeof stamp,"%Y-%m-%dT%H:%M:%SZ",&utc))return 0;
    written=fprintf(destination,"%s %s %s%s%s\n",
        stamp,component,event,detail&&*detail?" ":"",detail&&*detail?detail:"");
    return written>0&&!ferror(destination);
}
int digit_log_sync(void)
{
    return digit_log_is_open()&&fflush(digit_log_stream)==0&&
        fsync(fileno(digit_log_stream))==0&&
        fflush(module_log_stream)==0&&fsync(fileno(module_log_stream))==0;
}
void digit_log_close(void)
{
    if(digit_log_stream){
        (void)fflush(digit_log_stream);(void)fclose(digit_log_stream);
        digit_log_stream=NULL;
    }
    if(module_log_stream){
        (void)fflush(module_log_stream);(void)fclose(module_log_stream);
        module_log_stream=NULL;
    }
}
