#ifndef YYERRCODE
#define YYERRCODE 256
#endif

#define tAGO 257
#define tDAY 258
#define tDAYZONE 259
#define tAMPM 260
#define tMONTH 261
#define tMONTH_UNIT 262
#define tSEC_UNIT 263
#define tUNUMBER 264
#define tZONE 265
#define tDST 266
typedef union {
    time_t		Number;
} YYSTYPE;
extern YYSTYPE yylval;
