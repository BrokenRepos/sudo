#ifndef lint
/*static char yysccsid[] = "from: @(#)yaccpar	1.9 (Berkeley) 02/21/93";*/
static char yyrcsid[]
#if __GNUC__ == 2
  __attribute__ ((unused))
#endif /* __GNUC__ == 2 */
  = "$OpenBSD: skeleton.c,v 1.13 1998/11/18 15:45:12 dm Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "parse.yacc"
/*
 * Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Chris Jepeway
 * <jepeway@cs.utk.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * XXX - the whole opFOO naming thing is somewhat bogus.
 *
 * XXX - the way things are stored for printmatches is stupid,
 *       they should be stored as elements in an array and then
 *       list_matches() can format things the way it wants.
 */

#include "config.h"
#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#if defined(YYBISON) && defined(HAVE_ALLOCA_H) && !defined(__GNUC__)
#include <alloca.h>
#endif /* YYBISON && HAVE_ALLOCA_H && !__GNUC__ */
#ifdef HAVE_LSEARCH
#include <search.h>
#endif /* HAVE_LSEARCH */

#include "sudo.h"
#include "parse.h"

#ifndef HAVE_LSEARCH
#include "emul/search.h"
#endif /* HAVE_LSEARCH */

#ifndef HAVE_STRCASECMP
#define strcasecmp(a,b)		strcmp(a,b)
#endif /* !HAVE_STRCASECMP */

#ifndef lint
static const char rcsid[] = "$Sudo$";
#endif /* lint */

/*
 * Globals
 */
extern int sudolineno, parse_error;
int errorlineno = -1;
int clearaliases = 1;
int printmatches = FALSE;

/*
 * Alias types
 */
#define HOST_ALIAS		 1
#define CMND_ALIAS		 2
#define USER_ALIAS		 3
#define RUNAS_ALIAS		 4

/*
 * The matching stack, initial space allocated in init_parser().
 */
struct matchstack *match;
int top = 0, stacksize = 0;

#define push \
    { \
	if (top >= stacksize) { \
	    while ((stacksize += STACKINCREMENT) < top); \
	    match = (struct matchstack *) erealloc(match, sizeof(struct matchstack) * stacksize); \
	} \
	match[top].user   = -1; \
	match[top].cmnd   = -1; \
	match[top].host   = -1; \
	match[top].runas  = -1; \
	match[top].nopass = -1; \
	top++; \
    }

#define pushcp \
    { \
	if (top >= stacksize) { \
	    while ((stacksize += STACKINCREMENT) < top); \
	    match = (struct matchstack *) erealloc(match, sizeof(struct matchstack) * stacksize); \
	} \
	match[top].user   = match[top-1].user; \
	match[top].cmnd   = match[top-1].cmnd; \
	match[top].host   = match[top-1].host; \
	match[top].runas  = match[top-1].runas; \
	match[top].nopass = match[top-1].nopass; \
	top++; \
    }

#define pop \
    { \
	if (top == 0) \
	    yyerror("matching stack underflow"); \
	else \
	    top--; \
    }

/*
 * Shortcuts for append()
 */
#define append_cmnd(s, p) append(s, &cm_list[cm_list_len].cmnd, \
	&cm_list[cm_list_len].cmnd_len, &cm_list[cm_list_len].cmnd_size, p)

#define append_runas(s, p) append(s, &cm_list[cm_list_len].runas, \
	&cm_list[cm_list_len].runas_len, &cm_list[cm_list_len].runas_size, p)

#define append_entries(s, p) append(s, &ga_list[ga_list_len-1].entries, \
	&ga_list[ga_list_len-1].entries_len, \
	&ga_list[ga_list_len-1].entries_size, p)

/*
 * The stack for printmatches.  A list of allowed commands for the user.
 */
static struct command_match *cm_list = NULL;
static size_t cm_list_len = 0, cm_list_size = 0;

/*
 * List of Cmnd_Aliases and expansions for `sudo -l'
 */
static int in_alias = FALSE;
static size_t ga_list_len = 0, ga_list_size = 0;
static struct generic_alias *ga_list = NULL;

/*
 * Protoypes
 */
extern int  command_matches	__P((char *, char *, char *, char *));
extern int  addr_matches	__P((char *));
extern int  netgr_matches	__P((char *, char *, char *));
extern int  usergr_matches	__P((char *, char *));
static int  find_alias		__P((char *, int));
static int  add_alias		__P((char *, int));
static int  more_aliases	__P((void));
static void append		__P((char *, char **, size_t *, size_t *, char *));
static void expand_ga_list	__P((void));
static void expand_match_list	__P((void));
       void init_parser		__P((void));
       void yyerror		__P((char *));

