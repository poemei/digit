#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include "core_hotload.h"
#include "core_activation.h"
#include "logging.h"

/* Core owns module replacement; no module may grant its own activation. */
typedef struct {
 char id[STNLABZ_MODULE_ID_MAX];
 char sha[SHA256_DIGEST_LENGTH*2+1];
 char rejected_sha[SHA256_DIGEST_LENGTH*2+1];
 unsigned major,minor,patch;
} observed_t;
struct digit_hotload {
 char directory[1024];
 stnlabz_module_registry_t *registry;
 stnlabz_module_loader_t *loader;
 const stnlabz_module_host_t *host;
 digit_policy_request_t policy;
 observed_t *seen;
 size_t count,capacity;
};
static int digest(const char *path,char out[65]){
 unsigned char bytes[16384],hash[SHA256_DIGEST_LENGTH];
 SHA256_CTX ctx;ssize_t n;size_t i;int fd=open(path,O_RDONLY|O_CLOEXEC);
 if(fd<0)return 0;
 if(!SHA256_Init(&ctx)){close(fd);return 0;}
 while((n=read(fd,bytes,sizeof bytes))>0)SHA256_Update(&ctx,bytes,(size_t)n);
 if(n<0||!SHA256_Final(hash,&ctx)){close(fd);return 0;}
 close(fd);
 for(i=0;i<sizeof hash;i++)sprintf(out+2*i,"%02x",hash[i]);
 out[64]=0;return 1;
}
static observed_t *find(digit_hotload_t *w,const char *id){
 size_t i;for(i=0;i<w->count;i++)if(!strcmp(w->seen[i].id,id))return &w->seen[i];
 return NULL;
}
static int remember(digit_hotload_t *w,const char *id,const char *hash,
 const stnlabz_module_descriptor_t *d){
 observed_t *s=find(w,id);
 if(!s){
  observed_t *p;size_t cap;
  if(w->count==w->capacity){
   cap=w->capacity?w->capacity*2:16;
   p=realloc(w->seen,cap*sizeof *p);if(!p)return 0;
   w->seen=p;w->capacity=cap;
  }
  s=&w->seen[w->count++];memset(s,0,sizeof *s);
  snprintf(s->id,sizeof s->id,"%s",id);
 }
 snprintf(s->sha,sizeof s->sha,"%s",hash);
 s->major=d->version_major;s->minor=d->version_minor;s->patch=d->version_patch;
 return 1;
}
/* STN-LABZ Core owns lifecycle audit. Transport-specific publication
 * (including IRC and Slack) belongs exclusively to Digit modules.
 * Preserve replacement metadata and operational logging without dispatching
 * any mission-specific service from Core. */
