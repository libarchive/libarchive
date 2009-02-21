#include <stdlib.h>
#ifndef lint
#ifdef __unused
__unused
#endif
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.37 2003/02/12 18:03:55 davidc Exp $";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
#if defined(__cplusplus) || __STDC__
static int yygrowstack(void);
#else
static int yygrowstack();
#endif
#define YYPREFIX "yy"
#line 2 "getdate.y"
/*
 * March 2005: Further modified and simplified by Tim Kientzle:
 * Eliminate minutes-based calculations (just do everything in
 * seconds), have lexer only recognize unsigned integers (handle '+'
 * and '-' characters in grammar), combine tables into one table with
 * explicit abbreviation notes, do am/pm adjustments in the grammar
 * (eliminate some state variables and post-processing).  Among other
 * things, these changes eliminated two shift/reduce conflicts.  (Went
 * from 10 to 8.)
 * All of Tim Kientzle's changes to this file are public domain.
 */

/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**
**  This grammar has 10 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
/* SUPPRESS 287 on yaccpar_sccsid *//* Unused static variable */
/* SUPPRESS 288 on yyerrlab *//* Label unused */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/tar/getdate.y,v 1.9 2007/07/20 01:27:50 kientzle Exp $");
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
/* Bison tries to redefine malloc() and free() without the next define. */
#define	YYINCLUDED_STDLIB_H
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#define __STDC__	/* for a bug of bison 2.1 on Windows */
#endif

#define yyparse getdate_yyparse
#define yylex getdate_yylex
#define yyerror getdate_yyerror

static int yyparse(void);
static int yylex(void);
static int yyerror(const char *);

time_t get_date(char *);

#define EPOCH		1970
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)

/*
**  Daylight-savings mode:  on, off, or not yet known.
*/
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
**  Global variables.  We could get rid of most of these by using a good
**  union as the yacc stack.  (This routine was originally written before
**  yacc had the %union construct.)  Maybe someday; right now we only use
**  the %union very rarely.
*/
static char	*yyInput;

static DSTMODE	yyDSTmode;
static time_t	yyDayOrdinal;
static time_t	yyDayNumber;
static int	yyHaveDate;
static int	yyHaveDay;
static int	yyHaveRel;
static int	yyHaveTime;
static int	yyHaveZone;
static time_t	yyTimezone;
static time_t	yyDay;
static time_t	yyHour;
static time_t	yyMinutes;
static time_t	yyMonth;
static time_t	yySeconds;
static time_t	yyYear;
static time_t	yyRelMonth;
static time_t	yyRelSeconds;