void
yyerror(s)
    char *s;
{
    /* Save the line the first error occured on. */
    if (errorlineno == -1)
	errorlineno = sudolineno ? sudolineno - 1 : 0;
#ifndef TRACELEXER
    (void) fprintf(stderr, ">>> sudoers file: %s, line %d <<<\n", s,
	sudolineno ? sudolineno - 1 : 0);
#else
    (void) fprintf(stderr, "<*> ");
#endif
    parse_error = TRUE;
}
#line 203 "parse.yacc"
typedef union {
    char *string;
    int BOOLEAN;
    struct sudo_command command;
    int tok;
} YYSTYPE;
#line 227 "sudo.tab.c"
#define ALIAS 257
#define NTWKADDR 258
#define FQHOST 259
#define NETGROUP 260
#define USERGROUP 261
#define NAME 262
#define ALL 263
#define RUNAS 264
#define NOPASSWD 265
#define PASSWD 266
#define COMMAND 267
#define COMMENT 268
#define HOSTALIAS 269
#define CMNDALIAS 270
#define USERALIAS 271
#define RUNASALIAS 272
#define ERROR 273
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    5,    5,    7,    5,    5,    5,    5,    5,
    8,    8,   13,   16,   16,    2,    2,    2,    2,    2,
    2,   15,   15,   17,   20,   21,   20,   18,   18,   22,
   22,   23,   24,   23,    3,    3,    3,    3,    3,   19,
   19,   19,    1,    1,    1,   10,   10,   26,   25,   14,
   14,   11,   11,   28,   27,   29,   29,   12,   12,   31,
   30,    9,    9,   33,   32,   34,   34,    6,    6,    4,
    4,    4,    4,    4,
};
short yylen[] = {                                         2,
    1,    2,    1,    2,    0,    3,    2,    2,    2,    2,
    1,    3,    3,    1,    2,    1,    1,    1,    1,    1,
    1,    1,    3,    3,    1,    0,    3,    0,    2,    1,
    3,    1,    0,    3,    1,    1,    1,    1,    1,    0,
    1,    1,    1,    1,    1,    1,    3,    0,    4,    1,
    3,    1,    3,    0,    4,    1,    3,    1,    3,    0,
    4,    1,    3,    0,    4,    1,    3,    1,    2,    1,
    1,    1,    1,    1,
};
short yydefred[] = {                                      0,
    0,    3,    0,    0,    0,    0,    0,    1,    0,    4,
   48,    0,   46,   54,    0,   52,   64,    0,   62,   60,
    0,   58,    2,   73,   72,   71,   70,   74,    0,   68,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   69,
   21,   17,   20,   18,   19,   16,    0,   14,    0,   11,
    0,   50,    0,   47,    0,   53,    0,   63,    0,   59,
   15,    0,    0,    0,    0,   44,   43,   45,   26,   25,
   56,    0,   66,    0,   38,   37,   36,   35,   39,   33,
   32,    0,   30,   12,    0,    0,   22,    0,   51,    0,
    0,    0,    0,    0,    0,    0,   41,   42,    0,   27,
   57,   67,   34,   31,   23,   24,
};
short yydgoto[] = {                                       7,
   70,   48,   81,   30,    8,   31,    9,   49,   18,   12,
   15,   21,   50,   51,   86,   52,   87,   88,   99,   71,
   90,   82,   83,   93,   13,   32,   16,   34,   72,   22,
   38,   19,   36,   74,
};
short yysindex[] = {                                   -242,
 -264,    0, -247, -246, -244, -236, -242,    0,   -8,    0,
    0,  -36,    0,    0,  -35,    0,    0,  -34,    0,    0,
  -27,    0,    0,    0,    0,    0,    0,    0, -215,    0,
  170,  -29, -247,  -25, -246,  -12, -244,   -4, -236,    0,
    0,    0,    0,    0,    0,    0, -219,    0,    2,    0,
  -41,    0,  170,    0,  -21,    0,   -8,    0,  174,    0,
    0,  170, -231,  170,   14,    0,    0,    0,    0,    0,
    0,   17,    0,   19,    0,    0,    0,    0,    0,    0,
    0,   20,    0,    0,  174,   21,    0, -259,    0, -248,
  -21,   -8, -207,  174,   20, -231,    0,    0,  -21,    0,
    0,    0,    0,    0,    0,    0,
};
short yyrindex[] = {                                    181,
    0,    0,    0,    0,    0,    0,  181,    0,    0,    0,
    0,   86,    0,    0,  103,    0,    0,  120,    0,    0,
  137,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  154,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  -33,    0,    1,    0,    0,    0,    0,    0,
    0,   18,    0,   35,    0,    0,    0,    0,    0,    0,
    0,   52,    0,    0,    0,   69,    0,  -16,    0,    0,
    0,    0,    0,    0,  -28,  -33,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
  -24,   15,  -26,   41,   64,  -55,    0,    0,    0,    0,
    0,    0,   10,   22,    0,    9,  -22,    0,    0,  -83,
    0,   -7,  -17,    0,   46,    0,   45,    0,    0,   42,
    0,   47,    0,    0,
};
#define YYTABLESIZE 444
short yytable[] = {                                      28,
   49,   73,   64,   10,   29,   97,   98,  101,   66,   11,
   14,   69,   17,    1,   67,  106,   40,   55,   68,   63,
   20,   33,   35,   37,   29,    2,    3,    4,    5,    6,
   39,   53,   85,   49,   65,   55,  102,   41,   42,   43,
   44,   24,   45,   46,   25,   26,   27,   28,   57,   75,
   55,   61,   76,   77,   78,   79,   59,   64,   49,   62,
   91,   61,   92,   94,   96,  100,  103,   65,   13,   40,
   23,   84,   89,  105,   65,   55,  104,   95,   54,   56,
   60,    0,    0,   58,   61,    8,    0,    0,    0,    0,
    0,    0,   65,    0,    0,    0,    0,    0,    0,    0,
    0,   13,    9,    0,    0,    0,    0,    0,    0,   61,
    0,    0,    0,    0,    0,    0,    0,    0,    8,    7,
    0,    0,    0,    0,    0,    0,   13,    0,    0,    0,
    0,    0,    0,    0,    0,    9,   10,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    7,    6,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   10,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    6,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   47,    0,    0,    0,   80,    0,    0,    0,
    0,    0,    0,    5,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   28,    0,    0,    0,    0,   29,   28,
    0,   28,   28,   28,   29,   66,   29,   29,   29,    0,
   40,   67,    0,    0,    0,   68,   40,    0,   24,    0,
   40,   25,   26,   27,   28,    0,   49,   49,    0,    0,
   49,   49,   49,   49,    0,    0,    0,    0,   49,   49,
   49,   49,   49,   55,   55,    0,    0,   55,   55,   55,
   55,    0,    0,    0,    0,   55,   55,   55,   55,   55,
   65,   65,    0,    0,   65,   65,   65,   65,    0,    0,
    0,    0,   65,   65,   65,   65,   65,   61,   61,    0,
    0,   61,   61,   61,   61,    0,    0,    0,    0,   61,
   61,   61,   61,   61,   13,   13,    0,    0,   13,   13,
   13,   13,    0,    0,    0,    0,   13,   13,   13,   13,
   13,    8,    8,    0,    0,    8,    8,    8,    8,    0,
    0,    0,    0,    8,    8,    8,    8,    8,    9,    9,
    0,    0,    9,    9,    9,    9,    0,    0,    0,    0,
    9,    9,    9,    9,    9,    7,    7,    0,    0,    7,
    7,    7,    7,    0,    0,    0,    0,    7,    7,    7,
    7,    7,   10,   10,    0,    0,   10,   10,   10,   10,
    0,    0,    0,    0,   10,   10,   10,   10,   10,    6,
    6,    0,    0,    6,    6,    6,    6,    0,    0,    0,
    0,    6,    6,    6,    6,    6,   41,   42,   43,   44,
   75,   45,   46,   76,   77,   78,   79,    5,    0,    0,
    5,    5,    5,    5,
};
short yycheck[] = {                                      33,
    0,   57,   44,  268,   33,  265,  266,   91,  257,  257,
  257,   33,  257,  256,  263,   99,   33,    0,  267,   61,
  257,   58,   58,   58,   33,  268,  269,  270,  271,  272,
   58,   61,  264,   33,    0,   61,   92,  257,  258,  259,
  260,  257,  262,  263,  260,  261,  262,  263,   61,  257,
   33,    0,  260,  261,  262,  263,   61,   44,   58,   58,
   44,   47,   44,   44,   44,   90,   93,   33,    0,   29,
    7,   62,   64,   96,   53,   58,   94,   85,   33,   35,
   39,   -1,   -1,   37,   33,    0,   -1,   -1,   -1,   -1,
   -1,   -1,   58,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   33,    0,   -1,   -1,   -1,   -1,   -1,   -1,   58,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   33,    0,
   -1,   -1,   -1,   -1,   -1,   -1,   58,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   33,    0,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   33,    0,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   33,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   33,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   33,   -1,   -1,   -1,   33,   -1,   -1,   -1,
   -1,   -1,   -1,   33,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  257,   -1,   -1,   -1,   -1,  257,  263,
   -1,  265,  266,  267,  263,  257,  265,  266,  267,   -1,
  257,  263,   -1,   -1,   -1,  267,  263,   -1,  257,   -1,
  267,  260,  261,  262,  263,   -1,  256,  257,   -1,   -1,
  260,  261,  262,  263,   -1,   -1,   -1,   -1,  268,  269,
  270,  271,  272,  256,  257,   -1,   -1,  260,  261,  262,
  263,   -1,   -1,   -1,   -1,  268,  269,  270,  271,  272,
  256,  257,   -1,   -1,  260,  261,  262,  263,   -1,   -1,
   -1,   -1,  268,  269,  270,  271,  272,  256,  257,   -1,
   -1,  260,  261,  262,  263,   -1,   -1,   -1,   -1,  268,
  269,  270,  271,  272,  256,  257,   -1,   -1,  260,  261,
  262,  263,   -1,   -1,   -1,   -1,  268,  269,  270,  271,
  272,  256,  257,   -1,   -1,  260,  261,  262,  263,   -1,
   -1,   -1,   -1,  268,  269,  270,  271,  272,  256,  257,
   -1,   -1,  260,  261,  262,  263,   -1,   -1,   -1,   -1,
  268,  269,  270,  271,  272,  256,  257,   -1,   -1,  260,
  261,  262,  263,   -1,   -1,   -1,   -1,  268,  269,  270,
  271,  272,  256,  257,   -1,   -1,  260,  261,  262,  263,
   -1,   -1,   -1,   -1,  268,  269,  270,  271,  272,  256,
  257,   -1,   -1,  260,  261,  262,  263,   -1,   -1,   -1,
   -1,  268,  269,  270,  271,  272,  257,  258,  259,  260,
  257,  262,  263,  260,  261,  262,  263,  257,   -1,   -1,
  260,  261,  262,  263,
};
#define YYFINAL 7
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 273
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'!'",0,0,0,0,0,0,0,0,0,0,"','",0,"'.'",0,0,0,0,0,0,0,0,0,0,0,"':'",0,0,"'='",0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"ALIAS",
"NTWKADDR","FQHOST","NETGROUP","USERGROUP","NAME","ALL","RUNAS","NOPASSWD",
"PASSWD","COMMAND","COMMENT","HOSTALIAS","CMNDALIAS","USERALIAS","RUNASALIAS",
"ERROR",
};
char *yyrule[] = {
"$accept : file",
"file : entry",
"file : file entry",
"entry : COMMENT",
"entry : error COMMENT",
"$$1 :",
"entry : $$1 opuser privileges",
"entry : USERALIAS useraliases",
"entry : HOSTALIAS hostaliases",
"entry : CMNDALIAS cmndaliases",
"entry : RUNASALIAS runasaliases",
"privileges : privilege",
"privileges : privileges ':' privilege",
"privilege : hostlist '=' cmndspeclist",
"ophostspec : hostspec",
"ophostspec : '!' hostspec",
"hostspec : ALL",
"hostspec : NTWKADDR",
"hostspec : NETGROUP",
"hostspec : NAME",
"hostspec : FQHOST",
"hostspec : ALIAS",
"cmndspeclist : cmndspec",
"cmndspeclist : cmndspeclist ',' cmndspec",
"cmndspec : runasspec nopasswd opcmnd",
"opcmnd : cmnd",
"$$2 :",
"opcmnd : '!' $$2 cmnd",
"runasspec :",
"runasspec : RUNAS runaslist",
"runaslist : oprunasuser",
"runaslist : runaslist ',' oprunasuser",
"oprunasuser : runasuser",
"$$3 :",
"oprunasuser : '!' $$3 runasuser",
"runasuser : NAME",
"runasuser : USERGROUP",
"runasuser : NETGROUP",
"runasuser : ALIAS",
"runasuser : ALL",
"nopasswd :",
"nopasswd : NOPASSWD",
"nopasswd : PASSWD",
"cmnd : ALL",
"cmnd : ALIAS",
"cmnd : COMMAND",
"hostaliases : hostalias",
"hostaliases : hostaliases ':' hostalias",
"$$4 :",
"hostalias : ALIAS $$4 '=' hostlist",
"hostlist : ophostspec",
"hostlist : hostlist ',' ophostspec",
"cmndaliases : cmndalias",
"cmndaliases : cmndaliases ':' cmndalias",
"$$5 :",
"cmndalias : ALIAS $$5 '=' cmndlist",
"cmndlist : opcmnd",
"cmndlist : cmndlist ',' opcmnd",
"runasaliases : runasalias",
"runasaliases : runasaliases ':' runasalias",
"$$6 :",
"runasalias : ALIAS $$6 '=' runaslist",
"useraliases : useralias",
"useraliases : useraliases ':' useralias",
"$$7 :",
"useralias : ALIAS $$7 '=' userlist",
"userlist : opuser",
"userlist : userlist ',' opuser",
"opuser : user",
"opuser : '!' user",
"user : NAME",
"user : USERGROUP",
"user : NETGROUP",
"user : ALIAS",
"user : ALL",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 673 "parse.yacc"

typedef struct {
    int type;
    char *name;
} aliasinfo;

#define MOREALIASES (32)
aliasinfo *aliases = NULL;
size_t naliases = 0;
size_t nslots = 0;


/*
 * Compare two aliasinfo structures, strcmp() style.
 */
static int
aliascmp(a1, a2)
    const VOID *a1, *a2;
{
    int r;
    aliasinfo *ai1, *ai2;

    ai1 = (aliasinfo *) a1;
    ai2 = (aliasinfo *) a2;
    r = strcmp(ai1->name, ai2->name);
    if (r == 0)
	r = ai1->type - ai2->type;

    return(r);
}

/*
 * Compare two generic_alias structures, strcmp() style.
 */
static int
genaliascmp(entry, key)
    const VOID *entry, *key;
{
    struct generic_alias *ga1 = (struct generic_alias *) key;
    struct generic_alias *ga2 = (struct generic_alias *) entry;

    return(strcmp(ga1->alias, ga2->alias));
}


/*
 * Adds the named alias of the specified type to the aliases list.
 */
static int
add_alias(alias, type)
    char *alias;
    int type;
{
    aliasinfo ai, *aip;
    size_t onaliases;
    char s[512];

    if (naliases >= nslots && !more_aliases()) {
	(void) snprintf(s, sizeof(s), "Out of memory defining alias `%s'",
			alias);
	yyerror(s);
	return(FALSE);
    }

    ai.type = type;
    ai.name = estrdup(alias);
    onaliases = naliases;

    aip = (aliasinfo *) lsearch((VOID *)&ai, (VOID *)aliases, &naliases,
				sizeof(ai), aliascmp);
    if (aip == NULL) {
	(void) snprintf(s, sizeof(s), "Aliases corrupted defining alias `%s'",
			alias);
	yyerror(s);
	return(FALSE);
    }
    if (onaliases == naliases) {
	(void) snprintf(s, sizeof(s), "Alias `%s' already defined", alias);
	yyerror(s);
	return(FALSE);
    }

    return(TRUE);
}

/*
 * Searches for the named alias of the specified type.
 */
static int
find_alias(alias, type)
    char *alias;
    int type;
{
    aliasinfo ai;

    ai.name = alias;
    ai.type = type;

    return(lfind((VOID *)&ai, (VOID *)aliases, &naliases,
		 sizeof(ai), aliascmp) != NULL);
}

/*
 * Allocates more space for the aliases list.
 */
static int
more_aliases()
{

    nslots += MOREALIASES;
    if (nslots == MOREALIASES)
	aliases = (aliasinfo *) malloc(nslots * sizeof(aliasinfo));
    else
	aliases = (aliasinfo *) realloc(aliases, nslots * sizeof(aliasinfo));

    return(aliases != NULL);
}

/*
 * Lists the contents of the aliases list.
 */
void
dumpaliases()
{
    size_t n;

    for (n = 0; n < naliases; n++) {
	switch (aliases[n].type) {
	case HOST_ALIAS:
	    (void) puts("HOST_ALIAS");
	    break;

	case CMND_ALIAS:
	    (void) puts("CMND_ALIAS");
	    break;

	case USER_ALIAS:
	    (void) puts("USER_ALIAS");
	    break;

	case RUNAS_ALIAS:
	    (void) puts("RUNAS_ALIAS");
	    break;
	}
	(void) printf("\t%s\n", aliases[n].name);
    }
}

/*
 * Lists the contents of cm_list and ga_list for `sudo -l'.
 */
void
list_matches()
{
    int i; 
    char *p;
    struct generic_alias *ga, key;

    (void) puts("You may run the following commands on this host:");
    for (i = 0; i < cm_list_len; i++) {

	/* Print the runas list. */
	(void) fputs("    ", stdout);
	if (cm_list[i].runas) {
	    (void) putchar('(');
	    p = strtok(cm_list[i].runas, ", ");
	    do {
		if (p != cm_list[i].runas)
		    (void) fputs(", ", stdout);

		key.alias = p;
		if ((ga = (struct generic_alias *) lfind((VOID *) &key,
		    (VOID *) &ga_list[0], &ga_list_len, sizeof(key), genaliascmp)))
		    (void) fputs(ga->entries, stdout);
		else
		    (void) fputs(p, stdout);
	    } while ((p = strtok(NULL, ", ")));
	    (void) fputs(") ", stdout);
	} else {
	    (void) printf("(%s) ", RUNAS_DEFAULT);
	}

	/* Is a password required? */
	if (cm_list[i].nopasswd == TRUE)
	    (void) fputs("NOPASSWD: ", stdout);

	/* Print the actual command or expanded Cmnd_Alias. */
	key.alias = cm_list[i].cmnd;
	if ((ga = (struct generic_alias *) lfind((VOID *) &key,
	    (VOID *) &ga_list[0], &ga_list_len, sizeof(key), genaliascmp)))
	    (void) puts(ga->entries);
	else
	    (void) puts(cm_list[i].cmnd);
    }

    /* Be nice and free up space now that we are done. */
    for (i = 0; i < ga_list_len; i++) {
	free(ga_list[i].alias);
	free(ga_list[i].entries);
    }
    free(ga_list);
    ga_list = NULL;

    for (i = 0; i < cm_list_len; i++) {
	free(cm_list[i].runas);
	free(cm_list[i].cmnd);
    }
    free(cm_list);
    cm_list = NULL;
    cm_list_len = 0;
    cm_list_size = 0;
}

/*
 * Appends a source string to the destination, optionally prefixing a separator.
 */
static void
append(src, dstp, dst_len, dst_size, separator)
    char *src, **dstp;
    size_t *dst_len, *dst_size;
    char *separator;
{
    size_t src_len = strlen(src);
    char *dst = *dstp;

    /*
     * Only add the separator if there is something to separate from.
     * If the last char is a '!', don't apply the separator (XXX).
     */
    if (separator && dst && dst[*dst_len - 1] != '!')
	src_len += strlen(separator);
    else
	separator = NULL;

    /* Assumes dst will be NULL if not set. */
    if (dst == NULL) {
	dst = (char *) emalloc(BUFSIZ);
	*dst_size = BUFSIZ;
	*dst_len = 0;
	*dstp = dst;
    }

    /* Allocate more space if necessary. */
    if (*dst_size <= *dst_len + src_len) {
	while (*dst_size <= *dst_len + src_len)
	    *dst_size += BUFSIZ;

	dst = (char *) erealloc(dst, *dst_size);
	*dstp = dst;
    }

    /* Copy src -> dst adding a separator if appropriate and adjust len. */
    dst += *dst_len;
    *dst_len += src_len;
    *dst = '\0';
    if (separator)
	(void) strcat(dst, separator);
    (void) strcat(dst, src);
}

/*
 * Frees up space used by the aliases list and resets the associated counters.
 */
void
reset_aliases()
{
    size_t n;

    if (aliases) {
	for (n = 0; n < naliases; n++)
	    free(aliases[n].name);
	free(aliases);
	aliases = NULL;
    }
    naliases = nslots = 0;
}

/*
 * Increments ga_list_len, allocating more space as necessary.
 */
static void
expand_ga_list()
{

    if (++ga_list_len >= ga_list_size) {
	while ((ga_list_size += STACKINCREMENT) < ga_list_len)
	    ;
	ga_list = (struct generic_alias *)
	    erealloc(ga_list, sizeof(struct generic_alias) * ga_list_size);
    }

    ga_list[ga_list_len - 1].entries = NULL;
}

/*
 * Increments cm_list_len, allocating more space as necessary.
 */
static void
expand_match_list()
{

    if (++cm_list_len >= cm_list_size) {
	while ((cm_list_size += STACKINCREMENT) < cm_list_len)
	    ;
	if (cm_list == NULL)
	    cm_list_len = 0;		/* start at 0 since it is a subscript */
	cm_list = (struct command_match *)
	    erealloc(cm_list, sizeof(struct command_match) * cm_list_size);
    }

    cm_list[cm_list_len].runas = cm_list[cm_list_len].cmnd = NULL;
    cm_list[cm_list_len].nopasswd = FALSE;
}

/*
 * Frees up spaced used by a previous parser run and allocates new space
 * for various data structures.
 */
void
init_parser()
{

    /* Free up old data structures if we run the parser more than once. */
    if (match) {
	free(match);
	match = NULL;
	top = 0;
	parse_error = FALSE;
	errorlineno = -1;   
	sudolineno = 1;     
    }

    /* Allocate space for the matching stack. */
    stacksize = STACKINCREMENT;
    match = (struct matchstack *) emalloc(sizeof(struct matchstack) * stacksize);

    /* Allocate space for the match list (for `sudo -l'). */
    if (printmatches == TRUE)
	expand_match_list();
}
#line 873 "sudo.tab.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
#if defined(__cplusplus) || __STDC__
static int yygrowstack(void)
#else
static int yygrowstack()
#endif
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
#if defined(__cplusplus) || __STDC__
yyparse(void)
#else
yyparse()
#endif
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 3:
#line 244 "parse.yacc"
{ ; }
break;
case 4:
#line 246 "parse.yacc"
{ yyerrok; }
break;
case 5:
#line 247 "parse.yacc"
{ push; }
break;
case 6:
#line 247 "parse.yacc"
{
			    while (top && user_matches != TRUE) {
				pop;
			    }
			}
break;
case 7:
#line 253 "parse.yacc"
{ ; }
break;
case 8:
#line 255 "parse.yacc"
{ ; }
break;
case 9:
#line 257 "parse.yacc"
{ ; }
break;
case 10:
#line 259 "parse.yacc"
{ ; }
break;
case 13:
#line 267 "parse.yacc"
{
			    /*
			     * We already did a push if necessary in
			     * cmndspec so just reset some values so
			     * the next 'privilege' gets a clean slate.
			     */
			    host_matches = -1;
			    runas_matches = -1;
			    no_passwd = -1;
			}
break;
case 14:
#line 279 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				host_matches = TRUE;
			}
