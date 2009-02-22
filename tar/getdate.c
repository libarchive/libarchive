/*
 * This code is in the public domain and has no copyright.
 *
 * This is a plain C recursive-descent translation of an old
 * public-domain YACC grammar that has been used for parsing dates in
 * very many open-source projects.
 *
 * Since the original authors were generous enough to donate their
 * work to the public domain, I feel compelled to match their
 * generosity.
 *
 * Tim Kientzle, February 2009.
 */

/*
 * Header comment from original getdate.y:
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

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* This file defines a single public function. */
time_t get_date(char *);


#define EPOCH		1970
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)

/* Daylight-savings mode:  on, off, or not yet known. */
enum DSTMODE { DSTon, DSToff, DSTmaybe };
/* Meridian:  am or pm. */
enum { tAM, tPM };
/* Token types returned by nexttoken() */
enum TOKEN { tAGO = 260, tDAY, tDAYZONE, tAMPM, tMONTH, tMONTH_UNIT, tSEC_UNIT,
       tUNUMBER, tZONE, tDST };
/* nexttoken() returns a token and an optional value. */
struct token { enum TOKEN token; time_t value; };

/*
**  Global variables.  We could get rid of most of these by using a good
**  union as the yacc stack.  (This routine was originally written before
**  yacc had the %union construct.)  Maybe someday; right now we only use
**  the %union very rarely.
*/
struct gdstate {
	struct token *tokenp;
	enum DSTMODE	DSTmode;
	time_t	DayOrdinal;
	time_t	DayNumber;
	int	HaveDate;
	int	HaveDay;
	int	HaveRel;
	int	HaveTime;
	int	HaveZone;
	time_t	Timezone;
	time_t	Day;
	time_t	Hour;
	time_t	Minutes;
	time_t	Month;
	time_t	Seconds;
	time_t	Year;
	time_t	RelMonth;
	time_t	RelSeconds;
} global;

/*
 * A series of functions that recognize certain common time phrases.
 * Each function returns 1 if it managed to make sense of some of the
 * tokens, zero otherwise.
 */

static int
baretimephrase(void)
{
	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == ':'
	    && global.tokenp[2].token == tUNUMBER
	    && global.tokenp[3].token == ':'
	    && global.tokenp[4].token == tUNUMBER) {
		++global.HaveTime;
		global.Hour = global.tokenp[0].value;
		global.Minutes = global.tokenp[2].value;
		global.Seconds = global.tokenp[4].value;
		global.tokenp += 5;
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == ':'
	    && global.tokenp[2].token == tUNUMBER) {
		++global.HaveTime;
		global.Hour = global.tokenp[0].value;
		global.Minutes = global.tokenp[2].value;
		global.Seconds = 0;
		global.tokenp += 3;
		return 1;
	}

	return 0;
}

static int
timephrase(void)
{
	if (baretimephrase()) {
		if (global.tokenp[0].token == tAMPM) {
			/* "7:12pm", "12:20:13am" */
			if (global.Hour == 12)
				global.Hour = 0;
			if (global.tokenp[0].value == tPM)
				global.Hour += 12;
			global.tokenp += 1;
			return 1;
		}
		if (global.tokenp[0].token == '+' && global.tokenp[1].token == tUNUMBER) {
			/* "7:14+0700" */
			global.DSTmode = DSToff;
			global.Timezone = - (global.tokenp[1].value % 100
			    + (global.tokenp[1].value / 100) * 60);
			global.tokenp += 2;
			return 1;
		}
		if (global.tokenp[0].token == '-' && global.tokenp[1].token == tUNUMBER) {
			/* "19:14:12-0530" */
			global.DSTmode = DSToff;
			global.Timezone = + (global.tokenp[1].value % 100
			    + (global.tokenp[1].value / 100) * 60);
			global.tokenp += 2;
			return 1;
		}
		/* "7:12:18" "19:17" */
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER && global.tokenp[1].token == tAMPM) {
		/* "7am" */
		global.HaveTime++;
		global.Hour = global.tokenp[0].value;
		if (global.Hour == 12)
			global.Hour = 0;
		global.Minutes = 0;
		global.Seconds = 0;
		if (global.tokenp[1].value == tPM)
			global.Hour += 12;
		global.tokenp += 2;
		return 1;
	}

	return 0;
}