static int log_report(digit_hotload_t *w,const char *event,const char *id,
 const observed_t *old,const stnlabz_module_descriptor_t *candidate,
 const char *before,const char *after,const char *reason,const char *recovery){
 char detail[760],active_version[32];
 int replacement_ok=!strcmp(event,"HOTLOAD_ACTIVE");
 int active_known=replacement_ok||
  (recovery&&(!strcmp(recovery,"incumbent_retained")||
               !strcmp(recovery,"incumbent_restored")));
 (void)w;
 if(replacement_ok&&candidate)
  snprintf(active_version,sizeof active_version,"%u.%u.%u",
   candidate->version_major,candidate->version_minor,candidate->version_patch);
 else if(active_known&&old)
  snprintf(active_version,sizeof active_version,"%u.%u.%u",
   old->major,old->minor,old->patch);
 else snprintf(active_version,sizeof active_version,"unknown");
 snprintf(detail,sizeof detail,
  "reported_event=%s module=%s active_version=%s incumbent_version=%u.%u.%u candidate_version=%u.%u.%u incumbent_sha256=%s candidate_sha256=%s reason=%s recovery=%s",
  event,id,active_version,old?old->major:0,old?old->minor:0,old?old->patch:0,
  candidate?candidate->version_major:0,candidate?candidate->version_minor:0,
  candidate?candidate->version_patch:0,before?before:"unknown",after?after:"unknown",
  reason,recovery);
 return digit_log_event("MODULE","HOTLOAD_NOTICE",detail)&&digit_log_sync();
}
static int newer(const observed_t *o,const stnlabz_module_descriptor_t *d){
 if(d->version_major!=o->major)return d->version_major>o->major;
 if(d->version_minor!=o->minor)return d->version_minor>o->minor;
 return d->version_patch>o->patch;
}
static int snapshot_candidate(const char *path,char *staged,size_t cap,
 char sha[65]){
 char buf[16384];int in,out;ssize_t n;size_t off;SHA256_CTX ctx;
 unsigned char hash[SHA256_DIGEST_LENGTH];size_t i;
 if(snprintf(staged,cap,"%s.candidate.XXXXXX",path)>=(int)cap)return 0;
 out=mkstemp(staged);if(out<0)return 0;
 in=open(path,O_RDONLY|O_CLOEXEC);
 if(in<0){close(out);unlink(staged);return 0;}
 if(!SHA256_Init(&ctx)){close(in);close(out);unlink(staged);return 0;}
 while((n=read(in,buf,sizeof buf))>0){
  SHA256_Update(&ctx,buf,(size_t)n);
  for(off=0;off<(size_t)n;){
   ssize_t k=write(out,buf+off,(size_t)n-off);
   if(k<=0){close(in);close(out);unlink(staged);return 0;}
   off+=(size_t)k;
  }
 }
 close(in);
 if(n<0||close(out)!=0||!SHA256_Final(hash,&ctx)){unlink(staged);return 0;}
 for(i=0;i<sizeof hash;i++)sprintf(sha+i*2,"%02x",hash[i]);
 sha[64]=0;return 1;
}
static void replacement(digit_hotload_t *w,const char *id,const char *path,
 observed_t *original){
 char staged[1120],sha[65];stnlabz_module_loader_t candidate;
 const stnlabz_module_descriptor_t *desc=NULL;
 const stnlabz_module_record_t *inc;
 stnlabz_module_record_t saved,trial;
 stnlabz_module_qualification_result_t q={0};
 stnlabz_module_result_t rc;
 stnlabz_loaded_module_t previous;
 size_t pos;int found=0,old_stopped=0;
 if(!snapshot_candidate(path,staged,sizeof staged,sha)){
  char detail[400];
  snprintf(detail,sizeof detail,"module=%s path=%.230s reason=candidate_snapshot_failed",id,path);
  (void)digit_log_event("MODULE","HOTLOAD_FAILED",detail);
  (void)digit_log_sync();
  return;
 }
 if(!strcmp(original->sha,sha)){unlink(staged);return;}
 stnlabz_module_loader_init(&candidate);
 if(stnlabz_module_loader_load(&candidate,id,staged,&desc)!=STNLABZ_MODULE_LOADER_OK){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,NULL,original->sha,sha,
   "candidate_load_failed","incumbent_retained");goto end;
 }
 if(!newer(original,desc)){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,desc,original->sha,sha,
   "sha256_changed_without_version_increment_or_version_regressed","incumbent_retained");
  goto end;
 }
 /* Qualify a separately loaded candidate before touching the incumbent. */
 stnlabz_module_registry_t isolated;
 stnlabz_module_registry_init(&isolated);
 rc=stnlabz_module_registry_discover(&isolated,desc);
 if(rc==STNLABZ_MODULE_OK)rc=stnlabz_module_registry_verify(&isolated,id);
 if(rc==STNLABZ_MODULE_OK)rc=stnlabz_module_registry_qualify(&isolated,id);
 if(rc==STNLABZ_MODULE_OK){
  const stnlabz_module_record_t *record=stnlabz_module_registry_find(&isolated,id);
  if(record)q=record->qualification;
 }
 stnlabz_module_registry_destroy(&isolated);
 if(rc!=STNLABZ_MODULE_OK){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,desc,original->sha,sha,
   "candidate_qualification_failed","incumbent_retained");goto end;
 }
 inc=stnlabz_module_registry_find(w->registry,id);
 if(!inc||inc->state!=STNLABZ_MODULE_STATE_ACTIVE){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,desc,original->sha,sha,
   "incumbent_not_active","incumbent_status_unverified");goto end;
 }
 saved=*inc;
 for(pos=0;pos<w->loader->count;pos++)
  if(!strcmp(w->loader->modules[pos].module_id,id)){found=1;break;}
 if(!found){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,desc,original->sha,sha,
   "incumbent_loader_missing","incumbent_status_unverified");goto end;
 }
 previous=w->loader->modules[pos];
 w->policy.module_id=id;
 /* Recheck the trusted authorization boundary before changing live state. */
 digit_policy_result_t decision;
 if(!digit_core_policy_evaluate(&w->policy,&decision)||decision.decision!=DIGIT_POLICY_ALLOW){
  (void)log_report(w,"HOTLOAD_DENIED",id,original,desc,original->sha,sha,
   "authorization_denied","incumbent_retained");goto end;
 }
 if(!digit_core_stop(w->registry,id)){
  (void)log_report(w,"HOTLOAD_FAILED",id,original,desc,original->sha,sha,
   "incumbent_stop_failed","incumbent_status_unverified");goto end;
 }
 old_stopped=1;
 /* Keep the old library loaded for rollback; replace only the registry record. */
 for(size_t j=0;j<w->registry->count;j++)
  if(!strcmp(w->registry->modules[j].descriptor.id,id)){
   trial=saved;trial.descriptor=*desc;trial.qualification=q;
   trial.state=STNLABZ_MODULE_STATE_QUALIFIED;trial.activation_authorized=0;
   w->registry->modules[j]=trial;break;
  }
 if(digit_core_authorize(w->registry,id,&w->policy)==STNLABZ_MODULE_OK &&
    digit_core_activate(w->registry,id,w->host,&w->policy)==STNLABZ_MODULE_OK){
  /* Retain incumbent handle until process shutdown: callbacks may be in flight. */
  w->loader->modules[pos]=candidate.modules[0];
  candidate.count=0;
  (void)remember(w,id,sha,desc);
  (void)log_report(w,"HOTLOAD_ACTIVE",id,original,desc,original->sha,sha,
   "qualification_and_activation_passed","replacement_active");
  (void)previous;
 }else{
  for(size_t j=0;j<w->registry->count;j++)
   if(!strcmp(w->registry->modules[j].descriptor.id,id)){
    w->registry->modules[j]=saved;
    w->registry->modules[j].state=STNLABZ_MODULE_STATE_STOPPED;
    w->registry->modules[j].activation_authorized=0;break;
   }
  rc=digit_core_authorize(w->registry,id,&w->policy);
  if(rc==STNLABZ_MODULE_OK)rc=digit_core_activate(w->registry,id,w->host,&w->policy);
  (void)log_report(w,"HOTLOAD_FAILED",id,original,desc,original->sha,sha,
    "candidate_activation_failed",rc==STNLABZ_MODULE_OK?
    "incumbent_restored":"incumbent_restore_failed");
 }
 (void)old_stopped;