break;
case 15:
#line 283 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				host_matches = FALSE;
			}
break;
case 16:
#line 288 "parse.yacc"
{
			    yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 17:
#line 292 "parse.yacc"
{
			    if (addr_matches(yyvsp[0].string))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 18:
#line 297 "parse.yacc"
{
			    if (netgr_matches(yyvsp[0].string, user_host, NULL))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 19:
#line 302 "parse.yacc"
{
			    if (strcasecmp(user_shost, yyvsp[0].string) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 20:
#line 307 "parse.yacc"
{
			    if (strcasecmp(user_host, yyvsp[0].string) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 21:
#line 312 "parse.yacc"
{
			    /* could be an all-caps hostname */
			    if (find_alias(yyvsp[0].string, HOST_ALIAS) == TRUE ||
				strcasecmp(user_shost, yyvsp[0].string) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 24:
#line 325 "parse.yacc"
{
			    /*
			     * Push the entry onto the stack if it is worth
			     * saving (or if nothing else is on the stack)
			     * and clear match status.
			     */
			    if (user_matches == TRUE && host_matches == TRUE &&
				((cmnd_matches != -1 && runas_matches != -1) ||
				top == 1))
				pushcp;
			    cmnd_matches = -1;
			}
break;
case 25:
#line 339 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				cmnd_matches = TRUE;
			}
break;
case 26:
#line 343 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("!", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_cmnd("!", NULL);
			    }
			}
break;
case 27:
#line 351 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				cmnd_matches = FALSE;
			}
break;
case 28:
#line 357 "parse.yacc"
{
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE) {
				if (runas_matches == -1) {
				    cm_list[cm_list_len].runas_len = 0;
				} else {
				    /* Inherit runas data. */
				    cm_list[cm_list_len].runas =
					estrdup(cm_list[cm_list_len-1].runas);
				    cm_list[cm_list_len].runas_len =
					cm_list[cm_list_len-1].runas_len;
				    cm_list[cm_list_len].runas_size =
					cm_list[cm_list_len-1].runas_size;
				}
			    }
			    /*
			     * If this is the first entry in a command list
			     * then check against RUNAS_DEFAULT.
			     */
			    if (runas_matches == -1)
				runas_matches =
				    (strcmp(RUNAS_DEFAULT, user_runas) == 0);
			}
break;
case 29:
#line 380 "parse.yacc"
{ ; }
break;
case 32:
#line 387 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				runas_matches = TRUE;
			}
break;
case 33:
#line 391 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("!", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas("!", ", ");
			    }
			}
break;
case 34:
#line 399 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				runas_matches = FALSE;
			}
