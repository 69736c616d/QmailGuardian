#include "cdb.h"
#include "byte.h"
#include "open.h"
#include "error.h"
#include "control.h"
#include "constmap.h"
#include "stralloc.h"
#include "recipients.h"
#include "strerr.h"
#include "fork.h"
#include "wait.h"
#include "str.h"
#include "case.h"
#include "readwrite.h"
#include "substdio.h"
#include "auto_break.h"
#define FDAUTH 3

static stralloc key = {0};
static stralloc domain = {0};
static stralloc wildhost = {0};
static stralloc address = {0};
static stralloc rcptline = {0};
static stralloc vkey = {0};
static stralloc verp = {0};
static int flagrcpts = 0;
static int fdrcps;

/** @file  recipients.c 
    @brief functions recipients_init, recipients, recipients_parse, callapam
    @param pointer to address, length of address
    @return         -3: problem with PAM 
                    -2: out of memory 
                    -1: error reading control file
                     0: address not found; unsuccessfull
                     1: CDB lookup; successfull
                     2: PAM lookup; successfull
                     3: Wildcarded domain; successfull
                     4: Pass-thru; neutral 
                    10: none existing control file; pass-thru 
*/

int recipients_init()
{
  flagrcpts = control_readfile(&rcptline,"control/recipients",0);
  if (flagrcpts != 1) return flagrcpts;
  return 0;
}

int recipients(char *buf,int len)
{
  int at;
  int i;
  int r;

  if (flagrcpts != 1) return 10;

  address.len = 0;				/* multiple recipients */
  domain.len = 0;

  at = byte_rchr(buf,len,'@');
  if (at < len) {
    if (!stralloc_copyb(&domain,buf + at + 1,len - at - 1)) return -2;
    if (!stralloc_copyb(&address,buf,len)) return -2;
  } else {
    if (!stralloc_copyb(&address,buf,len)) return -2;
    if (!stralloc_append(&address,"@")) return -2;
    if (!stralloc_copys(&domain,"localhost")) return -2;
    if (!stralloc_cat(&address,&domain)) return -2; 
  }

  if (!stralloc_0(&address)) return -2;
  if (!stralloc_0(&domain)) return -2;
  key.len = 0;
  if (!stralloc_copys(&key,":")) return -2;
  if (!stralloc_cat(&key,&address)) return -2;
  if (!stralloc_0(&key)) return -2;		/* \0\0 terminated */
  case_lowerb(key.s,key.len);
  case_lowerb(domain.s,domain.len);

  vkey.len = 0;
  verp.len = 0;
  for (i = 0; i < at; i++) {
    if (buf[i] == *auto_break) {
      if (!stralloc_copyb(&verp,buf,i + 1)) return -2;
      if (!stralloc_append(&verp,"@")) return -2;
      if (!stralloc_cat(&verp,&domain)) return -2;
      if (!stralloc_copys(&vkey,":")) return -2;
      if (!stralloc_cat(&vkey,&verp)) return -2;
      if (!stralloc_0(&vkey)) return -2;	/* \0\0 terminated */
      case_lowerb(vkey.s,vkey.len);
      break;
    } 
  }

  r = recipients_parse(domain.s,domain.len,address.s,key.s,key.len,verp.s,vkey.s,vkey.len);
  if (r) return r;
  return 0;
} 

char ssrcptbuf[512];
substdio ssrcpt = SUBSTDIO_FDBUF(safewrite,FDAUTH,ssrcptbuf,sizeof(ssrcptbuf));