end:
 stnlabz_module_loader_unload_all(&candidate);
 unlink(staged);
}
digit_hotload_t *digit_hotload_create(const char *directory,
 stnlabz_module_registry_t *registry,stnlabz_module_loader_t *loader,
 const stnlabz_module_host_t *host,const digit_policy_request_t *policy){
 digit_hotload_t *w;size_t i;
 if(!directory||!registry||!loader||!host||!policy)return NULL;
 w=calloc(1,sizeof *w);if(!w)return NULL;
 if(strlen(directory)>=sizeof w->directory){free(w);return NULL;}
 snprintf(w->directory,sizeof w->directory,"%s",directory);
 w->registry=registry;w->loader=loader;w->host=host;w->policy=*policy;
 /* Rictus pattern: Core polls active registry entries, not a fixed watch list.
    Remember installed binary hashes solely to enforce Digit's SHA/version rule. */
 {
  char detail[400];
  snprintf(detail,sizeof detail,"directory=%.310s status=ready registry_count=%zu",directory,registry->count);
  (void)digit_log_event("MODULE","HOTLOAD_WATCH_READY",detail);
  (void)digit_log_sync();
 }
 for(i=0;i<registry->count;i++){
  const stnlabz_module_record_t *r=&registry->modules[i];
  const stnlabz_loaded_module_t *m=stnlabz_module_loader_find(loader,r->descriptor.id);
  char sha[65],path[1200],detail[450];
  if(r->state!=STNLABZ_MODULE_STATE_ACTIVE||!m)continue;
  if(snprintf(path,sizeof path,"%s/%s/bin/%s.so",directory,
    r->descriptor.id,r->descriptor.id)>=(int)sizeof path)continue;
  if(!digest(path,sha)){
   snprintf(detail,sizeof detail,"module=%s reason=baseline_hash_unavailable path=%.250s",
     r->descriptor.id,path);
   (void)digit_log_event("MODULE","HOTLOAD_FAILED",detail);
  }else if(!remember(w,r->descriptor.id,sha,&r->descriptor)){
   free(w->seen);free(w);return NULL;
  }else{
   snprintf(detail,sizeof detail,"module=%s version=%u.%u.%u sha256=%s",
    r->descriptor.id,r->descriptor.version_major,r->descriptor.version_minor,
    r->descriptor.version_patch,sha);
   (void)digit_log_event("MODULE","HOTLOAD_WATCH",detail);
  }
 }
 (void)digit_log_sync();
 return w;
}
/* Admit a newly installed module without stopping Digit. Module manifest must
   identify its own directory; no module gains authority from installation alone. */