break;
case 35:
#line 404 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas(yyvsp[0].string, ", ");
			    }
			    if (strcmp(yyvsp[0].string, user_runas) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 36:
#line 416 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas(yyvsp[0].string, ", ");
			    }
			    if (usergr_matches(yyvsp[0].string, user_runas))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 37:
#line 428 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas(yyvsp[0].string, ", ");
			    }
			    if (netgr_matches(yyvsp[0].string, NULL, user_runas))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 38:
#line 440 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas(yyvsp[0].string, ", ");
			    }
			    /* could be an all-caps username */
			    if (find_alias(yyvsp[0].string, RUNAS_ALIAS) == TRUE ||
				strcmp(yyvsp[0].string, user_runas) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 39:
#line 454 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas(yyvsp[0].string, ", ");
			    }
			    yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 40:
#line 467 "parse.yacc"
{
			    /* Inherit NOPASSWD/PASSWD status. */
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE) {
				if (no_passwd == TRUE)
				    cm_list[cm_list_len].nopasswd = TRUE;
				else
				    cm_list[cm_list_len].nopasswd = FALSE;
			    }
			}
break;
case 41:
#line 477 "parse.yacc"
{
			    no_passwd = TRUE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].nopasswd = TRUE;
			}
break;
case 42:
#line 483 "parse.yacc"
{
			    no_passwd = FALSE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].nopasswd = FALSE;
			}
