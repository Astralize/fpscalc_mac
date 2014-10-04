YACC=bison -y -d
SRCS = LUF.c fpsmain.c
OBJS = y.tab.o lex.yy.o LUF.o fpsmain.o
LEX=flex
CC=gcc
LOADLIBS = -ll -lm
DEPEND = depend
PROG = fpscalc
CFLAGS = -D__USE_FIXED_PROTOTYPES__ -Wall $(INCLUDE) $(DEBUG)

all: $(PROG)

y.tab.c y.tab.h: parser.y 
	$(YACC) parser.y

lex.yy.c: lexer.l y.tab.h
	$(LEX) lexer.l

$(PROG): y.tab.o lex.yy.o LUF.o fpsmain.o
	$(CC) $(CFLAGS) -o fpscalc $(OBJS) $(LOADLIBS) 

clean:
	rm -f *.o *~ core y.tab.* lex.yy.c

cleaner: clean
	rm fpscalc

depend:
	/bin/rm -f $(DEPEND)
	$(CC) -MM $(CFLAGS) $(SRCS) > $(DEPEND)
