

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math.h"

#include "y.tab.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
	extern const char *StringDuplication( const char *value );
#ifdef __cplusplus
}
#endif __cplusplus


/* Pre-define the YY_INPUT macro so that it interprets yyin as a character pointer  */
/* and reads the input from this string rather than from a file pointer.  We MUST	*/
/* use the yyrestart() routine to set this file pointer to start with.				*/
#ifndef UNUSED_YY_INPUT
#define UNUSED_YY_INPUT(buf,result,max_size) \
			if (((char *) yyin) == (char *) NULL) { \
				result = 0; \
			} /* End if - then */ \
			else { \
				if (*(char *) yyin =='\\0') { \
					result = 0; \
				} /* End if - then */ \
				else { \
					strncpy(buf,(char *) yyin, max_size); \
					buf[max_size] = '\\0'; \
					result = strlen(buf); \
					yyin += result; \
					if (result < max_size) yyin = NULL; \
				} /* End if else */ \
			} /* End if - else */
#endif

%}


%option noyywrap
%option case-insensitive



DIGIT	([0-9])
ALPHA	([A-Za-z_])
WORD	([A-Za-z_0-9:; \t.]*)
WSPACE	([ \t])
ANY		(.)

/* Declare an exclusive 'QUOTED' condition to read quoted strings.	*/
/* Where a string has quotes around it, the contents of the			*/
/* string must be interpreted literally rather than as any of the	*/
/* special tokens we're usually looking for.						*/

%x COMMENT

%%

	char	l_szCommentStringBuffer[4096];
	char	*l_pszQuotedString;