static int
zonephrase(void)
{
	if (global.tokenp[0].token == tZONE
	    && global.tokenp[1].token == tDST) {
		global.HaveZone++;
		global.Timezone = global.tokenp[0].value;
		global.DSTmode = DSTon;
		global.tokenp += 1;
		return 1;
	}

	if (global.tokenp[0].token == tZONE) {
		global.HaveZone++;
		global.Timezone = global.tokenp[0].value;
		global.DSTmode = DSToff;
		global.tokenp += 1;
		return 1;
	}

	if (global.tokenp[0].token == tDAYZONE) {
		global.HaveZone++;
		global.Timezone = global.tokenp[0].value;
		global.DSTmode = DSTon;
		global.tokenp += 1;
		return 1;
	}
	return 0;
}


static int
datephrase(void)
{
	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == '/'
	    && global.tokenp[2].token == tUNUMBER
	    && global.tokenp[3].token == '/'
	    && global.tokenp[4].token == tUNUMBER) {
		global.HaveDate++;
		if (global.tokenp[0].value >= 13) {
			/* First number is big:  2004/01/29, 99/02/17 */
			global.Year = global.tokenp[0].value;
			global.Month = global.tokenp[2].value;
			global.Day = global.tokenp[4].value;
		} else if ((global.tokenp[4].value >= 13) || (global.tokenp[2].value >= 13)) {
			/* Last number is big:  01/07/98 */
			/* Middle number is big:  01/29/04 */
			global.Month = global.tokenp[0].value;
			global.Day = global.tokenp[2].value;
			global.Year = global.tokenp[4].value;
		} else {
			/* No significant clues: 02/03/04 */
			global.Month = global.tokenp[0].value;
			global.Day = global.tokenp[2].value;
			global.Year = global.tokenp[4].value;
		}
		global.tokenp += 5;
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == '/'
	    && global.tokenp[2].token == tUNUMBER) {
		/* "1/15" */
		global.HaveDate++;
		global.Month = global.tokenp[0].value;
		global.Day = global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == '-'
	    && global.tokenp[2].token == tUNUMBER
	    && global.tokenp[3].token == '-'
	    && global.tokenp[4].token == tUNUMBER) {
		/* ISO 8601 format.  yyyy-mm-dd.  */
		global.HaveDate++;
		global.Year = global.tokenp[0].value;
		global.Month = global.tokenp[2].value;
		global.Day = global.tokenp[4].value;
		global.tokenp += 5;
		return 1;
	}


	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == '-'
	    && global.tokenp[2].token == tMONTH
	    && global.tokenp[3].token == '-'
	    && global.tokenp[4].token == tUNUMBER) {
		global.HaveDate++;
		if (global.tokenp[0].value > 31) {
			/* e.g. 1992-Jun-17 */
			global.Year = global.tokenp[0].value;
			global.Month = global.tokenp[2].value;
			global.Day = global.tokenp[4].value;
		} else {
			/* e.g. 17-JUN-1992.  */
			global.Day = global.tokenp[0].value;
			global.Month = global.tokenp[2].value;
			global.Year = global.tokenp[4].value;
		}
		global.tokenp += 5;
		return 1;
	}

	if (global.tokenp[0].token == tMONTH
	    && global.tokenp[1].token == tUNUMBER
	    && global.tokenp[2].token == ','
	    && global.tokenp[3].token == tUNUMBER) {
		/* "June 17, 2001" */
		global.HaveDate++;
		global.Month = global.tokenp[0].value;
		global.Day = global.tokenp[1].value;
		global.Year = global.tokenp[3].value;
		global.tokenp += 4;
		return 1;
	}

	if (global.tokenp[0].token == tMONTH
	    && global.tokenp[1].token == tUNUMBER) {
		/* "May 3" */
		global.HaveDate++;
		global.Month = global.tokenp[0].value;
		global.Day = global.tokenp[1].value;
		global.tokenp += 2;
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == tMONTH
	    && global.tokenp[2].token == tUNUMBER) {
		/* "12 Sept 1997" */
		global.HaveDate++;
		global.Day = global.tokenp[0].value;
		global.Month = global.tokenp[1].value;
		global.Year = global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}

	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == tMONTH) {
		/* "12 Sept" */
		global.HaveDate++;
		global.Day = global.tokenp[0].value;
		global.Month = global.tokenp[1].value;
		global.tokenp += 2;
		return 1;
	}


	return 0;
}