break;
case 43:
#line 491 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE) {
				    append_cmnd(yyvsp[0].string, NULL);
				    expand_match_list();
				}
			    }

			    yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);

			    if (safe_cmnd)
				free(safe_cmnd);
			    safe_cmnd = estrdup(user_cmnd);
			}
break;
case 44:
#line 509 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries(yyvsp[0].string, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE) {
				    append_cmnd(yyvsp[0].string, NULL);
				    expand_match_list();
				}
			    }

			    if (find_alias(yyvsp[0].string, CMND_ALIAS) == TRUE)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 45:
#line 524 "parse.yacc"
{
			    if (printmatches == TRUE) {
				if (in_alias == TRUE) {
				    append_entries(yyvsp[0].command.cmnd, ", ");
				    if (yyvsp[0].command.args)
					append_entries(yyvsp[0].command.args, " ");
				}
				if (host_matches == TRUE &&
				    user_matches == TRUE)  {
				    append_cmnd(yyvsp[0].command.cmnd, NULL);
				    if (yyvsp[0].command.args)
					append_cmnd(yyvsp[0].command.args, " ");
				    expand_match_list();
				}
			    }

			    if (command_matches(user_cmnd, user_args,
				yyvsp[0].command.cmnd, yyvsp[0].command.args))
				yyval.BOOLEAN = TRUE;

			    free(yyvsp[0].command.cmnd);
			    if (yyvsp[0].command.args)
				free(yyvsp[0].command.args);
			}