static void discover_new(digit_hotload_t *w){
 DIR *dir=opendir(w->directory);
 struct dirent *entry;
 if(!dir)return;
 while((entry=readdir(dir))!=NULL){
  char manifest[1200],binary[1200],id[STNLABZ_MODULE_ID_MAX];
  char staged[1400],sha[65],detail[500];
  FILE *config;
  size_t len;
  stnlabz_module_loader_t candidate;
  const stnlabz_module_descriptor_t *desc=NULL;
  stnlabz_module_result_t rc;
  stnlabz_module_record_t *record;
  struct stat st;
  int admitted=0;
  if(entry->d_name[0]=='.')continue;
  len=strlen(entry->d_name);
  if(!len||len>=sizeof id)continue;
  memcpy(id,entry->d_name,len+1);
  if(stnlabz_module_registry_find(w->registry,id))continue;
  if(snprintf(manifest,sizeof manifest,"%s/%s/module.conf",w->directory,id)>=(int)sizeof manifest ||
     snprintf(binary,sizeof binary,"%s/%s/bin/%s.so",w->directory,id,id)>=(int)sizeof binary)continue;
  if(stat(manifest,&st)||!S_ISREG(st.st_mode))continue;
  config=fopen(manifest,"r");
  if(!config)continue;
  {
   char line[128],expected[128];
   snprintf(expected,sizeof expected,"id=%s",id);
   if(!fgets(line,sizeof line,config)){fclose(config);continue;}
   line[strcspn(line,"\r\n")]=0;
   if(strcmp(line,expected)||fgetc(config)!=EOF){fclose(config);continue;}
  }
  fclose(config);
  if(stat(binary,&st)||!S_ISREG(st.st_mode))continue;
  if(!snapshot_candidate(binary,staged,sizeof staged,sha)){
   snprintf(detail,sizeof detail,"module=%s reason=new_module_snapshot_failed",id);
   (void)digit_log_event("MODULE","HOTLOAD_FAILED",detail);continue;
  }
  snprintf(detail,sizeof detail,"module=%s candidate_sha256=%s status=new_module",id,sha);
  (void)digit_log_event("MODULE","HOTLOAD_DETECTED",detail);
  stnlabz_module_loader_init(&candidate);
  if(stnlabz_module_loader_load(&candidate,id,staged,&desc)!=STNLABZ_MODULE_LOADER_OK){
   snprintf(detail,sizeof detail,"module=%s candidate_sha256=%s reason=candidate_load_failed",id,sha);
   (void)digit_log_event("MODULE","HOTLOAD_DENIED",detail);goto cleanup;
  }
  rc=stnlabz_module_registry_discover(w->registry,desc);
  if(rc==STNLABZ_MODULE_OK)rc=stnlabz_module_registry_verify(w->registry,id);
  if(rc==STNLABZ_MODULE_OK)rc=stnlabz_module_registry_qualify(w->registry,id);
  if(rc!=STNLABZ_MODULE_OK){
   snprintf(detail,sizeof detail,"module=%s candidate_sha256=%s reason=qualification_failed result=%d",id,sha,(int)rc);
   (void)digit_log_event("MODULE","HOTLOAD_DENIED",detail);goto rollback;
  }
  w->policy.module_id=id;
  rc=digit_core_authorize(w->registry,id,&w->policy);
  if(rc==STNLABZ_MODULE_OK)rc=digit_core_activate(w->registry,id,w->host,&w->policy);
  if(rc!=STNLABZ_MODULE_OK){
   snprintf(detail,sizeof detail,"module=%s candidate_sha256=%s reason=activation_failed result=%d",id,sha,(int)rc);
   (void)digit_log_event("MODULE","HOTLOAD_FAILED",detail);goto rollback;
  }
  /* Give ownership of the live handle to the existing loader. */
  if(w->loader->count==w->loader->capacity){
   size_t capacity=w->loader->capacity?w->loader->capacity*2:16;
   stnlabz_loaded_module_t *next=realloc(w->loader->modules,capacity*sizeof *next);
   if(!next){
    (void)digit_core_stop(w->registry,id);
    (void)digit_log_event("MODULE","HOTLOAD_FAILED","reason=loader_capacity_failed");
    goto rollback;
   }
   w->loader->modules=next;w->loader->capacity=capacity;
  }
  w->loader->modules[w->loader->count++]=candidate.modules[0];
  candidate.count=0;admitted=1;
  (void)remember(w,id,sha,desc);
  snprintf(detail,sizeof detail,"module=%s version=%u.%u.%u candidate_sha256=%s reason=new_module_activated",
   id,desc->version_major,desc->version_minor,desc->version_patch,sha);
  (void)log_report(w,"HOTLOAD_ACTIVE",id,NULL,desc,"none",sha,
   "new_module_activated","module_active");
rollback:
  if(!admitted){
   record=(stnlabz_module_record_t *)stnlabz_module_registry_find(w->registry,id);
   if(record&&record->state!=STNLABZ_MODULE_STATE_ACTIVE)
    (void)stnlabz_module_registry_unregister(w->registry,id);
  }
cleanup:
  stnlabz_module_loader_unload_all(&candidate);
  unlink(staged);
  (void)digit_log_sync();
 }
 closedir(dir);
}
void digit_hotload_poll(digit_hotload_t *w){
 size_t i;if(!w)return;
 discover_new(w);
 /* Rictus-style direct registry walk: newly active entries are visible immediately.
    Only the hash baseline is cached, never the set of modules being polled. */
 for(i=0;i<w->registry->count;i++){
  const stnlabz_module_record_t *r=&w->registry->modules[i];
  const stnlabz_loaded_module_t *loaded;
  observed_t *known,baseline;
  char path[1200],hash[65],detail[450];
  if(r->state!=STNLABZ_MODULE_STATE_ACTIVE)continue;
  loaded=stnlabz_module_loader_find(w->loader,r->descriptor.id);
  if(!loaded)continue;
  if(snprintf(path,sizeof path,"%s/%s/bin/%s.so",w->directory,
    r->descriptor.id,r->descriptor.id)>=(int)sizeof path)continue;
  known=find(w,r->descriptor.id);
  if(!digest(path,hash)){
   /* Failed probes are visible; a later successful probe can recover. */
   if(known&&strcmp(known->rejected_sha,"unreadable")){
    snprintf(detail,sizeof detail,"module=%s path=%.240s reason=candidate_hash_unavailable",
      r->descriptor.id,path);
    (void)digit_log_event("MODULE","HOTLOAD_FAILED",detail);
    (void)digit_log_sync();
    snprintf(known->rejected_sha,sizeof known->rejected_sha,"unreadable");
   }
   continue;
  }
  if(!known){
   /* Never infer a preexisting version from a newly deployed binary. */
   if(!remember(w,r->descriptor.id,hash,&r->descriptor))continue;
   snprintf(detail,sizeof detail,"module=%s version=%u.%u.%u sha256=%s status=baseline",
     r->descriptor.id,r->descriptor.version_major,r->descriptor.version_minor,
     r->descriptor.version_patch,hash);
   (void)digit_log_event("MODULE","HOTLOAD_WATCH",detail);
   (void)digit_log_sync();
   continue;
  }
  if(!strcmp(hash,known->sha))continue;
  if(!strcmp(hash,known->rejected_sha))continue;
  baseline=*known;
  snprintf(detail,sizeof detail,"module=%s version=%u.%u.%u incumbent_sha256=%s candidate_sha256=%s",
    baseline.id,baseline.major,baseline.minor,baseline.patch,baseline.sha,hash);
  (void)digit_log_event("MODULE","HOTLOAD_DETECTED",detail);
  (void)digit_log_sync();
  replacement(w,baseline.id,path,&baseline);
  known=find(w,baseline.id);
  if(known&&strcmp(known->sha,hash))
   snprintf(known->rejected_sha,sizeof known->rejected_sha,"%s",hash);
 }
}
void digit_hotload_destroy(digit_hotload_t *w){
 if(!w)return;free(w->seen);free(w);
}