static int
relunitphrase(void)
{
	if (global.tokenp[0].token == '-'
	    && global.tokenp[1].token == tUNUMBER
	    && global.tokenp[2].token == tSEC_UNIT) {
		/* "-3 hours" */
		global.HaveRel++;
		global.RelSeconds -= global.tokenp[1].value * global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}
	if (global.tokenp[0].token == '+'
	    && global.tokenp[1].token == tUNUMBER
	    && global.tokenp[2].token == tSEC_UNIT) {
		/* "+1 minute" */
		global.HaveRel++;
		global.RelSeconds += global.tokenp[1].value * global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}
	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == tSEC_UNIT) {
		/* "1 day" */
		global.HaveRel++;
		global.RelSeconds += global.tokenp[1].value * global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}
	if (global.tokenp[0].token == '-'
	    && global.tokenp[1].token == tUNUMBER
	    && global.tokenp[2].token == tMONTH_UNIT) {
		/* "-3 months" */
		global.HaveRel++;
		global.RelMonth -= global.tokenp[1].value * global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}
	if (global.tokenp[0].token == '+'
	    && global.tokenp[1].token == tUNUMBER
	    && global.tokenp[2].token == tMONTH_UNIT) {
		/* "+5 years" */
		global.HaveRel++;
		global.RelMonth += global.tokenp[1].value * global.tokenp[2].value;
		global.tokenp += 3;
		return 1;
	}
	if (global.tokenp[0].token == tUNUMBER
	    && global.tokenp[1].token == tMONTH_UNIT) {
		/* "2 years" */
		global.HaveRel++;
		global.RelMonth += global.tokenp[0].value * global.tokenp[1].value;
		global.tokenp += 2;
		return 1;
	}
	if (global.tokenp[0].token == tSEC_UNIT) {
		/* "now", "tomorrow" */
		global.HaveRel++;
		global.RelSeconds += global.tokenp[0].value;
		++global.tokenp;
		return 1;
	}
	if (global.tokenp[0].token == tMONTH_UNIT) {
		/* "month" */
		global.HaveRel++;
		global.RelMonth += global.tokenp[0].value;
		global.tokenp += 1;
		return 1;
	}
	return 0;
}

static int
dayphrase(void)
{
	if (global.tokenp[0].token == tDAY) {
		/* "tues", "wednesday," */
		global.HaveDay++;
		global.DayOrdinal = 1;
		global.DayNumber = global.tokenp[0].value;
		global.tokenp += 1;
		if (global.tokenp[0].token == ',')
			global.tokenp += 1;
		return 1;
	}
	if (global.tokenp[0].token == tUNUMBER
		&& global.tokenp[1].token == tDAY) {
		/* "second tues" "3 wed" */
		global.HaveDay++;
		global.DayOrdinal = global.tokenp[0].value;
		global.DayNumber = global.tokenp[1].value;
		global.tokenp += 2;
		return 1;
	}
	return 0;
}

static int
phrase(void)
{
	if (timephrase())
		return 1;
	if (zonephrase())
		return 1;
	if (datephrase())
		return 1;
	if (dayphrase())
		return 1;
	if (relunitphrase()) {
		if (global.tokenp[0].token == tAGO) {
			global.RelSeconds = -global.RelSeconds;
			global.RelMonth = -global.RelMonth;
			global.tokenp += 1;
		}
		return 1;
	}

	/* Bare numbers sometimes have meaning. */
	if (global.tokenp[0].token == tUNUMBER) {
		if (global.HaveTime && global.HaveDate && !global.HaveRel) {
			global.Year = global.tokenp[0].value;
			global.tokenp += 1;
			return 1;
		}

		if(global.tokenp[0].value > 10000) {
			/* "20040301" */
			global.HaveDate++;
			global.Day= (global.tokenp[0].value)%100;
			global.Month= (global.tokenp[0].value/100)%100;
			global.Year = global.tokenp[0].value/10000;
			global.tokenp += 1;
			return 1;
		}

		if (global.tokenp[0].value < 24) {
			global.HaveTime++;
			global.Hour = global.tokenp[0].value;
			global.Minutes = 0;
			global.Seconds = 0;
			global.tokenp += 1;
			return 1;
		}

		if ((global.tokenp[0].value / 100 < 24)
		    && (global.tokenp[0].value % 100 < 60)) {
			/* "513" is same as "5:13" */
			global.Hour = global.tokenp[0].value / 100;
			global.Minutes = global.tokenp[0].value % 100;
			global.Seconds = 0;
			global.tokenp += 1;
			return 1;
		}
	}

	return 0;
}