[\(]			{	BEGIN(COMMENT);
				l_pszQuotedString = l_szCommentStringBuffer;
				memset( l_szCommentStringBuffer, 0, sizeof(l_szCommentStringBuffer));
			}

<COMMENT>([A-Za-z0-9 <>\\/#%:;.,_\-\[\]\t]+)	{
				strcat( l_szCommentStringBuffer, yytext );
				}

<COMMENT>[\)]			{
				/* We've found the closing quote so start the scanner	*/
				/* back into it's normal (INITIAL) condition			*/
				yylval.string = StringDuplication(l_szCommentStringBuffer);
				BEGIN(INITIAL);
				return COMMENTS;
			}

([nN][0-9]+) {
				yylval.string = StringDuplication(yytext);
				return LINE_NUMBER;
			}

([oO][0-9]+) {
				yylval.string = StringDuplication(yytext);
				return O_CODE;
			}


([eE][qQ]) {	return EQ;	}
([nN][eE]) {	return NE;	}
([lL][tT]) {	return LT;	}
([gG][tT]) {	return GT;	}

([aA][nN][dD])	{	return AND;	}
([oO][rR])		 {	return OR;	}

([iI][fF]) {	return IF;	}
([eE][lL][sS][eE]) {	return ELSE;	}
([eE][nN][dD][iI][fF]) {	return ENDIF;	}

([mM](0)?0) {	return M00;	}
([mM](0)?1) {	return M01;	}
([mM](0)?2) {	return M02;	}
([mM](0)?3) {	return M03;	}
([mM](0)?4) {	return M04;	}
([mM](0)?5) {	return M05;	}
([mM](0)?6) {	return M06;	}
([mM](0)?7) {	return M07;	}
([mM](0)?8) {	return M08;	}
([mM](0)?9) {	return M09;	}
([mM]60)	{	return M60;	}
([mM]1[0-9][0-9])	{	return USER_DEFINED_M_CODES;	}


([gG](0)?4) {	return G04;	}
([gG]10) {	return G10;	}
([gG]17) {	return G17;	}
([gG]18) {	return G18;	}
([gG]19) {	return G19;	}
([gG]20) {	return G20;	}
([gG]21) {	return G21;	}
([gG]28\.1) {	return G28_1;	}
([gG]28) {	return G28;	}
([gG]30\.1) {	return G30_1;	}
([gG]30) {	return G30;	}
([gG]33\.1) {	return G33_1;	}
([gG]33) {	return G33;	}
([gG]38\.2) {	return G38_2;	}
([gG]38\.3) {	return G38_3;	}
([gG]38\.4) {	return G38_4;	}
([gG]38\.5) {	return G38_5;	}
([gG]40) {	return G40;	}
([gG]41) {	return G41;	}
([gG]42) {	return G42;	}
([gG]43\.1) {	return G43_1;	}
([gG]43) {	return G43;	}
([gG]49) {	return G49;	}
([gG]53) {	return G53;	}
([gG]54) {	return G54;	}
([gG]55) {	return G55;	}
([gG]56) {	return G56;	}
([gG]57) {	return G57;	}
([gG]58) {	return G58;	}
([gG]59\.1) {	return G59_1;	}
([gG]59\.2) {	return G59_2;	}
([gG]59\.3) {	return G59_3;	}
([gG]59) {	return G59;	}
([gG]61\.1) {	return G61_1;	}
([gG]61) {	return G61;	}
([gG]64) {	return G64;	}
([gG]80) {	return G80;	}
([gG]81) {	return G81;	}
([gG]82) {	return G82;	}
([gG]83) {	return G83;	}
([gG]84) {	return G84;	}
([gG]85) {	return G85;	}
([gG]89) {	return G89;	}
([gG]90) {	return G90;	}
([gG]91) {	return G91;	}
([gG]92\.1) {	return G92_1;	}
([gG]92\.2) {	return G92_2;	}
([gG]92\.3) {	return G92_3;	}
([gG]92) {	return G92;	}
([gG]98) {	return G98;	}
([gG]99) {	return G99;	}

([Ff]) {	return FEEDRATE;	}
([sS]) {	return SPINDLE_SPEED;	}

([gG](0)?3) { return G03; }
([gG](0)?2) { return G02; }
([gG](0)?1) { return G01; }
([gG](0)?0) { return G00; }

([Xx])			{ return(X);}
([Yy])			{ return(Y);}
([Zz])			{ return(Z);}
([Aa])			{ return(A);}
([Bb])			{ return(B);}
([Cc])			{ return(C);}
([Uu])			{ return(U);}
([Vv])			{ return(V);}
([Ww])			{ return(W);}
([Ii])			{ return(I);}
([Jj])			{ return(J);}
([Kk])			{ return(K);}

([Ll])			{ return(L);}
([Pp])			{ return(P);}
([Qq])			{ return(Q);}
([Rr])			{ return(R);}

([Aa][Ss][Ii][Nn])	{ return(ASIN); }
([Aa][Cc][Oo][Ss])	{ return(ACOS); }
([Aa][Tt][Aa][Nn])	{ return(ATAN); }

([Ss][Ii][Nn])	{ return(SIN); }
([Cc][Oo][Ss])	{ return(COS); }
([Tt][Aa][Nn])	{ return(TAN); }

([Aa][Bb][Ss])	{ return(ABS); }
([Ss][Qq][Rr][Tt])	{ return(SQRT); }

([tT])([0-9]+)	{
					yylval.integer = atoi(yytext+1);
					return(TOOL_SELECTION);
				}

([dD])([0-9]+)	{
					yylval.integer = atoi(yytext+1);
					return(TOOL_NUMBER);
				}



(#)			{
				return(HASH);
			}

(<)([a-zA-Z0-9_]+)(>) {
				yylval.string = StringDuplication(yytext);
				return(NAME);
			}

([+-])*([0-9]+)(\.)([0-9]+)	{
				yylval.floating_point_number = atof(yytext);
				return(NUMBER_TOKEN);
			}

([+-])*([0-9]+)	{
				yylval.floating_point_number = atof(yytext);
				return(NUMBER_TOKEN);
			}

(\[)		{ return(OPEN_SQUARE_BRACKET); }
(\])		{ return(CLOSE_SQUARE_BRACKET); }
(\+)		{ return(PLUS); }
(-)			{ return(MINUS); }
(\*)		{ return(MULTIPLY); }
(\/)		{ return(DIVIDE); }
(\=)		{ return(ASSIGNMENT); }

[\n]		{ return END_BLOCK;	}



%%







/*
 * yyerror() --YACC/Lex's standard error handler.
 *
 * Notes:
 * The makefile uses 'sed' to rename this routine to "adhoc_error".
 * This routine should probably never be called, as (hopefully) all
 * the unit specification errors are explicitly matched in the grammar.
 * Consequently, this routine is reduced to a nullproc.
 */
void yyerror(char *s)
{
    (void)s;
#if 0
    printf("%s at \"%s\". (unit id=\"%s\")\n", s, yytext, GetUnitID());
#endif /* 0 */
}







