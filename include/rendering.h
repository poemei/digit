#ifndef DIGIT_RENDERING_H
#define DIGIT_RENDERING_H
#include "router.h"
#include "model_expression.h"
#include "dsd.h"
#define DIGIT_RENDER_TEXT_MAX 256
/* View input is assembled by a trusted caller from immutable controller/model results. */
typedef struct {
 digit_router_result_t route;
 int model_present;
 digit_expression_response_t model;
} digit_render_request_t;
typedef struct {
 digit_route_status_t route_status;
 digit_expression_status_t model_status;
 char request_id[DIGIT_ROUTER_REQUEST_ID_MAX];
 char text[DIGIT_RENDER_TEXT_MAX];
} digit_render_response_t;
/* View-only presentation for Router-selected diagnostic result. */
stnlabz_module_result_t digit_rendering_diagnostic(const digit_dsd_report_t *report,
 char *text,size_t capacity);
stnlabz_module_descriptor_t digit_rendering_descriptor(void);
stnlabz_module_result_t digit_rendering_qualify(stnlabz_module_qualification_result_t *result);
stnlabz_module_result_t digit_rendering_start(const stnlabz_module_host_t *host);
stnlabz_module_result_t digit_rendering_stop(void);
stnlabz_module_result_t digit_rendering_render(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context);
#endif
