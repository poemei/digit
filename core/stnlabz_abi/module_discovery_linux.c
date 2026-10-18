/*
 * STN-LABZ Module ABI 1.4
 * Linux dynamic module discovery.
 */

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "module_discovery.h"

#define STNLABZ_DISCOVERY_PATH_MAX 1024
#define STNLABZ_MODULE_CONF_NAME "module.conf"
#define STNLABZ_MODULE_BIN_DIR "bin"

static int stnlabz_discovery_join_path(char *output,size_t output_size,const char *left,const char *right)
{
    int written;
    if(output==NULL||left==NULL||right==NULL||output_size==0)return 0;
    written=snprintf(output,output_size,"%s/%s",left,right);
    return written>=0&&(size_t)written<output_size;
}

static int stnlabz_discovery_read_module_id(const char *config_path,char *module_id,size_t module_id_size)
{
    FILE *file;char line[512];int id_seen=0;
    if(config_path==NULL||module_id==NULL||module_id_size==0)return 0;
    module_id[0]='\0';file=fopen(config_path,"r");if(file==NULL)return 0;
    while(fgets(line,sizeof(line),file)!=NULL)
    {
        char *separator;char *value;size_t length=strlen(line);
        while(length>0&&(line[length-1]=='\n'||line[length-1]=='\r'))line[--length]='\0';
        if(line[0]=='\0'||line[0]=='#')continue;
        separator=strchr(line,'=');if(separator==NULL){fclose(file);return 0;}
        *separator='\0';value=separator+1;
        if(strcmp(line,"id")!=0||id_seen){fclose(file);return 0;}
        length=strlen(value);if(length==0||length>=module_id_size){fclose(file);return 0;}
        memcpy(module_id,value,length+1);id_seen=1;
    }
    fclose(file);return id_seen;
}

static int stnlabz_name_compare(const void *left,const void *right)
{
    const char *const *left_name=left;const char *const *right_name=right;return strcmp(*left_name,*right_name);
}

static void stnlabz_discovery_free_names(char **names,size_t count)
{
    size_t index;for(index=0;index<count;++index)free(names[index]);free(names);
}

static void stnlabz_discovery_copy(char *destination,size_t destination_size,const char *source)
{
    if(destination==NULL||destination_size==0)return;
    if(source==NULL)source="unknown";
    (void)snprintf(destination,destination_size,"%s",source);
}

static void stnlabz_discovery_report_rejection(stnlabz_module_discovery_report_t *report,const char *directory_name,const char *module_id,const char *stage,const char *reason)
{
    const char *resolved_module=(module_id!=NULL&&module_id[0]!='\0')?module_id:"unknown";
    stnlabz_discovery_copy(report->rejected_directory,sizeof(report->rejected_directory),directory_name);
    stnlabz_discovery_copy(report->rejected_module,sizeof(report->rejected_module),resolved_module);
    stnlabz_discovery_copy(report->rejection_stage,sizeof(report->rejection_stage),stage);
    stnlabz_discovery_copy(report->rejection_reason,sizeof(report->rejection_reason),reason);
    fprintf(stderr,"[MODULE] REJECTED: directory=%s module=%s stage=%s reason=%s -- Core continuing\n",
            directory_name!=NULL?directory_name:"unknown",resolved_module,stage!=NULL?stage:"unknown",reason!=NULL?reason:"unknown");
}

stnlabz_module_result_t stnlabz_module_discovery_get_path(char *modules_path,size_t modules_path_size)
{
    char executable_path[STNLABZ_DISCOVERY_PATH_MAX];char *separator;ssize_t length;int written;
    if(modules_path==NULL||modules_path_size==0)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    length=readlink("/proc/self/exe",executable_path,sizeof(executable_path)-1);
    if(length<=0||(size_t)length>=sizeof(executable_path)-1)return STNLABZ_MODULE_ERR_NOT_FOUND;
    executable_path[length]='\0';separator=strrchr(executable_path,'/');if(separator==NULL)return STNLABZ_MODULE_ERR_NOT_FOUND;
    *separator='\0';written=snprintf(modules_path,modules_path_size,"%s/modules",executable_path);
    if(written<0||(size_t)written>=modules_path_size)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    return STNLABZ_MODULE_OK;
}

