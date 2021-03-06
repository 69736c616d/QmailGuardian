#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "subfd.h"
#include "substdio.h"
#include "ip.h"
#include "ipme.h"
#include "exit.h"

char ipaddr[IPFMT];

int main()
{
 int j;

 switch (ipme_init()) {
   case  0: substdio_putsflush(subfderr,"out of memory\n"); _exit(111);
   case -1: substdio_putsflush(subfderr,"hard error\n"); _exit(100);
 }

 for (j = 0;j < ipme.len;++j) {
   switch(ipme.ix[j].af) {
     case AF_INET:
       substdio_put(subfdout,ipaddr,ip4_fmt(ipaddr,&ipme.ix[j].addr));
       break;
     case AF_INET6:
       substdio_put(subfdout,ipaddr,ip6_fmt(ipaddr,&ipme.ix[j].addr));
       break;
     default:
       substdio_puts(subfdout,"Unknown address family = ");
       substdio_put(subfdout,ipaddr,fmt_ulong(ipaddr,ipme.ix[j].af));
   }
   substdio_puts(subfdout,"\n");
 }

 substdio_flush(subfdout);
 _exit(0);
}
