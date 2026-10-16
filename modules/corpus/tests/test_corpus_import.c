#define main corpus_import_program
#include "../src/corpus_import.c"
#undef main
#include <assert.h>
int main(void){
 char a[]="digit learn CONVERSATION | STN-LABZ | A greeting may invite a response.\n";
 char b[]="PHP is a server-side scripting language.\n";
 char c[]="# Section heading\n",d[]="digit learn INVALID LINE\n";
 char e[]="learn SOCIAL | STN-LABZ | Coffee is a beverage.\n";
 char f[]=" \r\n";char *category=NULL,*origin=NULL,*text=NULL;
 assert(parse_line(a,"learning.txt",&category,&origin,&text)==1);
 assert(!strcmp(category,"CONVERSATION"));
 assert(!strcmp(origin,"STN-LABZ"));
 assert(!strcmp(text,"A greeting may invite a response."));
 assert(parse_line(b,"php.txt",&category,&origin,&text)==1);
 assert(!strcmp(category,"DOCUMENT"));
 assert(!strcmp(origin,"php.txt"));
 assert(!strcmp(text,"PHP is a server-side scripting language."));
 assert(parse_line(c,"doc.txt",&category,&origin,&text)==0);
 assert(parse_line(d,"doc.txt",&category,&origin,&text)==-1);
 assert(parse_line(e,"doc.txt",&category,&origin,&text)==1);
 assert(!strcmp(category,"SOCIAL"));
 assert(parse_line(f,"doc.txt",&category,&origin,&text)==0);
 assert(!field_ok("bad\tvalue",64));
 assert(!field_ok("",64));
 assert(field_ok("STN-LABZ",64));
 puts("Corpus import parsing: 16 assertions PASS");
 return 0;
}