break;
case 48:
#line 554 "parse.yacc"
{ push; }
break;
case 49:
#line 554 "parse.yacc"
{
			    if (host_matches == TRUE &&
				add_alias(yyvsp[-3].string, HOST_ALIAS) == FALSE)
				YYERROR;
			    pop;
			}
break;
case 54:
#line 570 "parse.yacc"
{
			    push;
			    if (printmatches == TRUE) {
				in_alias = TRUE;
				/* Allocate space for ga_list if necessary. */
				expand_ga_list();
				ga_list[ga_list_len-1].alias = estrdup(yyvsp[0].string);
			     }
			}
break;
case 55:
#line 578 "parse.yacc"
{
			    if (cmnd_matches == TRUE &&
				add_alias(yyvsp[-3].string, CMND_ALIAS) == FALSE)
				YYERROR;
			    pop;
			    free(yyvsp[-3].string);

			    if (printmatches == TRUE)
				in_alias = FALSE;
			}
break;
case 56:
#line 590 "parse.yacc"
{ ; }
break;
case 60:
#line 598 "parse.yacc"
{
			    push;
			    if (printmatches == TRUE) {
				in_alias = TRUE;
				/* Allocate space for ga_list if necessary. */
				expand_ga_list();
				ga_list[ga_list_len-1].alias = estrdup(yyvsp[0].string);
			    }
			}
