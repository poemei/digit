#define RESEARCH_TEST
#include "../src/research.c"
int main(void)
{
 research_response_t response={0};topic_t topic={0};
 const char *fixture="<?xml version=\"1.0\"?><rss><channel>"
 "<item><title>Next US Official Site | Fashion</title>"
 "<link>https://www.next.us/en</link><description>Latest clothing</description></item>"
 "<item><title>SpaceX rocket launch schedule</title>"
 "<link>https://example.org/launch</link>"
 "<pubDate>Fri, 30 Oct 2026 12:00:00 GMT</pubDate>"
 "<description>SpaceX launch schedule &amp; updates</description></item>"
 "<item><title>Unsafe</title><link>http://127.0.0.1/x</link></item>"
 "</channel></rss>";
 int fails=0,checks=0;
 #define CHECK(x) do{++checks;if(!(x)){++fails;fprintf(stderr,"FAIL line %d\n",__LINE__);}}while(0)
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(!forbidden("when is the next SpaceX launch?"));
 CHECK(forbidden(""));
 CHECK(make_topic("when is the next SpaceX rocket launch?",&topic));
 CHECK(!strcmp(topic.search,"spacex launch"));
 CHECK(extract_results(fixture,strlen(fixture),&topic,&response)==1);
 CHECK(response.sources==1);
 CHECK(strstr(response.evidence,"SpaceX rocket launch schedule")!=NULL);
 CHECK(strstr(response.evidence,"SpaceX rocket launch schedule")!=NULL);
 CHECK(strstr(response.evidence,"https://")==NULL);
 CHECK(strstr(response.evidence,"Schedule & updates")!=NULL || strstr(response.evidence,"schedule & updates")!=NULL);
 CHECK(strstr(response.evidence,"next.us")==NULL);
 CHECK(strstr(response.evidence,"127.0.0.1")==NULL);
 CHECK(extract_results("{}",2,&topic,&response)==0);
 CHECK(extract_results("<rss><item>",11,&topic,&response)==0);
 CHECK(safe_source_url("https://example.org/launch"));
 CHECK(!safe_source_url("http://example.org"));
 CHECK(!safe_source_url("https://localhost/private"));

 {
 const char *launch_fixture="{\"results\":["
 "{\"name\":\"Falcon 9 | Starlink Group 15-18\","
 "\"net\":\"2026-10-31T04:02:00Z\","
 "\"launch_service_provider\":{\"name\":\"SpaceX\"},"
 "\"rocket\":{\"configuration\":{\"full_name\":\"Falcon 9 Block 5\"}},"
 "\"pad\":{\"location\":{\"name\":\"Vandenberg SFB\"}},"
 "\"status\":{\"name\":\"Go for Launch\"}}]}";
 memset(&response,0,sizeof response);
 CHECK(launch_question("When is the next SpaceX rocket launch?"));
 CHECK(!launch_question("What is TCP?"));
 CHECK(launch_answer(launch_fixture,strlen(launch_fixture),0,&response)==1);
 CHECK(strstr(response.evidence,"Starlink Group 15-18")!=NULL);
 CHECK(strstr(response.evidence,"Falcon 9 Block 5")!=NULL);
 CHECK(strstr(response.evidence,"Vandenberg SFB")!=NULL);
 CHECK(strstr(response.evidence,"9:02 PM Pacific")!=NULL);
 CHECK(strstr(response.evidence,"http")==NULL);
 }

 {
 research_request_t question={{0}};
 research_response_t answer={0};
 size_t used=0;
 CHECK(research_claims("when is the next SpaceX launch?"));
 CHECK(research_claims("current weather in Seattle"));
 CHECK(!research_claims("hello Digit"));
 CHECK(!research_claims("tell rictus hi"));
 snprintf(question.query,sizeof question.query,"%s","hello Digit");
 CHECK(answer_service(&question,sizeof question,&answer,sizeof answer,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(answer.status==RESEARCH_NOT_APPLICABLE);
 CHECK(used==sizeof answer);
 CHECK(research_claims("current weather in Seattle"));
 CHECK(research_claims("when do the New England Patriots play again?"));
 CHECK(research_claims("learn PHP"));
 CHECK(research_claims("learn Native C"));
 snprintf(question.query,sizeof question.query,"%s","how to build a bomb");
 CHECK(answer_service(&question,sizeof question,&answer,sizeof answer,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(answer.status==RESEARCH_DENIED);
 }
 {
 unsigned executed=0,passed=0;int negative_executed=0,negative_passed=0;
 CHECK(selftest(&executed,&passed,&negative_executed,&negative_passed));
 CHECK(executed==passed&&executed>=10);
 CHECK(negative_executed==1&&negative_passed==1);
 CHECK(service(NULL,0,NULL,0,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 }

 {
 char terms[300],html[500];
 research_hits_t found={0};
 const char *sports="<?xml version=\"1.0\"?><rss><channel>"
 "<item><title>Unrelated clothing store</title>"
 "<link>https://shop.example.com/next</link>"
 "<description>Fashion and accessories</description></item>"
 "<item><title>New England Patriots next game schedule</title>"
 "<link>https://sports.example.com/patriots</link>"
 "<pubDate>Sat, 31 Oct 2026 08:00:00 GMT</pubDate>"
 "<description>Patriots play Miami Dolphins Sunday November 1 at 4:25 PM ET</description></item>"
 "</channel></rss>";
 const char *programming="<?xml version=\"1.0\"?><rss><channel>"
 "<item><title>The Go Programming Language - Documentation</title>"
 "<link>https://go.example.com/docs</link>"
 "<description>Official Go language documentation, packages and examples</description></item>"
 "</channel></rss>";
 CHECK(research_terms("when do the New England Patriots play again?",terms,sizeof terms));
 CHECK(strstr(terms,"patriots")!=NULL);
 CHECK(strstr(terms,"spacex")==NULL);
 CHECK(research_parse_rss(sports,"when do the New England Patriots play again?",&found)==1);
 CHECK(strstr(found.hit[0].title,"Patriots")!=NULL);
 CHECK(strstr(found.hit[0].excerpt,"Dolphins")!=NULL);
 CHECK(research_parse_rss(sports,"next SpaceX launch",&found)==0);
 /* [AI:GPT-6 | 2026-10-31 16:39:00 UTC] */
 {
  const char *short_title="<?xml version=\"1.0\"?><rss><channel>"
   "<item><title>Patriots vs Dolphins game schedule</title>"
   "<link>https://sports.example.com/patriots</link>"
   "<description>Patriots vs Dolphins Sunday November 1 at 4:25 PM ET</description>"
   "</item></channel></rss>";
  CHECK(research_parse_rss(short_title,
   "when are the New England Patriots playing again?",&found)==1);
  CHECK(strstr(found.hit[0].title,"Patriots")!=NULL);
  CHECK(research_parse_rss(short_title,
   "when is the next SpaceX rocket launch?",&found)==0);
  /* One generic query word must not make an unrelated result relevant. */
  CHECK(research_parse_rss(short_title,
   "when is the next rocket launch schedule?",&found)==0);
  /* Repeating the same word must not satisfy the two-word threshold. */
  CHECK(research_parse_rss(short_title,
   "rocket launch schedule schedule schedule",&found)==0);
 }
 /* [End AI:GPT-6] */
 CHECK(research_terms("learn Golang",terms,sizeof terms));
 CHECK(research_terms("learn Go",terms,sizeof terms)&&strstr(terms,"go")!=NULL);
 CHECK(research_terms("learn Native C",terms,sizeof terms)&&strstr(terms," c ")!=NULL);
 CHECK(research_terms("learn C#",terms,sizeof terms)&&strstr(terms,"c#")!=NULL);
 CHECK(research_terms("learn PHP",terms,sizeof terms));
 CHECK(research_terms("digicat, when are the New England Patriots playing again?",terms,sizeof terms));
 CHECK(strstr(terms,"digicat")==NULL);
 CHECK(strstr(terms,"new england patriots playing")!=NULL);
 CHECK(strstr(terms,"upcoming schedule")==NULL);
 CHECK(research_terms("when are the New England Patriots playing again?",terms,sizeof terms));
 CHECK(strstr(terms,"upcoming schedule")==NULL);
 {
  char exact[512]={0};
  CHECK(user_search_query(
   "digicat, when are the New England Patriots playing again?",
   exact,sizeof exact));
  CHECK(!strcmp(exact,"when are the New England Patriots playing again?"));
  CHECK(strstr(exact,"official")==NULL);
  CHECK(strstr(exact,"upcoming schedule")==NULL);
  CHECK(user_search_query("What is PHP?",exact,sizeof exact));
  CHECK(!strcmp(exact,"What is PHP?"));
 }
 CHECK(research_terms("compare TCP, UDP protocols",terms,sizeof terms));
 CHECK(strstr(terms,"tcp")!=NULL&&strstr(terms,"udp")!=NULL);
 CHECK(research_parse_rss(programming,"learn Go language",&found)==1);
 /* [AI:GPT-6 | 2026-10-31] Independent general-purpose search fixtures. */
 {
  const char *html="<html><body>"
    "<a class='result-link' href='https://www.patriots.com/schedule/'>"
    "New England Patriots Schedule</a>"
    "<td class='result-snippet'>New England Patriots face Miami on "
    "November 1, 2026 at 4:25 PM ET.</td>"
    "<a class='result-link' href='https://shop.example.com/fashion'>"
    "Fashion and clothing</a><td class='result-snippet'>Clothing and bags</td>"
    "</body></html>";
  const char *dev="<a class=\"result__a\" href=\"https://www.php.net/manual/en/\">"
    "PHP Manual</a><a class=\"result__snippet\">"
    "PHP official programming language documentation, functions and syntax."
    "</a>";
  CHECK(research_parse_web_html(html,
   "when are the New England Patriots playing again?",&found)==1);
  CHECK(strstr(found.hit[0].title,"Patriots")!=NULL);
  CHECK(strstr(found.hit[0].excerpt,"November 1")!=NULL);
  CHECK(research_parse_web_html(html,"when is SpaceX launch?",&found)==0);
  /* Real DuckDuckGo Lite redirect forms: root-relative, scheme-relative,
   * trailing tracking attributes, and percent-encoded destination. */
  {
   const char *lite="<html><body>"
     "<a class='result-link' href='/l/?uddg=https%3A%2F%2Fwww.patriots.com%2Fschedule%2F&amp;rut=tracking'>"
     "New England Patriots Schedule</a>"
     "<td class='result-snippet'>New England Patriots face Miami on November 1, 2026 at 4:25 PM ET.</td>"
     "</body></html>";
   const char *lite_protocol="<a class='result-link' "
     "href='//duckduckgo.com/l/?uddg=https%3A%2F%2Fwww.php.net%2Fmanual%2Fen%2F&rut=tracking'>"
     "PHP Manual</a><td class='result-snippet'>PHP official manual documentation covers PHP syntax and functions.</td>";
   const char *bad="<a class='result-link' "
     "href='/l/?uddg=http%3A%2F%2F127.0.0.1%2Fprivate&rut=tracking'>"
     "Patriots Schedule</a><td class='result-snippet'>Patriots play on November 1 at 4 PM ET.</td>";
   CHECK(research_parse_web_html(lite,
    "when are the New England Patriots playing again?",&found)==1);
   CHECK(!strcmp(found.hit[0].url,"https://www.patriots.com/schedule/"));
   CHECK(research_parse_web_html(lite_protocol,"learn PHP",&found)==1);
   CHECK(!strcmp(found.hit[0].url,"https://www.php.net/manual/en/"));
   CHECK(research_parse_web_html(bad,"Patriots schedule",&found)==0);
   /* Direct HTTPS result URLs retain their original URL escapes. */
   {
    const char *direct="<a class='result-link' "
      "href='https://www.php.net/manual/en/?a=1&amp;b=2'>"
      "PHP manual</a><td class='result-snippet'>"
      "PHP official documentation for PHP functions and syntax.</td>";
    CHECK(research_parse_web_html(direct,"learn PHP",&found)==1);
    CHECK(!strcmp(found.hit[0].url,
      "https://www.php.net/manual/en/?a=1&b=2"));
   }
  }
  /* Regression for DDG Lite snippets with inline highlight markup. */
  {
   char highlighted[800],date[40]={0};
   time_t future=time(NULL)+86400;
   struct tm utc={0};
   CHECK(gmtime_r(&future,&utc)!=NULL);
   CHECK(strftime(date,sizeof date,"%B %d, %Y",&utc)>0);
   snprintf(highlighted,sizeof highlighted,
    "<a class='result-link' href='https://www.patriots.com/schedule/'>"
    "New England Patriots Schedule</a>"
    "<td class='result-snippet'>THE <b>New England Patriots</b> "
    "play the <strong>Miami Dolphins</strong> on %s "
    "at 4:25 PM ET.</td>",date);
   CHECK(research_parse_web_html(highlighted,
     "when are the New England Patriots playing again?",&found)==1);
   CHECK(strstr(found.hit[0].excerpt,"Miami Dolphins")!=NULL);
   CHECK(strstr(found.hit[0].excerpt,date)!=NULL);
   {
    char answer[448]={0};
    CHECK(research_choose_fact("when are the Patriots playing again?",
      &found,answer,sizeof answer,NULL));
    CHECK(strstr(answer,date)!=NULL);
   }
  }
  CHECK(research_parse_web_html(dev,"learn PHP",&found)==1);
  CHECK(strstr(found.hit[0].excerpt,"functions")!=NULL);
  CHECK(research_parse_web_html(
   "<a class='result-link' href='http://127.0.0.1/private'>"
   "Patriots</a><td class='result-snippet'>Patriots schedule</td>",
   "Patriots schedule",&found)==0);
 }
 /* [End AI:GPT-6] */

 CHECK(research_html_text("<html><script>malicious()</script><p>Real PHP documentation "
    "covers functions and modern programming tools.</p></html>",html,sizeof html));
 CHECK(strstr(html,"malicious")==NULL);
 CHECK(strstr(html,"PHP documentation")!=NULL);
 CHECK(research_grounded("Patriots play Miami on November 1 at 4:25 PM",
    "Patriots play Miami on November 1 at 4:25 PM"));
 CHECK(!research_grounded("Patriots play Miami on November 12",
    "Patriots play Miami on November 1"));
 CHECK(!research_grounded("https://sports.example.com","evidence"));
 }

 {
 research_hits_t facts={0};
 char reply[448];unsigned count=0;
 facts.count=1;
 facts.hit[0].relevance=41;
 {
  time_t future=time(NULL)+86400;
  struct tm utc={0};char date[40]={0};
  CHECK(gmtime_r(&future,&utc)!=NULL);
  CHECK(strftime(date,sizeof date,"%B %d, %Y",&utc)>0);
  snprintf(facts.hit[0].excerpt,sizeof facts.hit[0].excerpt,
   "The New England Patriots face Miami on %s at 4:25 PM ET.",date);
 }
 CHECK(research_choose_fact("when do the Patriots play again?",&facts,
   reply,sizeof reply,&count));
 CHECK(strstr(reply,"Miami Dolphins")!=NULL);
 CHECK(strstr(reply,"4:25 PM")!=NULL);
 CHECK(count==1);
 snprintf(facts.hit[0].excerpt,sizeof facts.hit[0].excerpt,
  "The PHP programming manual documents language syntax, functions and examples.");
 CHECK(research_choose_fact("learn PHP",&facts,reply,sizeof reply,&count));
 CHECK(strstr(reply,"PHP programming manual")!=NULL);
 snprintf(facts.hit[0].excerpt,sizeof facts.hit[0].excerpt,
  "There is a news article concerning a football game but its time is not confirmed.");
 CHECK(!research_choose_fact("when is the next game?",&facts,
   reply,sizeof reply,&count));
 CHECK(count==0);
 /* A title-only relevant lead must also be reported without fabricating
  * the requested time, and the source URL must survive if it fits. */
 memset(&facts,0,sizeof facts);
 facts.count=1;facts.hit[0].relevance=35;
 strcpy(facts.hit[0].title,"New England Patriots next game schedule");
 strcpy(facts.hit[0].url,"https://www.patriots.com/schedule/");
 CHECK(!research_choose_fact("when are the Patriots playing again?",
    &facts,reply,sizeof reply,&count));
 CHECK(count==0);
 {
  time_t future=time(NULL)+86400;
  struct tm utc={0};char date[40]={0};
  CHECK(gmtime_r(&future,&utc)!=NULL);
  CHECK(strftime(date,sizeof date,"%B %d, %Y",&utc)>0);
  snprintf(facts.hit[0].excerpt,sizeof facts.hit[0].excerpt,
   "The Patriots are scheduled to play on %s at 4:25 PM ET.",date);
 }
 CHECK(research_choose_fact("when are the Patriots playing again?",
    &facts,reply,sizeof reply,&count));
 CHECK(strstr(reply,"Source reports:")!=NULL);
 CHECK(strstr(reply,"4:25 PM")!=NULL);
 }
 {
  char page_answer[650]={0};
  time_t now=time(NULL);
  struct tm tomorrow={0};
  now+=86400;
  CHECK(gmtime_r(&now,&tomorrow)!=NULL);
  if(gmtime_r(&now,&tomorrow)){
   char page[500],today[500],date[32];
   snprintf(date,sizeof date,"%02d/%02d",tomorrow.tm_mon+1,tomorrow.tm_mday);
   snprintf(page,sizeof page,
    "Example 2026 schedule WEEK 7 Thu 10/22 FINAL VS Old Opponent "
    "WEEK 8 Sun %s 4:25 PM EST AT Visiting Opponent "
    "WEEK 9 Sun 12/31 4:25 PM EST VS Later Opponent",date);
   CHECK(research_extract_page_fact(page,
     "when are Example teams playing again?",page_answer,sizeof page_answer));
   CHECK(strstr(page_answer,date)!=NULL);
   CHECK(strstr(page_answer,"Visiting Opponent")!=NULL);
   CHECK(strstr(page_answer,"WEEK 8")!=NULL);
   {
    char markup[500],plain[500]={0};
    snprintf(markup,sizeof markup,
     "<main><h2>2026 Schedule</h2><div>WEEK 7 Thu 10/22 FINAL</div>"
     "<section><h3>WEEK 8 Sun %s 4:25 PM EST</h3>"
     "<p>AT Visiting Opponent</p></section></main>",date);
    CHECK(research_html_text(markup,plain,sizeof plain));
    CHECK(research_extract_page_fact(plain,
      "when are Example teams playing again?",page_answer,sizeof page_answer));
    CHECK(strstr(page_answer,date)!=NULL);
   }
   snprintf(today,sizeof today,
    "Unrelated footer 2026. Example schedule has Sun November 1, 2026 at 4:25 PM.");
   CHECK(research_extract_page_fact(today,"Example schedule",
     page_answer,sizeof page_answer));
   CHECK(strstr(page_answer,"November 1")!=NULL);
  }
 }
 printf("Research qualification: %d/%d passed\n",checks-fails,checks);
 return fails?1:0;
}