int callapam(char *pam,char *addr)
{
  int i;
  int j=0;
  int wstat;
  int pi[2];
  int child; 
  char ch;
  static stralloc mailaddress = {0};

  char *childargs[7] = {0, 0, 0, 0, 0, 0, 0};
  stralloc pamarg = {0};
  stralloc pamname = {0};
  stralloc pamarg1 = {0};
  stralloc pamarg2 = {0};
  stralloc pamarg3 = {0};
  stralloc pamarg4 = {0};
  stralloc pamarg5 = {0};

  for (i = 0; ch = pam[i]; i++) {	
    if (j < 6) {
      if (ch != ' ') 
        if (!stralloc_append(&pamarg,&ch)) return -2;
      if (ch == ' ' || ch == '\n' || i == str_len(pam) - 1) {
        if (!stralloc_0(&pamarg)) return -2;
        switch (j) {
          case 0:
            if (!stralloc_copy(&pamname,&pamarg)) return -2;
            childargs[0] = pamname.s;
          case 1:
            if (!stralloc_copy(&pamarg1,&pamarg)) return -2;
            childargs[1] = pamarg1.s;
          case 2:
            if (!stralloc_copy(&pamarg2,&pamarg)) return -2;
            childargs[2] = pamarg2.s;
          case 3:
            if (!stralloc_copy(&pamarg3,&pamarg)) return -2;
            childargs[3] = pamarg3.s; 
          case 4:
            if (!stralloc_copy(&pamarg4,&pamarg)) return -2;
            childargs[4] = pamarg4.s; 
          case 5:
            if (!stralloc_copy(&pamarg5,&pamarg)) return -2;
            childargs[5] = pamarg5.s; 
        }
        j++;
        pamarg.len = 0;
      } 
    }
  }
  childargs[j] = 0; 

  close(FDAUTH);
  if (pipe(pi) == -1) return -3;
  if (pi[0] != FDAUTH) return -3;

  switch(child = fork()) {
    case -1:
      return -3;
    case 0:
      close(pi[1]);
      if (fd_copy(FDAUTH,pi[0]) == -1) return -3;
      sig_pipedefault();
      execvp(childargs[0],childargs);
      return 111;
  }
  close(pi[0]);

/* checkpassword compliant form: address\0\0\0 */
  
  mailaddress.len = 0;
  if (!stralloc_copys(&mailaddress,addr)) return -2;
  if (!stralloc_0(&mailaddress)) return -2;
  if (!stralloc_0(&mailaddress)) return -2;
  if (!stralloc_0(&mailaddress)) return -2;

  substdio_fdbuf(&ssrcpt,write,pi[1],ssrcptbuf,sizeof(ssrcptbuf));
  if (substdio_put(&ssrcpt,mailaddress.s,mailaddress.len) == -1) return -3;
  if (substdio_flush(&ssrcpt) == -1) return -3;
  close(pi[1]);
  
  if (wait_pid(&wstat,child) == -1) return -3;
  if (wait_crashed(wstat)) return -3;
  return wait_exitcode(wstat);
}

int recipients_parse(char *rhost,int rlen,char *addr,char *rkey,int klen,char *vaddr,char *vkey,int vlen)
{
  int i;
  int r;
  int j = 0;
  int k = 0;
  uint32 dlen;
  static stralloc line = {0};
  int seenhost = 0;

  wildhost.len = 0;
  line.len = 0;

  if (!stralloc_copys(&wildhost,"!")) return -2;
  if (!stralloc_cats(&wildhost,rhost)) return -2;
  if (!stralloc_0(&wildhost)) return -2;
  
  for (i = 0; i < rcptline.len; ++i) {	
    if (!stralloc_append(&line,&rcptline.s[i])) return -2;

    if (rcptline.s[i] == '\0') {
      if (!stralloc_0(&line)) return -2;

      j = byte_chr(line.s,line.len,':');
      k = byte_chr(line.s,line.len,'|'); 

      if (!str_diffn(line.s,wildhost.s,wildhost.len - 1)) return 3;    /* wilddomain */ 
      if (j  > 0 || k > 0)
        if (!str_diffn(line.s,"@",1))                                /* exact */  
          if (!str_diffn(line.s+1,rhost,rlen - 1)) seenhost = 1; 

      if (!seenhost) {                                               /* domain */
        if (j > 0 && rlen >= j)
          if (!str_diffn(line.s,rhost + rlen - j - 1,j - 1)) seenhost = 2;
        if (k > 0 && rlen >= k)
          if (!str_diffn(line.s,rhost + rlen - k - 1,k - 1)) seenhost = 3;
      }
      if (!seenhost)                                                 /* pass-thru */
        if (!str_diffn(line.s,"!*",2)) return 4;

      if (k > 0 && k < line.len)                                     /* pam */
        if (seenhost || !str_diffn(line.s,"*",1)) {
          r = callapam(line.s + k + 1,addr); 
          if (vlen > 0 && r != 0)
            r = callapam(line.s + k + 1,vaddr); 
          if (r == 0) return 2;
          if (r == 111) return r;
        }

      if (j > 0 && j < line.len) 			             /* cdb */
        if (seenhost || !str_diffn(line.s,"*",1)) {
          fdrcps = open_read(line.s+j+1);
          if (fdrcps != -1) {
            r = cdb_seek(fdrcps,rkey,klen - 2,&dlen);
            if (vlen > 0 && r == 0)
              r = cdb_seek(fdrcps,vkey,vlen - 2,&dlen);
            close(fdrcps);
            if (r) return 1;
          } 
        }
      
      if (!seenhost) {
        fdrcps = open_read(line.s);                                  /* legacy cdb */					
        if (fdrcps != -1) {
          r = cdb_seek(fdrcps,rkey,klen - 2,&dlen);
          if (vlen > 0 && r == 0)
            r = cdb_seek(fdrcps,vkey,vlen - 2,&dlen);
          close(fdrcps);
          if (r) return 1;
        }
      }

      line.len = 0;
    }
  }
  return 0;
}
