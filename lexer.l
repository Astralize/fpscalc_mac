%{
#include <string.h>
#include "y.tab.h"
extern int Line_number;
extern void ceprintf(char* fmt, ...);
%}

%option nounput

ws	[ \t]
comment	!.*
letter	[a-zA-Z_]
digit	[0-9]
alnum	{letter}|{digit}
id	{alnum}*{letter}{alnum}*
number	{digit}*(\.{digit}+)?(E[+\-]?{digit}+)?
%%
system		return SYS;
var		return VAR;
floor		return FLOOR;
ceiling		return CEILING;
sigma		return SIGMA;
all		return ALL;
ep		return EP;
hp		return HP;
lp		return LP;
min		return MIN;
max		return MAX;
priority	return PRIORITY;
blocking	return BLOCKING;
tasks		return TASKS;
semaphores	return SEMAPHORES;
semaphore	return SEMAPHORE;
scalar		return SCALAR;
indexed		return INDEXED;
initialise	return INITIALISE;
formulas	return FORMULAS;
declarations	return DECLARATIONS;
"{"		return LEFTBRACE;
"}"		return RIGHTBRACE;
"="		return ASSIGN;
"("		return LEFTPAREN;
")"		return RIGHTPAREN;
"["		return LEFTHOOK;
"]"		return RIGHTHOOK;
","		return COMMA;
";"		return SEMICOLON;
"*"		return STAR;
"/"		return SLASH;
"+"		return PLUS;
"-"		return MINUS;
"i"		return INDEXVAR;
"j"		return N_INDEXVAR;
\n		++Line_number;
{comment}	;
{ws}		;
{number}	{ yylval.string = strdup(yytext); return NUMBER; }
{id}		{ yylval.string = strdup(yytext); return ID; }
"--"+"-"*	ceprintf("Expression too negative '%s'", yytext);
.		ceprintf("Illegal character '%s'", yytext);
