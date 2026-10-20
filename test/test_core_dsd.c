/* Core-owned DSD regression: actual trace binding, evidence and assessment. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../core/core_dsd.h"
#include "../modules/dsd/dsd.h"
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
 assert(strstr(report.lines[3],"modules/conversation/conversation.c:157"));
 assert(strstr(report.lines[5],"no general response generator"));
 trace_query.trace_id=id;used=0;
 assert(core_dsd_dispatch(DIGIT_DSD_DIAGNOSE_SERVICE,&trace_query,sizeof trace_query,&trace,sizeof trace,&used)==STNLABZ_MODULE_OK);
 assert(used==sizeof trace&&trace.found&&trace.count>=3);
 assert(core_dsd_dispatch(DIGIT_DSD_BIND_SERVICE,&clear,sizeof clear,NULL,0,&used)==STNLABZ_MODULE_OK);
 puts("Core DSD request/assessment regression: PASS");
 return 0;
}
