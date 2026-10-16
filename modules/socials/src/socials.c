#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "socials.h"

/* Nonverbal Unicode resources reside solely in this module.
 * No nicknames, wording, scripted responses, or conversation admission. */
static const char *const hearts[] = {"❤️","🩷","🧡","💛","💚","💙","💜","🤍","💕","💖","💗","💞"};
static const char *const smiles[] = {"😊","🥰","☺️","🤗","😍","😄","😁","😌"};
static const char *const playful[] = {"😜","😋","🤭","🤪","😂","😏","🙃"};
static const char *const gentle[] = {"🥹","🫣","🫶","🌸","🎀","🦋"};
static const char *const celebrate[] = {"✨","🌟","🎉","🎊","💫","🌈"};
static const stnlabz_module_host_t *host;
static int active;

static uint32_t context_hash(const char *s, uint32_t category) {
 uint32_t h = UINT32_C(2166136261) ^ category;
 while (*s) { h ^= (unsigned char)*s++; h *= UINT32_C(16777619); }
 return h;
}
static stnlabz_module_result_t select_emoji(
 const void *request, size_t request_size, void *response,
 size_t response_size, size_t *response_used, void *ctx) {
 const digit_socials_request_t *q=request;
 digit_socials_response_t *r=response;
 const char *const *pool=NULL; size_t count=0, n;
 uint32_t category;
 (void)ctx;
 if(response_used)*response_used=0;
 if(!active) return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!q || request_size!=sizeof *q || !r || response_size<sizeof *r || !response_used ||
    q->version!=DIGIT_SOCIALS_VERSION ||
    !memchr(q->context,0,sizeof q->context) || !q->context[0] ||
    q->category>DIGIT_SOCIALS_CELEBRATE)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 category=q->category;
 if(category==DIGIT_SOCIALS_ANY)
   category=1U+(context_hash(q->context,0)%DIGIT_SOCIALS_CELEBRATE);
 switch(category) {
 case DIGIT_SOCIALS_HEART: pool=hearts;count=sizeof hearts/sizeof *hearts;break;
 case DIGIT_SOCIALS_SMILE: pool=smiles;count=sizeof smiles/sizeof *smiles;break;
 case DIGIT_SOCIALS_PLAYFUL: pool=playful;count=sizeof playful/sizeof *playful;break;
 case DIGIT_SOCIALS_GENTLE: pool=gentle;count=sizeof gentle/sizeof *gentle;break;
 case DIGIT_SOCIALS_CELEBRATE: pool=celebrate;count=sizeof celebrate/sizeof *celebrate;break;
 default:return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 }
 n=context_hash(q->context,category)%count;
 memset(r,0,sizeof *r);
 r->version=DIGIT_SOCIALS_VERSION;r->category=category;
 snprintf(r->emoji,sizeof r->emoji,"%s",pool[n]);
 *response_used=sizeof *r;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q) {
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h) {
 if(active||!h||!h->register_service||!h->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service(DIGIT_SOCIALS_SERVICE,select_emoji,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;active=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) {
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_SOCIALS_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 active=0;host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"socials");
 snprintf(d.name,sizeof d.name,"Digit Nonverbal Social Expressions");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