#line 93 "getdate.y"
typedef union {
    time_t		Number;
} YYSTYPE;
#line 118 "getdate.c"
#define YYERRCODE 256
#define tAGO 257
#define tDAY 258
#define tDAYZONE 259
#define tAM 260
#define tPM 261
#define tMONTH 262
#define tMONTH_UNIT 263
#define tSEC_UNIT 264
#define tUNUMBER 265
#define tZONE 266
#define tDST 267
const short yylhs[] = {                                        -1,
    0,    0,    1,    1,    1,    1,    1,    1,    2,    2,
    2,    2,    2,    2,    2,    8,    8,    3,    3,    3,
    5,    5,    5,    4,    4,    4,    4,    4,    4,    4,
    4,    6,    6,    9,    9,    9,    9,    9,    9,    9,
    9,    7,
};
const short yylen[] = {                                         2,
    0,    2,    1,    1,    1,    1,    1,    1,    2,    2,
    1,    2,    2,    3,    3,    3,    5,    1,    1,    2,
    1,    2,    2,    3,    5,    5,    5,    2,    4,    2,
    3,    2,    1,    3,    3,    2,    1,    3,    3,    2,
    1,    1,
};
const short yydefred[] = {                                      1,
    0,    0,   19,    0,   41,   37,    0,    0,    0,    0,
    2,    3,    4,    5,    6,    7,    8,    0,    0,   22,
    0,   23,    9,   10,    0,   40,   36,    0,    0,    0,
   20,    0,    0,   12,   13,    0,    0,   32,    0,   31,
    0,    0,    0,    0,   39,   35,   38,   34,   14,   15,
   29,    0,    0,    0,    0,   27,   26,   17,   25,
};
const short yydgoto[] = {                                       1,
   11,   12,   13,   14,   15,   16,   17,   18,   19,
};
const short yysindex[] = {                                      0,
  -43,  -40,    0, -260,    0,    0,  -34, -258, -253, -250,
    0,    0,    0,    0,    0,    0,    0,  -29, -237,    0,
  -23,    0,    0,    0, -243,    0,    0, -259, -242, -240,
    0, -256, -246,    0,    0, -239, -238,    0, -236,    0,
  -15,  -14,  -26,  -13,    0,    0,    0,    0,    0,    0,
    0, -232, -230, -229, -227,    0,    0,    0,    0,
};
const short yyrindex[] = {                                      0,
    0,   10,    0,    0,    0,    0,   67,   19,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   65,   28,    0,
   37,    0,    0,    0,   56,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    1,   47,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
const short yygindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 333
const short yytable[] = {                                       9,
   16,   10,   41,   20,   21,   42,   45,   46,   31,   21,
   28,   32,   30,   36,   33,   37,   47,   48,   18,   38,
   39,   40,   43,   29,   44,   49,   50,   33,   51,   52,
   53,   54,   56,   55,   57,   58,   28,   59,    0,    0,
    0,    0,    0,   16,    0,   16,   24,    0,    0,    0,
    0,    0,   21,    0,   21,   30,    0,    0,    0,    0,
    0,   18,    0,   18,   11,    0,   42,    0,    0,    0,
   33,    0,   33,    0,    0,    0,    0,    0,    0,   28,
    0,   28,    0,    0,    0,    0,    0,    0,    0,   24,
    0,   24,    0,    0,    0,    0,    0,    0,   30,    0,
   30,    0,    0,    0,    0,    0,    0,    0,    0,   42,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    2,    3,    0,    0,    4,    5,
    6,    7,    8,   22,    0,   23,   24,   25,   26,   27,
   34,   35,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   21,   21,    0,
    0,   21,   21,   21,   21,   21,   18,   18,    0,    0,
   18,   18,   18,   18,   18,   33,   33,    0,    0,   33,
   33,   33,   33,   33,   28,   28,    0,    0,   28,   28,
   28,   28,   28,    0,   24,   24,    0,    0,   24,   24,
   24,   24,   24,   30,   30,    0,    0,   30,   30,   30,
    0,   30,   11,   11,    0,   42,   11,   11,   11,   11,
   11,   42,   42,
};
const short yycheck[] = {                                      43,
    0,   45,  262,   44,  265,  265,  263,  264,  267,    0,
   45,  265,   47,   43,  265,   45,  263,  264,    0,  257,
   44,  265,  265,   58,  265,  265,  265,    0,  265,   45,
   45,   58,  265,   47,  265,  265,    0,  265,   -1,   -1,
   -1,   -1,   -1,   43,   -1,   45,    0,   -1,   -1,   -1,
   -1,   -1,   43,   -1,   45,    0,   -1,   -1,   -1,   -1,
   -1,   43,   -1,   45,    0,   -1,    0,   -1,   -1,   -1,
   43,   -1,   45,   -1,   -1,   -1,   -1,   -1,   -1,   43,
   -1,   45,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   43,
   -1,   45,   -1,   -1,   -1,   -1,   -1,   -1,   43,   -1,
   45,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   43,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  258,  259,   -1,   -1,  262,  263,
  264,  265,  266,  258,   -1,  260,  261,  262,  263,  264,
  260,  261,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  258,  259,
  260,  261,  262,  263,  264,  265,  266,  258,  259,   -1,
   -1,  262,  263,  264,  265,  266,  258,  259,   -1,   -1,
  262,  263,  264,  265,  266,  258,  259,   -1,   -1,  262,
  263,  264,  265,  266,  258,  259,   -1,   -1,  262,  263,
  264,  265,  266,   -1,  258,  259,   -1,   -1,  262,  263,
  264,  265,  266,  258,  259,   -1,   -1,  262,  263,  264,
   -1,  266,  258,  259,   -1,  259,  262,  263,  264,  265,
  266,  265,  266,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 267
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,"'+'","','","'-'",0,"'/'",0,0,0,0,0,0,0,0,0,0,"':'",0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"tAGO","tDAY",
"tDAYZONE","tAM","tPM","tMONTH","tMONTH_UNIT","tSEC_UNIT","tUNUMBER","tZONE",
"tDST",
};
const char * const yyrule[] = {
"$accept : spec",
"spec :",
"spec : spec item",
"item : time",
"item : zone",
"item : date",
"item : day",
"item : rel",
"item : number",
"time : tUNUMBER tAM",
"time : tUNUMBER tPM",
"time : bare_time",
"time : bare_time tAM",
"time : bare_time tPM",
"time : bare_time '+' tUNUMBER",
"time : bare_time '-' tUNUMBER",
"bare_time : tUNUMBER ':' tUNUMBER",
"bare_time : tUNUMBER ':' tUNUMBER ':' tUNUMBER",
"zone : tZONE",
"zone : tDAYZONE",
"zone : tZONE tDST",
"day : tDAY",
"day : tDAY ','",
"day : tUNUMBER tDAY",
"date : tUNUMBER '/' tUNUMBER",
"date : tUNUMBER '/' tUNUMBER '/' tUNUMBER",
"date : tUNUMBER '-' tUNUMBER '-' tUNUMBER",
"date : tUNUMBER '-' tMONTH '-' tUNUMBER",
"date : tMONTH tUNUMBER",
"date : tMONTH tUNUMBER ',' tUNUMBER",
"date : tUNUMBER tMONTH",
"date : tUNUMBER tMONTH tUNUMBER",
"rel : relunit tAGO",
"rel : relunit",
"relunit : '-' tUNUMBER tSEC_UNIT",
"relunit : '+' tUNUMBER tSEC_UNIT",
"relunit : tUNUMBER tSEC_UNIT",
"relunit : tSEC_UNIT",
"relunit : '-' tUNUMBER tMONTH_UNIT",
"relunit : '+' tUNUMBER tMONTH_UNIT",
"relunit : tUNUMBER tMONTH_UNIT",
"relunit : tMONTH_UNIT",
"number : tUNUMBER",
};
#endif
#if YYDEBUG
#include <stdio.h>
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
#line 338 "getdate.y"

static struct TABLE {
	size_t		abbrev;
	const char	*name;
	int		type;
	time_t		value;
} const TimeWords[] = {
	/* am/pm */
	{ 0, "am",		tAM,	0 },
	{ 0, "pm",		tPM,	0 },

	/* Month names. */
	{ 3, "january",		tMONTH,  1 },
	{ 3, "february",	tMONTH,  2 },
	{ 3, "march",		tMONTH,  3 },
	{ 3, "april",		tMONTH,  4 },
	{ 3, "may",		tMONTH,  5 },
	{ 3, "june",		tMONTH,  6 },
	{ 3, "july",		tMONTH,  7 },
	{ 3, "august",		tMONTH,  8 },
	{ 3, "september",	tMONTH,  9 },
	{ 3, "october",		tMONTH, 10 },
	{ 3, "november",	tMONTH, 11 },
	{ 3, "december",	tMONTH, 12 },

	/* Days of the week. */
	{ 2, "sunday",		tDAY, 0 },
	{ 3, "monday",		tDAY, 1 },
	{ 2, "tuesday",		tDAY, 2 },
	{ 3, "wednesday",	tDAY, 3 },
	{ 2, "thursday",	tDAY, 4 },
	{ 2, "friday",		tDAY, 5 },
	{ 2, "saturday",	tDAY, 6 },

	/* Timezones: Offsets are in minutes. */
	{ 0, "gmt",  tZONE,     HOUR( 0) }, /* Greenwich Mean */
	{ 0, "ut",   tZONE,     HOUR( 0) }, /* Universal (Coordinated) */
	{ 0, "utc",  tZONE,     HOUR( 0) },
	{ 0, "wet",  tZONE,     HOUR( 0) }, /* Western European */
	{ 0, "bst",  tDAYZONE,  HOUR( 0) }, /* British Summer */
	{ 0, "wat",  tZONE,     HOUR( 1) }, /* West Africa */
	{ 0, "at",   tZONE,     HOUR( 2) }, /* Azores */
	/* { 0, "bst", tZONE, HOUR( 3) }, */ /* Brazil Standard: Conflict */
	/* { 0, "gst", tZONE, HOUR( 3) }, */ /* Greenland Standard: Conflict*/
	{ 0, "nft",  tZONE,     HOUR(3)+30 }, /* Newfoundland */
	{ 0, "nst",  tZONE,     HOUR(3)+30 }, /* Newfoundland Standard */
	{ 0, "ndt",  tDAYZONE,  HOUR(3)+30 }, /* Newfoundland Daylight */
	{ 0, "ast",  tZONE,     HOUR( 4) }, /* Atlantic Standard */
	{ 0, "adt",  tDAYZONE,  HOUR( 4) }, /* Atlantic Daylight */
	{ 0, "est",  tZONE,     HOUR( 5) }, /* Eastern Standard */
	{ 0, "edt",  tDAYZONE,  HOUR( 5) }, /* Eastern Daylight */
	{ 0, "cst",  tZONE,     HOUR( 6) }, /* Central Standard */
	{ 0, "cdt",  tDAYZONE,  HOUR( 6) }, /* Central Daylight */
	{ 0, "mst",  tZONE,     HOUR( 7) }, /* Mountain Standard */
	{ 0, "mdt",  tDAYZONE,  HOUR( 7) }, /* Mountain Daylight */
	{ 0, "pst",  tZONE,     HOUR( 8) }, /* Pacific Standard */
	{ 0, "pdt",  tDAYZONE,  HOUR( 8) }, /* Pacific Daylight */
	{ 0, "yst",  tZONE,     HOUR( 9) }, /* Yukon Standard */
	{ 0, "ydt",  tDAYZONE,  HOUR( 9) }, /* Yukon Daylight */
	{ 0, "hst",  tZONE,     HOUR(10) }, /* Hawaii Standard */
	{ 0, "hdt",  tDAYZONE,  HOUR(10) }, /* Hawaii Daylight */
	{ 0, "cat",  tZONE,     HOUR(10) }, /* Central Alaska */
	{ 0, "ahst", tZONE,     HOUR(10) }, /* Alaska-Hawaii Standard */
	{ 0, "nt",   tZONE,     HOUR(11) }, /* Nome */
	{ 0, "idlw", tZONE,     HOUR(12) }, /* Intl Date Line West */
	{ 0, "cet",  tZONE,     -HOUR(1) }, /* Central European */
	{ 0, "met",  tZONE,     -HOUR(1) }, /* Middle European */
	{ 0, "mewt", tZONE,     -HOUR(1) }, /* Middle European Winter */
	{ 0, "mest", tDAYZONE,  -HOUR(1) }, /* Middle European Summer */
	{ 0, "swt",  tZONE,     -HOUR(1) }, /* Swedish Winter */
	{ 0, "sst",  tDAYZONE,  -HOUR(1) }, /* Swedish Summer */
	{ 0, "fwt",  tZONE,     -HOUR(1) }, /* French Winter */
	{ 0, "fst",  tDAYZONE,  -HOUR(1) }, /* French Summer */
	{ 0, "eet",  tZONE,     -HOUR(2) }, /* Eastern Eur, USSR Zone 1 */
	{ 0, "bt",   tZONE,     -HOUR(3) }, /* Baghdad, USSR Zone 2 */
	{ 0, "it",   tZONE,     -HOUR(3)-30 },/* Iran */
	{ 0, "zp4",  tZONE,     -HOUR(4) }, /* USSR Zone 3 */
	{ 0, "zp5",  tZONE,     -HOUR(5) }, /* USSR Zone 4 */
	{ 0, "ist",  tZONE,     -HOUR(5)-30 },/* Indian Standard */
	{ 0, "zp6",  tZONE,     -HOUR(6) }, /* USSR Zone 5 */
	/* { 0, "nst",  tZONE, -HOUR(6.5) }, */ /* North Sumatra: Conflict */
	/* { 0, "sst", tZONE, -HOUR(7) }, */ /* So Sumatra, USSR 6: Conflict */
	{ 0, "wast", tZONE,     -HOUR(7) }, /* West Australian Standard */
	{ 0, "wadt", tDAYZONE,  -HOUR(7) }, /* West Australian Daylight */
	{ 0, "jt",   tZONE,     -HOUR(7)-30 },/* Java (3pm in Cronusland!)*/
	{ 0, "cct",  tZONE,     -HOUR(8) }, /* China Coast, USSR Zone 7 */
	{ 0, "jst",  tZONE,     -HOUR(9) }, /* Japan Std, USSR Zone 8 */
	{ 0, "cast", tZONE,     -HOUR(9)-30 },/* Central Australian Std */
	{ 0, "cadt", tDAYZONE,  -HOUR(9)-30 },/* Central Australian Daylt */
	{ 0, "east", tZONE,     -HOUR(10) }, /* Eastern Australian Std */
	{ 0, "eadt", tDAYZONE,  -HOUR(10) }, /* Eastern Australian Daylt */
	{ 0, "gst",  tZONE,     -HOUR(10) }, /* Guam Std, USSR Zone 9 */
	{ 0, "nzt",  tZONE,     -HOUR(12) }, /* New Zealand */
	{ 0, "nzst", tZONE,     -HOUR(12) }, /* New Zealand Standard */
	{ 0, "nzdt", tDAYZONE,  -HOUR(12) }, /* New Zealand Daylight */
	{ 0, "idle", tZONE,     -HOUR(12) }, /* Intl Date Line East */

	{ 0, "dst",  tDST,		0 },

	/* Time units. */
	{ 4, "years",		tMONTH_UNIT,	12 },
	{ 5, "months",		tMONTH_UNIT,	1 },
	{ 9, "fortnights",	tSEC_UNIT,	14 * 24 * 60 * 60 },
	{ 4, "weeks",		tSEC_UNIT,	7 * 24 * 60 * 60 },
	{ 3, "days",		tSEC_UNIT,	1 * 24 * 60 * 60 },
	{ 4, "hours",		tSEC_UNIT,	60 * 60 },
	{ 3, "minutes",		tSEC_UNIT,	60 },
	{ 3, "seconds",		tSEC_UNIT,	1 },

	/* Relative-time words. */
	{ 0, "tomorrow",	tSEC_UNIT,	1 * 24 * 60 * 60 },
	{ 0, "yesterday",	tSEC_UNIT,	-1 * 24 * 60 * 60 },
	{ 0, "today",		tSEC_UNIT,	0 },
	{ 0, "now",		tSEC_UNIT,	0 },
	{ 0, "last",		tUNUMBER,	-1 },
	{ 0, "this",		tSEC_UNIT,	0 },
	{ 0, "next",		tUNUMBER,	2 },
	{ 0, "first",		tUNUMBER,	1 },
	{ 0, "1st",		tUNUMBER,	1 },
/*	{ 0, "second",		tUNUMBER,	2 }, */
	{ 0, "2nd",		tUNUMBER,	2 },
	{ 0, "third",		tUNUMBER,	3 },
	{ 0, "3rd",		tUNUMBER,	3 },
	{ 0, "fourth",		tUNUMBER,	4 },
	{ 0, "4th",		tUNUMBER,	4 },
	{ 0, "fifth",		tUNUMBER,	5 },
	{ 0, "5th",		tUNUMBER,	5 },
	{ 0, "sixth",		tUNUMBER,	6 },
	{ 0, "seventh",		tUNUMBER,	7 },
	{ 0, "eighth",		tUNUMBER,	8 },
	{ 0, "ninth",		tUNUMBER,	9 },
	{ 0, "tenth",		tUNUMBER,	10 },
	{ 0, "eleventh",	tUNUMBER,	11 },
	{ 0, "twelfth",		tUNUMBER,	12 },
	{ 0, "ago",		tAGO,		1 },

	/* Military timezones. */
	{ 0, "a",	tZONE,	HOUR(  1) },
	{ 0, "b",	tZONE,	HOUR(  2) },
	{ 0, "c",	tZONE,	HOUR(  3) },
	{ 0, "d",	tZONE,	HOUR(  4) },
	{ 0, "e",	tZONE,	HOUR(  5) },
	{ 0, "f",	tZONE,	HOUR(  6) },
	{ 0, "g",	tZONE,	HOUR(  7) },
	{ 0, "h",	tZONE,	HOUR(  8) },
	{ 0, "i",	tZONE,	HOUR(  9) },
	{ 0, "k",	tZONE,	HOUR( 10) },
	{ 0, "l",	tZONE,	HOUR( 11) },
	{ 0, "m",	tZONE,	HOUR( 12) },
	{ 0, "n",	tZONE,	HOUR(- 1) },
	{ 0, "o",	tZONE,	HOUR(- 2) },
	{ 0, "p",	tZONE,	HOUR(- 3) },
	{ 0, "q",	tZONE,	HOUR(- 4) },
	{ 0, "r",	tZONE,	HOUR(- 5) },
	{ 0, "s",	tZONE,	HOUR(- 6) },
	{ 0, "t",	tZONE,	HOUR(- 7) },
	{ 0, "u",	tZONE,	HOUR(- 8) },
	{ 0, "v",	tZONE,	HOUR(- 9) },
	{ 0, "w",	tZONE,	HOUR(-10) },
	{ 0, "x",	tZONE,	HOUR(-11) },
	{ 0, "y",	tZONE,	HOUR(-12) },
	{ 0, "z",	tZONE,	HOUR(  0) },

	/* End of table. */
	{ 0, NULL,	0,	0 }
};




/* ARGSUSED */
static int
yyerror(const char *s)
{
	(void)s;
	return 0;
}

static time_t
ToSeconds(time_t Hours, time_t Minutes, time_t Seconds)
{
	if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
		return -1;
	if (Hours < 0 || Hours > 23)
		return -1;
	return (Hours * 60L + Minutes) * 60L + Seconds;
}


/* Year is either
 * A number from 0 to 99, which means a year from 1970 to 2069, or
 * The actual year (>=100).  */
static time_t
Convert(time_t Month, time_t Day, time_t Year,
	time_t Hours, time_t Minutes, time_t Seconds, DSTMODE DSTmode)
{
	static int DaysInMonth[12] = {
		31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	time_t	tod;
	time_t	Julian;
	int	i;

	if (Year < 69)
		Year += 2000;
	else if (Year < 100)
		Year += 1900;
	DaysInMonth[1] = Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0)
	    ? 29 : 28;
	/* Checking for 2038 bogusly assumes that time_t is 32 bits.  But
	   I'm too lazy to try to check for time_t overflow in another way.  */
	if (Year < EPOCH || Year > 2038
	    || Month < 1 || Month > 12
	    /* Lint fluff:  "conversion from long may lose accuracy" */
	    || Day < 1 || Day > DaysInMonth[(int)--Month])
		return -1;

	Julian = Day - 1;
	for (i = 0; i < Month; i++)
		Julian += DaysInMonth[i];
	for (i = EPOCH; i < Year; i++)
		Julian += 365 + (i % 4 == 0);
	Julian *= SECSPERDAY;
	Julian += yyTimezone * 60L;
	if ((tod = ToSeconds(Hours, Minutes, Seconds)) < 0)
		return -1;
	Julian += tod;
	if (DSTmode == DSTon
	    || (DSTmode == DSTmaybe && localtime(&Julian)->tm_isdst))
		Julian -= 60 * 60;
	return Julian;
}


static time_t
DSTcorrect(time_t Start, time_t Future)
{
	time_t	StartDay;
	time_t	FutureDay;

	StartDay = (localtime(&Start)->tm_hour + 1) % 24;
	FutureDay = (localtime(&Future)->tm_hour + 1) % 24;
	return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(time_t Start, time_t DayOrdinal, time_t DayNumber)
{
	struct tm	*tm;
	time_t	now;

	now = Start;
	tm = localtime(&now);
	now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
	now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
	return DSTcorrect(Start, now);
}


static time_t
RelativeMonth(time_t Start, time_t RelMonth)
{
	struct tm	*tm;
	time_t	Month;
	time_t	Year;

	if (RelMonth == 0)
		return 0;
	tm = localtime(&Start);
	Month = 12 * (tm->tm_year + 1900) + tm->tm_mon + RelMonth;
	Year = Month / 12;
	Month = Month % 12 + 1;
	return DSTcorrect(Start,
	    Convert(Month, (time_t)tm->tm_mday, Year,
		(time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
		DSTmaybe));
}

static int
yylex(void)
{
	char	c;
	char	buff[64];

	for ( ; ; ) {
		while (isspace((unsigned char)*yyInput))
			yyInput++;

		/* Skip parenthesized comments. */
		if (*yyInput == '(') {
			int Count = 0;
			do {
				c = *yyInput++;
				if (c == '\0')
					return c;
				if (c == '(')
					Count++;
				else if (c == ')')
					Count--;
			} while (Count > 0);
			continue;
		}

		/* Try the next token in the word table first. */
		/* This allows us to match "2nd", for example. */
		{
			char *src = yyInput;
			const struct TABLE *tp;
			unsigned i = 0;

			/* Force to lowercase and strip '.' characters. */
			while (*src != '\0'
			    && (isalnum((unsigned char)*src) || *src == '.')
			    && i < sizeof(buff)-1) {
				if (*src != '.') {
					if (isupper((unsigned char)*src))
						buff[i++] = tolower((unsigned char)*src);
					else
						buff[i++] = *src;
				}
				src++;
			}
			buff[i++] = '\0';

			/*
			 * Find the first match.  If the word can be
			 * abbreviated, make sure we match at least
			 * the minimum abbreviation.
			 */
			for (tp = TimeWords; tp->name; tp++) {
				size_t abbrev = tp->abbrev;
				if (abbrev == 0)
					abbrev = strlen(tp->name);
				if (strlen(buff) >= abbrev
				    && strncmp(tp->name, buff, strlen(buff))
				    	== 0) {
					/* Skip over token. */
					yyInput = src;
					/* Return the match. */
					yylval.Number = tp->value;
					return tp->type;
				}
			}
		}

		/*
		 * Not in the word table, maybe it's a number.  Note:
		 * Because '-' and '+' have other special meanings, I
		 * don't deal with signed numbers here.
		 */
		if (isdigit((unsigned char)(c = *yyInput))) {
			for (yylval.Number = 0; isdigit((unsigned char)(c = *yyInput++)); )
				yylval.Number = 10 * yylval.Number + c - '0';
			yyInput--;
			return (tUNUMBER);
		}

		return (*yyInput++);
	}
}

#define TM_YEAR_ORIGIN 1900

/* Yield A - B, measured in seconds.  */
static long
difftm (struct tm *a, struct tm *b)
{
	int ay = a->tm_year + (TM_YEAR_ORIGIN - 1);
	int by = b->tm_year + (TM_YEAR_ORIGIN - 1);
	int days = (
		/* difference in day of year */
		a->tm_yday - b->tm_yday
		/* + intervening leap days */
		+  ((ay >> 2) - (by >> 2))
		-  (ay/100 - by/100)
		+  ((ay/100 >> 2) - (by/100 >> 2))
		/* + difference in years * 365 */
		+  (long)(ay-by) * 365
		);
	return (60*(60*(24*days + (a->tm_hour - b->tm_hour))
	    + (a->tm_min - b->tm_min))
	    + (a->tm_sec - b->tm_sec));
}

time_t
get_date(char *p)
{
	struct tm	*tm;
	struct tm	gmt, *gmt_ptr;
	time_t		Start;
	time_t		tod;
	time_t		nowtime;
	long		tzone;

	memset(&gmt, 0, sizeof(gmt));
	yyInput = p;

	(void)time (&nowtime);

	gmt_ptr = gmtime (&nowtime);
	if (gmt_ptr != NULL) {
		/* Copy, in case localtime and gmtime use the same buffer. */
		gmt = *gmt_ptr;
	}

	if (! (tm = localtime (&nowtime)))
		return -1;

	if (gmt_ptr != NULL)
		tzone = difftm (&gmt, tm) / 60;
	else
		/* This system doesn't understand timezones; fake it. */
		tzone = 0;
	if(tm->tm_isdst)
		tzone += 60;

	yyYear = tm->tm_year + 1900;
	yyMonth = tm->tm_mon + 1;
	yyDay = tm->tm_mday;
	yyTimezone = tzone;
	yyDSTmode = DSTmaybe;
	yyHour = 0;
	yyMinutes = 0;
	yySeconds = 0;
	yyRelSeconds = 0;
	yyRelMonth = 0;
	yyHaveDate = 0;
	yyHaveDay = 0;
	yyHaveRel = 0;
	yyHaveTime = 0;
	yyHaveZone = 0;

	if (yyparse()
	    || yyHaveTime > 1 || yyHaveZone > 1
	    || yyHaveDate > 1 || yyHaveDay > 1)
		return -1;

	if (yyHaveDate || yyHaveTime || yyHaveDay) {
		Start = Convert(yyMonth, yyDay, yyYear,
		    yyHour, yyMinutes, yySeconds, yyDSTmode);
		if (Start < 0)
			return -1;
	} else {
		Start = nowtime;
		if (!yyHaveRel)
			Start -= ((tm->tm_hour * 60L + tm->tm_min) * 60L) + tm->tm_sec;
	}

	Start += yyRelSeconds;
	Start += RelativeMonth(Start, yyRelMonth);

	if (yyHaveDay && !yyHaveDate) {
		tod = RelativeDate(Start, yyDayOrdinal, yyDayNumber);
		Start += tod;
	}

	/* Have to do *something* with a legitimate -1 so it's
	 * distinguishable from the error return value.  (Alternately
	 * could set errno on error.) */
	return Start == -1 ? 0 : Start;
}


#if	defined(TEST)

/* ARGSUSED */
int
main(int argc, char **argv)
{
    time_t	d;

    while (*++argv != NULL) {
	    (void)printf("Input: %s\n", *argv);
	    d = get_date(*argv);
	    if (d == -1)
		    (void)printf("Bad format - couldn't convert.\n");
	    else
		    (void)printf("Output: %s\n", ctime(&d));
    }
    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
#line 823 "getdate.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
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

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

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
    if ((yyn = yydefred[yystate])) goto yyreduce;
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
#line 109 "getdate.y"
{ yyHaveTime++; }
break;
case 4:
#line 110 "getdate.y"
{ yyHaveZone++; }
break;
case 5:
#line 111 "getdate.y"
{ yyHaveDate++; }
break;
case 6:
#line 112 "getdate.y"
{ yyHaveDay++; }
break;
case 7:
#line 113 "getdate.y"
{ yyHaveRel++; }
break;
case 9:
#line 117 "getdate.y"
{
		/* "7am" */
		yyHour = yyvsp[-1].Number;
		yyMinutes = 0;
		yySeconds = 0;
		if (yyHour == 12)
			yyHour = 0;
	}
break;
case 10:
#line 125 "getdate.y"
{
		/* "7pm" */
		yyHour = yyvsp[-1].Number;
		yyMinutes = 0;
		yySeconds = 0;
		if (yyHour < 12)
			yyHour += 12;
	}
break;
case 11:
#line 133 "getdate.y"
{
		/* "7:12:18" "19:17" */
	}
break;
case 12:
#line 136 "getdate.y"
{
		/* "12:20:13am" */
		if (yyHour == 12)
			yyHour = 0;
	}
break;
case 13:
#line 141 "getdate.y"
{
		/* "7:12pm" */
		if (yyHour < 12)
			yyHour += 12;
	}
break;
case 14:
#line 146 "getdate.y"
{
		/* "7:14+0700" */
		yyDSTmode = DSToff;
		yyTimezone = - (yyvsp[0].Number % 100 + (yyvsp[0].Number / 100) * 60);
	}
break;
case 15:
#line 151 "getdate.y"
{
		/* "19:14:12-0530" */
		yyDSTmode = DSToff;
		yyTimezone = + (yyvsp[0].Number % 100 + (yyvsp[0].Number / 100) * 60);
	}
break;
case 16:
#line 158 "getdate.y"
{
		yyHour = yyvsp[-2].Number;
		yyMinutes = yyvsp[0].Number;
		yySeconds = 0;
	}
break;
case 17:
#line 163 "getdate.y"
{
		yyHour = yyvsp[-4].Number;
		yyMinutes = yyvsp[-2].Number;
		yySeconds = yyvsp[0].Number;
	}
break;
case 18:
#line 170 "getdate.y"
{
		yyTimezone = yyvsp[0].Number;
		yyDSTmode = DSToff;
	}
break;
case 19:
#line 174 "getdate.y"
{
		yyTimezone = yyvsp[0].Number;
		yyDSTmode = DSTon;
	}
break;
case 20:
#line 178 "getdate.y"
{
		yyTimezone = yyvsp[-1].Number;
		yyDSTmode = DSTon;
	}
break;
case 21:
#line 184 "getdate.y"
{
		yyDayOrdinal = 1;
		yyDayNumber = yyvsp[0].Number;
	}
break;
case 22:
#line 188 "getdate.y"
{
		/* "tue," "wednesday," */
		yyDayOrdinal = 1;
		yyDayNumber = yyvsp[-1].Number;
	}
break;
case 23:
#line 193 "getdate.y"
{
		/* "second tues" "3 wed" */
		yyDayOrdinal = yyvsp[-1].Number;
		yyDayNumber = yyvsp[0].Number;
	}
break;
case 24:
#line 200 "getdate.y"
{
		/* "1/15" */
		yyMonth = yyvsp[-2].Number;
		yyDay = yyvsp[0].Number;
	}
break;
case 25:
#line 205 "getdate.y"
{
		if (yyvsp[-4].Number >= 13) {
			/* First number is big:  2004/01/29, 99/02/17 */
			yyYear = yyvsp[-4].Number;
			yyMonth = yyvsp[-2].Number;
			yyDay = yyvsp[0].Number;
		} else if ((yyvsp[0].Number >= 13) || (yyvsp[-2].Number >= 13)) {
			/* Last number is big:  01/07/98 */
			/* Middle number is big:  01/29/04 */
			yyMonth = yyvsp[-4].Number;
			yyDay = yyvsp[-2].Number;
			yyYear = yyvsp[0].Number;
		} else {
			/* No significant clues: 02/03/04 */
			yyMonth = yyvsp[-4].Number;
			yyDay = yyvsp[-2].Number;
			yyYear = yyvsp[0].Number;
		}
	}
break;
case 26:
#line 224 "getdate.y"
{
		/* ISO 8601 format.  yyyy-mm-dd.  */
		yyYear = yyvsp[-4].Number;
		yyMonth = yyvsp[-2].Number;
		yyDay = yyvsp[0].Number;
	}
break;
case 27:
#line 230 "getdate.y"
{
		if (yyvsp[-4].Number > 31) {
			/* e.g. 1992-Jun-17 */
			yyYear = yyvsp[-4].Number;
			yyMonth = yyvsp[-2].Number;
			yyDay = yyvsp[0].Number;
		} else {
			/* e.g. 17-JUN-1992.  */
			yyDay = yyvsp[-4].Number;
			yyMonth = yyvsp[-2].Number;
			yyYear = yyvsp[0].Number;
		}
	}
break;
case 28:
#line 243 "getdate.y"
{
		/* "May 3" */
		yyMonth = yyvsp[-1].Number;
		yyDay = yyvsp[0].Number;
	}
break;
case 29:
#line 248 "getdate.y"
{
		/* "June 17, 2001" */
		yyMonth = yyvsp[-3].Number;
		yyDay = yyvsp[-2].Number;
		yyYear = yyvsp[0].Number;
	}
break;
case 30:
#line 254 "getdate.y"
{
		/* "12 Sept" */
		yyDay = yyvsp[-1].Number;
		yyMonth = yyvsp[0].Number;
	}
break;
case 31:
#line 259 "getdate.y"
{
		/* "12 Sept 1997" */
		yyDay = yyvsp[-2].Number;
		yyMonth = yyvsp[-1].Number;
		yyYear = yyvsp[0].Number;
	}
break;
case 32:
#line 267 "getdate.y"
{
		yyRelSeconds = -yyRelSeconds;
		yyRelMonth = -yyRelMonth;
	}
break;
case 34:
#line 274 "getdate.y"
{
		/* "-3 hours" */
		yyRelSeconds -= yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 35:
#line 278 "getdate.y"
{
		/* "+1 minute" */
		yyRelSeconds += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 36:
#line 282 "getdate.y"
{
		/* "1 day" */
		yyRelSeconds += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 37:
#line 286 "getdate.y"
{
		/* "hour" */
		yyRelSeconds += yyvsp[0].Number;
	}
break;
case 38:
#line 290 "getdate.y"
{
		/* "-3 months" */
		yyRelMonth -= yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 39:
#line 294 "getdate.y"
{
		/* "+5 years" */
		yyRelMonth += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 40:
#line 298 "getdate.y"
{
		/* "2 years" */
		yyRelMonth += yyvsp[-1].Number * yyvsp[0].Number;
	}
break;
case 41:
#line 302 "getdate.y"
{
		/* "6 months" */
		yyRelMonth += yyvsp[0].Number;
	}
break;
case 42:
#line 308 "getdate.y"
{
		if (yyHaveTime && yyHaveDate && !yyHaveRel)
			yyYear = yyvsp[0].Number;
		else {
			if(yyvsp[0].Number>10000) {
				/* "20040301" */
				yyHaveDate++;
				yyDay= (yyvsp[0].Number)%100;
				yyMonth= (yyvsp[0].Number/100)%100;
				yyYear = yyvsp[0].Number/10000;
			}
			else {
				/* "513" is same as "5:13" */
				yyHaveTime++;
				if (yyvsp[0].Number < 100) {
					yyHour = yyvsp[0].Number;
					yyMinutes = 0;
				}
				else {
					yyHour = yyvsp[0].Number / 100;
					yyMinutes = yyvsp[0].Number % 100;
				}
				yySeconds = 0;
			}
		}
	}
break;
#line 1339 "getdate.c"
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
