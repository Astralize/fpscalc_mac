#ifndef _LUF_h_
#define _LUF_h_

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

extern int Line_number;			/* Defined in LUF.c */

#ifdef __STDC__
int yyerror(char*);
void ceprintf(char*, ...);
char* newlabel(char*);
#else
int ceprintf();
char* newlabel();
#endif

#endif
