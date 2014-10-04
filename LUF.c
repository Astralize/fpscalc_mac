#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include "LUF.h"

/* Copyright 1996 Uppsala University
 * All rights reserved.
 *
 * The code is written by Mats Kindahl <matkin@docs.uu.se>
 *
 * This code is developed as part of a course in compiler construction
 * and can be used and redistributed freely as long as this copyright
 * notice is retained.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", THERE ARE *NO* WARRANTIES.
 */

/* Some helping functions */
#define _STR(x) #x			/* Turns 'x' into a string */
#define _VAL(x) _STR(x)			/* Expands 'x' and turns it */
					/* into a string */

/* Macro that expands to the string 'x' with file name and line number */
/* added.*/
#define MESSAGE(x) __FILE__ " line " _VAL(__LINE__) ": " #x
#define PMESSAGE(s, x) fputs(MESSAGE(x) "\n", s)

/* "When in danger when in doubt, run in circles, scream and shout" */
#define PANIC(m) do { PMESSAGE(stderr, m); abort(); } while(0)

/* Global variables */
int Line_number = 1;			/* Line number for use inside yyerror */

/*
 * ceprintf:
 *    This function is called whenever a error is encountered during
 *    the compilation. It will report the error on the error channel
 *    and call the function `parser_error' with `reason' to report a
 *    failure.
 */
void
  ceprintf(char* fmt, ...)
{
  char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  /*  vsprintf(buf, fmt, ap) > sizeof(buf); */
  vsprintf(buf, fmt, ap);
  yyerror(buf);

  va_end(ap);
}

/*
 * yyerror:
 *    A redefinition of yyerror as defined in 'liby.a'. This yyerror
 *    prints a line number before the error message. Also note that it
 *    prints a newline after the string, which means that no newline
 *    should be supplied.
 */
int
yyerror(char* str)
{
  fprintf(stderr, "line %d: %s\n", Line_number, str);
  exit(-1);
}


/*
 * newlabel:
 *    Function to generate new labels. Each time it is called, this
 *    function generates a new label by using the supplied prefix
 *    together with an internal counter. The counter gets increased
 *    for each call to the function.
 */
char*
newlabel(char* pfx)
{
  static int count = 0;
  char *buf = (char*) malloc(strlen(pfx)
			     + (sizeof(int) * 8 + 3) / 3);

  sprintf(buf, "%s%o", pfx, count++);

  return buf;
}
