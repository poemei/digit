/* Core-owned DSD regression: actual trace binding, evidence and assessment. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../core/core_dsd.h"
#include "../core/stnlabz_abi/dsd.h"
int main(void){
 uint64_t id=991,clear=0;size_t used=0;
 digit_dsd_record_t record={0};
 digit_dsd_report_request_t query={0};
 digit_dsd_report_result_t report={0};
 digit_dsd_query_t trace_query={0};
 digit_dsd_result_t trace={0};
 assert(core_dsd_handles(DIGIT_DSD_REPORT_SERVICE));
 assert(core_dsd_handles(DIGIT_DSD_CURRENT_SERVICE));
 assert(!core_dsd_handles("digit.conversation"));
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&id,sizeof id,NULL,0,&used)==STNLABZ_MODULE_OK);
 core_dsd_service_begin("digit.message",__LINE__);
 core_dsd_service_begin("digit.conversation",__LINE__);
 core_dsd_service_begin("digit.interpretation",__LINE__);
 record.trace_id=record.event.trace_id=id;
 record.event.span_id=3;record.event.line=157;
 record.event.result=0;
 strcpy(record.event.module,"conversation");
 strcpy(record.event.method,"process");
 strcpy(record.event.file,"modules/conversation/conversation.c");
 strcpy(record.event.operation,"participation");
 strcpy(record.event.status,"NO_GENERATOR");
 assert(core_dsd_dispatch(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,NULL,0,&used)==STNLABZ_MODULE_OK);
 core_dsd_service_result("digit.conversation",STNLABZ_MODULE_OK,0,__LINE__);
 core_dsd_service_result("digit.message",STNLABZ_MODULE_OK,0,__LINE__);
 query.trace_id=id;
 strcpy(query.origin,"Digit explain PHP");
 used=0;
 assert(core_dsd_dispatch(DIGIT_DSD_REPORT_SERVICE,&query,sizeof query,&report,sizeof report,&used)==STNLABZ_MODULE_OK);
 assert(used==sizeof report);
 assert(report.count>=6);
 assert(strstr(report.lines[0],"Digit explain PHP"));
 assert(strstr(report.lines[1],"digit.message -> digit.conversation -> digit.interpretation"));
 assert(strstr(report.lines[2],"Module: conversation"));
 assert(strstr(report.lines[3],"modules/conversation/conversation.c:157"));
 assert(strstr(report.lines[5],"no general response generator"));
 /* Recorded upstream rejection takes precedence over later IRC silence. */
 id=992;record.trace_id=record.event.trace_id=id;
 strcpy(record.event.operation,"response.view");
 strcpy(record.event.status,"INSUFFICIENT_EVIDENCE");
 record.event.line=201;record.event.result=2;
 assert(core_dsd_dispatch(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,NULL,0,&used)==STNLABZ_MODULE_OK);
 record.event.line=570;record.event.result=0;
 strcpy(record.event.module,"irc");
 strcpy(record.event.file,"modules/irc/irc.c");
 strcpy(record.event.method,"incoming");
 strcpy(record.event.status,"NO_RESPONSE");
 assert(core_dsd_dispatch(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,NULL,0,&used)==STNLABZ_MODULE_OK);
 query.trace_id=id;
 assert(core_dsd_dispatch(DIGIT_DSD_REPORT_SERVICE,&query,sizeof query,&report,sizeof report,&used)==STNLABZ_MODULE_OK);
 assert(strstr(report.lines[2],"Module: conversation"));
 assert(strstr(report.lines[3],"modules/conversation/conversation.c:201"));
 assert(strstr(report.lines[5],"insufficient evidence")!=NULL||
        strstr(report.lines[5],"adequate evidence")!=NULL);
 id=993;
 /* A handler returned NOT_FOUND: it was registered, so DSD must not
  * claim that the upstream service was absent from Core's registry. */
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&id,sizeof id,NULL,0,&used)==STNLABZ_MODULE_OK);
 core_dsd_service_begin("digit.gru.generate",__LINE__);
 core_dsd_service_result("digit.gru.generate",STNLABZ_MODULE_ERR_NOT_FOUND,0,__LINE__);
 query.trace_id=id;
 assert(core_dsd_dispatch(DIGIT_DSD_REPORT_SERVICE,&query,sizeof query,&report,sizeof report,&used)==STNLABZ_MODULE_OK);
 assert(strstr(report.lines[3],"service=digit.gru.generate")!=NULL);
 assert(strstr(report.lines[3],"HANDLER_NOT_FOUND")!=NULL);
 assert(strstr(report.lines[5],"registered service handler returned NOT_FOUND")!=NULL);
 id=994;
 /* Actual registry miss is separately recorded and attributed to the
  * precise named service, not a guessed upstream module. */
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&id,sizeof id,NULL,0,&used)==STNLABZ_MODULE_OK);
 core_dsd_service_begin("digit.absent",__LINE__);
 core_dsd_service_missing("digit.absent",__LINE__);
 query.trace_id=id;
 assert(core_dsd_dispatch(DIGIT_DSD_REPORT_SERVICE,&query,sizeof query,&report,sizeof report,&used)==STNLABZ_MODULE_OK);
 assert(strstr(report.lines[3],"service=digit.absent")!=NULL);
 assert(strstr(report.lines[3],"SERVICE_NOT_FOUND")!=NULL);
 assert(strstr(report.lines[5],"registry lookup found no handler")!=NULL);
 id=995;
 /* IRC probes Socials family, which may decline unrelated conversation.
  * Its NOT_FOUND is a normal fall-through when subsequent GRU succeeds. */
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&id,sizeof id,NULL,0,&used)==STNLABZ_MODULE_OK);
 core_dsd_service_begin("digit.socials.family",__LINE__);
 core_dsd_service_result("digit.socials.family",STNLABZ_MODULE_ERR_NOT_FOUND,0,__LINE__);
 core_dsd_service_begin("digit.gru.generate",__LINE__);
 core_dsd_service_result("digit.gru.generate",STNLABZ_MODULE_OK,30,__LINE__);
 query.trace_id=id;
 assert(core_dsd_dispatch(DIGIT_DSD_REPORT_SERVICE,&query,sizeof query,&report,sizeof report,&used)==STNLABZ_MODULE_OK);
 assert(strstr(report.lines[3],"service=digit.gru.generate")!=NULL);
 assert(strstr(report.lines[3],"RESPONSE_RETURNED")!=NULL);
 assert(strstr(report.lines[5],"response was produced")!=NULL);
 id=991;
 trace_query.trace_id=id;used=0;
 assert(core_dsd_dispatch(DIGIT_DSD_DIAGNOSE_SERVICE,&trace_query,sizeof trace_query,&trace,sizeof trace,&used)==STNLABZ_MODULE_OK);
 assert(used==sizeof trace&&trace.found&&trace.count>=3);
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&clear,sizeof clear,NULL,0,&used)==STNLABZ_MODULE_OK);
 puts("Core DSD request/assessment regression: PASS");
 return 0;
}