stnlabz_module_result_t stnlabz_module_discovery_scan(stnlabz_module_registry_t *registry,stnlabz_module_loader_t *loader,const char *modules_path,stnlabz_module_discovery_report_t *report)
{
    DIR *directory;struct dirent *entry;char **names=NULL;size_t name_count=0;size_t index;
    if(registry==NULL||loader==NULL||modules_path==NULL||report==NULL)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(report,0,sizeof(*report));directory=opendir(modules_path);if(directory==NULL)return STNLABZ_MODULE_ERR_NOT_FOUND;
    while((entry=readdir(directory))!=NULL)
    {
        char directory_path[STNLABZ_DISCOVERY_PATH_MAX];struct stat status;char **expanded;size_t length;
        if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0)continue;
        if(!stnlabz_discovery_join_path(directory_path,sizeof(directory_path),modules_path,entry->d_name))continue;
        if(stat(directory_path,&status)!=0||!S_ISDIR(status.st_mode))continue;
        length=strlen(entry->d_name);expanded=realloc(names,(name_count+1)*sizeof(*names));
        if(expanded==NULL){closedir(directory);stnlabz_discovery_free_names(names,name_count);return STNLABZ_MODULE_ERR_REGISTRY_FULL;}
        names=expanded;names[name_count]=malloc(length+1);
        if(names[name_count]==NULL){closedir(directory);stnlabz_discovery_free_names(names,name_count);return STNLABZ_MODULE_ERR_REGISTRY_FULL;}
        memcpy(names[name_count],entry->d_name,length+1);++name_count;
    }
    closedir(directory);qsort(names,name_count,sizeof(*names),stnlabz_name_compare);
    for(index=0;index<name_count;++index)
    {
        char directory_path[STNLABZ_DISCOVERY_PATH_MAX];char config_path[STNLABZ_DISCOVERY_PATH_MAX];char bin_path[STNLABZ_DISCOVERY_PATH_MAX];char so_name[STNLABZ_MODULE_ID_MAX+4];char so_path[STNLABZ_DISCOVERY_PATH_MAX];char module_id[STNLABZ_MODULE_ID_MAX];const stnlabz_module_descriptor_t *descriptor=NULL;stnlabz_module_loader_result_t loader_result;stnlabz_module_result_t registry_result;int written;
        module_id[0]='\0';report->directories_examined++;
        if(!stnlabz_discovery_join_path(directory_path,sizeof(directory_path),modules_path,names[index])||!stnlabz_discovery_join_path(config_path,sizeof(config_path),directory_path,STNLABZ_MODULE_CONF_NAME))
        {stnlabz_discovery_report_rejection(report,names[index],NULL,"path","module path too long or invalid");report->modules_rejected++;continue;}
        if(!stnlabz_discovery_read_module_id(config_path,module_id,sizeof(module_id)))
        {stnlabz_discovery_report_rejection(report,names[index],NULL,"manifest","module.conf missing, unreadable, or invalid");report->modules_rejected++;continue;}
        written=snprintf(so_name,sizeof(so_name),"%s.so",module_id);
        if(written<0||(size_t)written>=sizeof(so_name)||!stnlabz_discovery_join_path(bin_path,sizeof(bin_path),directory_path,STNLABZ_MODULE_BIN_DIR)||!stnlabz_discovery_join_path(so_path,sizeof(so_path),bin_path,so_name))
        {stnlabz_discovery_report_rejection(report,names[index],module_id,"binary_path","module binary path too long or invalid");report->modules_rejected++;continue;}
        loader_result=stnlabz_module_loader_load(loader,module_id,so_path,&descriptor);
        if(loader_result!=STNLABZ_MODULE_LOADER_OK)
        {stnlabz_discovery_report_rejection(report,names[index],module_id,"loader",stnlabz_module_loader_result_string(loader_result));report->modules_rejected++;continue;}
        report->modules_loaded++;registry_result=stnlabz_module_registry_discover(registry,descriptor);
        if(registry_result!=STNLABZ_MODULE_OK)
        {stnlabz_discovery_report_rejection(report,names[index],module_id,"registry",stnlabz_module_result_string(registry_result));(void)stnlabz_module_loader_unload(loader,module_id);report->modules_rejected++;continue;}
        report->modules_discovered++;
    }
    stnlabz_discovery_free_names(names,name_count);return STNLABZ_MODULE_OK;
}