break;
case 61:
#line 606 "parse.yacc"
{
			    if (runas_matches > 0 &&
				add_alias(yyvsp[-3].string, RUNAS_ALIAS) == FALSE)
				YYERROR;
			    pop;
			    free(yyvsp[-3].string);

			    if (printmatches == TRUE)
				in_alias = FALSE;
			}
break;
case 64:
#line 622 "parse.yacc"
{ push; }
break;
case 65:
#line 622 "parse.yacc"
{
			    if (user_matches == TRUE &&
				add_alias(yyvsp[-3].string, USER_ALIAS) == FALSE)
				YYERROR;
			    pop;
			    free(yyvsp[-3].string);
			}
break;
case 66:
#line 631 "parse.yacc"
{ ; }
break;
case 68:
#line 635 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				user_matches = TRUE;
			}
break;
case 69:
#line 639 "parse.yacc"
{
			    if (yyvsp[0].BOOLEAN == TRUE)
				user_matches = FALSE;
			}
break;
case 70:
#line 644 "parse.yacc"
{
			    if (strcmp(yyvsp[0].string, user_name) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 71:
#line 649 "parse.yacc"
{
			    if (usergr_matches(yyvsp[0].string, user_name))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 72:
#line 654 "parse.yacc"
{
			    if (netgr_matches(yyvsp[0].string, NULL, user_name))
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 73:
#line 659 "parse.yacc"
{
			    /* could be an all-caps username */
			    if (find_alias(yyvsp[0].string, USER_ALIAS) == TRUE ||
				strcmp(yyvsp[0].string, user_name) == 0)
				yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
case 74:
#line 666 "parse.yacc"
{
			    yyval.BOOLEAN = TRUE;
			    free(yyvsp[0].string);
			}
break;
#line 1575 "sudo.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
