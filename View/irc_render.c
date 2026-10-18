#include <stdio.h>
#include <string.h>
#include "irc_render.h"
/* IRC protocol line: <=510 payload bytes excluding CRLF, no control injection. */
int digit_irc_render(char *out,size_t cap,const char *target,const char *message){
 char clean[385];size_t i=0,j=0;int n;
 if(!out||!target||!message||!target[0]||strlen(target)>100)return 0;
 while(message[i]&&j<sizeof clean-1){
  unsigned char c=(unsigned char)message[i++];
  if(c=='\r'||c=='\n'||c==0x01||c<32||c==127)continue;
  clean[j++]=(char)c;
 }
 clean[j]=0;
 n=snprintf(out,cap,"PRIVMSG %s :%s\r\n",target,clean);
 return n>0&&(size_t)n<cap&&n<=512;
}