static struct LEXICON {
	size_t		abbrev;
	const char	*name;
	int		type;
	time_t		value;
} const TimeWords[] = {
	/* am/pm */
	{ 0, "am",		tAMPM,	tAM },
	{ 0, "pm",		tAMPM,	tPM },

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
	time_t Hours, time_t Minutes, time_t Seconds, enum DSTMODE DSTmode)
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
	Julian += global.Timezone * 60L;
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
nexttoken(char **in, time_t *value)
{
	char	c;
	char	buff[64];

	for ( ; ; ) {
		while (isspace((unsigned char)**in))
			++*in;

		/* Skip parenthesized comments. */
		if (**in == '(') {
			int Count = 0;
			do {
				c = *(*in)++;
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
			char *src = *in;
			const struct LEXICON *tp;
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
					*in = src;
					/* Return the match. */
					*value = tp->value;
					return tp->type;
				}
			}
		}

		/*
		 * Not in the word table, maybe it's a number.  Note:
		 * Because '-' and '+' have other special meanings, I
		 * don't deal with signed numbers here.
		 */
		if (isdigit((unsigned char)(c = **in))) {
			for (*value = 0; isdigit((unsigned char)(c = *(*in)++)); )
				*value = 10 * *value + c - '0';
			(*in)--;
			return (tUNUMBER);
		}

		return *(*in)++;
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
	struct token	tokens[256];
	struct token	*lasttoken;
	struct tm	*tm;
	struct tm	gmt, *gmt_ptr;
	time_t		Start;
	time_t		tod;
	time_t		nowtime;
	long		tzone;

	memset(&gmt, 0, sizeof(gmt));

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

	global.Year = tm->tm_year + 1900;
	global.Month = tm->tm_mon + 1;
	global.Day = tm->tm_mday;
	global.Timezone = tzone;
	global.DSTmode = DSTmaybe;
	global.Hour = 0;
	global.Minutes = 0;
	global.Seconds = 0;
	global.RelSeconds = 0;
	global.RelMonth = 0;
	global.HaveDate = 0;
	global.HaveDay = 0;
	global.HaveRel = 0;
	global.HaveTime = 0;
	global.HaveZone = 0;

	lasttoken = tokens;
	while ((lasttoken->token = nexttoken(&p, &lasttoken->value)) != 0) {
		++lasttoken;
		if (lasttoken > tokens + 255)
			return -1;
	}
	global.tokenp = tokens;

	while (global.tokenp < lasttoken) {
		if (!phrase())
			return -1;
	}

	if (global.HaveTime > 1 || global.HaveZone > 1
	    || global.HaveDate > 1 || global.HaveDay > 1)
		return -1;

	if (global.HaveDate || global.HaveTime || global.HaveDay) {
		Start = Convert(global.Month, global.Day, global.Year,
		    global.Hour, global.Minutes, global.Seconds, global.DSTmode);
		if (Start < 0)
			return -1;
	} else {
		Start = nowtime;
		if (!global.HaveRel)
			Start -= ((tm->tm_hour * 60L + tm->tm_min) * 60L) + tm->tm_sec;
	}

	Start += global.RelSeconds;
	Start += RelativeMonth(Start, global.RelMonth);

	if (global.HaveDay && !global.HaveDate) {
		tod = RelativeDate(Start, global.DayOrdinal, global.DayNumber);
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
