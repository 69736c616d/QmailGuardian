#include "substdio.h"
#include "subfd.h"
#include "strerr.h"
#include "stralloc.h"
#include "open.h"
#include "case.h"
#include "readwrite.h"
#include "cdbmss.h"

#define FATAL "setforward: fatal: "

void usage()
{
  strerr_die1x(100,"setforward: usage: setforward data.cdb data.tmp");
}
void nomem()
{
  strerr_die2x(111,FATAL,"out of memory");
}
void missingsemicolon()
{
  strerr_die2x(100,FATAL,"final instruction must end with semicolon");
}
void extracolon()
{
  strerr_die2x(100,FATAL,"double colons are not permitted");
}
void extracomma()
{
  strerr_die2x(100,FATAL,"commas are not permitted before colons");
}
void nulbyte()
{
  strerr_die2x(100,FATAL,"NUL bytes are not permitted");
}
void longaddress()
{
  strerr_die2x(100,FATAL,"addresses over 800 bytes are not permitted");
}

char *fncdb;
char *fntmp;
int fd;
struct cdbmss cdbmss;
stralloc key = {0};

stralloc target = {0}; /* always initialized; no NUL */
stralloc command = {0}; /* always initialized; no NUL */
stralloc instr = {0}; /* always initialized */

int flagtarget = 0;
/* 0: reading target; command is empty; instr is empty */
/* 1: target is complete; instr still has to be written; reading command */

void writeerr()
{
  strerr_die4sys(111,FATAL,"unable to write to ",fntmp,": ");
}

void doit(prepend,data,datalen)
char *prepend;
char *data;
int datalen;
{
  if (!stralloc_copys(&key,prepend)) nomem();
  if (!stralloc_cat(&key,&target)) nomem();
  case_lowerb(key.s,key.len);
  if (cdbmss_add(&cdbmss,key.s,key.len,data,datalen) == -1)
    writeerr();
}

int getch(ch)
char *ch;
{
  int r;

  r = substdio_get(subfdinsmall,ch,1);
  if (r == -1)
    strerr_die2sys(111,FATAL,"unable to read input: ");
  return r;
}

int main(argc,argv)
int argc;
char **argv;
{
  char ch;
  int r;

  if (!stralloc_copys(&target,"")) nomem();
  if (!stralloc_copys(&command,"")) nomem();
  if (!stralloc_copys(&instr,"")) nomem();

  fncdb = argv[1]; if (!fncdb) usage();
  fntmp = argv[2]; if (!fntmp) usage();

  fd = open_trunc(fntmp);
  if (fd == -1)
    strerr_die4sys(111,FATAL,"unable to create ",fntmp,": ");

  if (cdbmss_start(&cdbmss,fd) == -1) writeerr();

  for (;;) {
    if (!getch(&ch)) goto eof;

    if (ch == '#') {
      while (ch != '\n') if (!getch(&ch)) goto eof;
      continue;
    }

    if (ch == ' ') continue;
    if (ch == '\n') continue;
    if (ch == '\t') continue;

    if (ch == ':') {
      if (flagtarget) extracolon();
      flagtarget = 1;
      continue;
    }

    if ((ch == ',') || (ch == ';')) {
      if (!flagtarget) extracomma();
      if (command.len) {
        if (command.s[0] == '?') {
          doit("?",command.s + 1,command.len - 1);
        }
        else if ((command.s[0] == '|') || (command.s[0] == '!')) {
          if (!stralloc_cat(&instr,&command)) nomem();
          if (!stralloc_0(&instr)) nomem();
        }
        else if ((command.s[0] == '.') || (command.s[0] == '/')) {
          if (!stralloc_cat(&instr,&command)) nomem();
          if (!stralloc_0(&instr)) nomem();
        }
        else {
          if (command.len > 800) longaddress();
          if (command.s[0] != '&')
            if (!stralloc_cats(&instr,"&")) nomem();
          if (!stralloc_cat(&instr,&command)) nomem();
          if (!stralloc_0(&instr)) nomem();
        }
      }

      if (!stralloc_copys(&command,"")) nomem();

      if (ch == ';') {
	if (instr.len)
          doit(":",instr.s,instr.len);

        if (!stralloc_copys(&target,"")) nomem();
        if (!stralloc_copys(&instr,"")) nomem();
        flagtarget = 0;
      }
      continue;
    }

    if (ch == '\\') if (!getch(&ch)) goto eof;
    if (ch == 0) nulbyte();
    if (!stralloc_append(flagtarget ? &command : &target,&ch)) nomem();
  }

  eof:
  if (flagtarget || target.len)
    missingsemicolon();

  if (cdbmss_finish(&cdbmss) == -1) writeerr();
  if (fsync(fd) == -1) writeerr();
  if (close(fd) == -1) writeerr(); /* NFS stupidity */

  if (rename(fntmp,fncdb) == -1)
    strerr_die6sys(111,FATAL,"unable to move ",fntmp," to ",fncdb,": ");
  
  _exit(0);
}
