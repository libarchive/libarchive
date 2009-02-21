#ifndef YYERRCODE
#define YYERRCODE 256
#endif

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
typedef union {
    time_t		Number;
} YYSTYPE;
extern YYSTYPE yylval;
