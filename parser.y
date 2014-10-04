%{
#include "LUF.h"
#include "fpsmain.h"
#include <string.h>
#include <stdlib.h>

int yylex(void);

%}

%union {
  int integer;
  char *string;
  double floating;
}

%token <string> NUMBER ID
%token DECLARATION LEFTPAREN RIGHTPAREN LEFTHOOK RIGHTHOOK SYS
%token COMMA SEMICOLON STAR SLASH PLUS MINUS ASSIGN INITIALISE
%token VAR BLOCKING PRIORITY CEILING FLOOR SIGMA HP LP EP ALL
%token MIN MAX TASKS SEMAPHORE INDEXVAR N_INDEXVAR SCALAR INDEXED
%token LEFTBRACE RIGHTBRACE FORMULAS DECLARATIONS SEMAPHORES

%left PLUS MINUS
%left STAR SLASH
%left SIGMA
%left FLOOR CEILING MIN MAX
%right UMINUS

%type <floating> NumberExpression

%%

TaskSet	: GloabalDecs Systems
	  {
	    return(system_registry(GET_SYSTEM) + 1);
	  }
	;

GloabalDecs
	: GloabalDecs GloabalDec
	| /* empty */
	;

GloabalDec
	: INDEXED GlobalIndexVarList SEMICOLON
	| SCALAR GlobalScalarVarList SEMICOLON
	| TASKS GlobalNameList SEMICOLON
	;

GlobalIndexVarList
	:  GlobalIndexVarList COMMA ID
	    {
	      declare_global_variable($3, INDEXED_VAR);
	    }
	| ID
	    {
	      declare_global_variable($1, INDEXED_VAR);
	    }
	;

GlobalScalarVarList
	: ID COMMA GlobalScalarVarList
	    {
	      declare_global_variable($1, SCALAR_VAR);
	    }
	| ID
	    {
	      declare_global_variable($1, SCALAR_VAR);
	    }
	;

GlobalNameList
	: GlobalNameList COMMA ID
	    {
	      declare_global_task($3);
	    }
	| ID
	    {
	      declare_global_task($1);
	    }
	;

Systems	: Systems SystemDef
	| SystemDef
	;

SystemDef
	: SYS ID LEFTBRACE
	    {
	      declare_system($2);
	    }
	  DeclareBlock SemBlock Initialisation FormulaBlock RIGHTBRACE
	;

DeclareBlock
	: DECLARATIONS LEFTBRACE Declarations RIGHTBRACE

Declarations
	: Declaration Declarations
	| /* empty */
	;

Declaration
	: INDEXED IndexVarList SEMICOLON
	| SCALAR ScalarVarList SEMICOLON
	| BLOCKING ID SEMICOLON
	    {
	      declare_variable($2, BLOCKING_VAR);
	    }
	| PRIORITY ID SEMICOLON
	    {
	      declare_variable($2, PRIORITY_VAR);
	    }
	| TASKS NameList SEMICOLON
	;

SemBlock
	: SEMAPHORES LEFTBRACE Semaphores RIGHTBRACE
	| /* empty */

Semaphores
	: Semaphore Semaphores
	| /* empty */
	;

Semaphore
	: SEMAPHORE LEFTPAREN ID COMMA ID COMMA NUMBER RIGHTPAREN SEMICOLON
	    {
	      add_semaphore($3, $5, strtod($7, NULL));
	    }
	;


IndexVarList
	:  IndexVarList COMMA ID
	    {
	      declare_variable($3, INDEXED_VAR);
	    }
	| ID
	    {
	      declare_variable($1, INDEXED_VAR);
	    }
	;

ScalarVarList
	: ID COMMA ScalarVarList
	    {
	      declare_variable($1, SCALAR_VAR);
	    }
	| ID
	    {
	      declare_variable($1, SCALAR_VAR);
	    }
	;

NameList
	: NameList COMMA ID
	    {
	      declare_task($3);
	    }
	| ID
	    {
	      declare_task($1);
	    }
	;

Initialisation
	: INITIALISE LEFTBRACE Inits RIGHTBRACE
	| /* empty */
	;

Inits	: Init Inits
	| /* empty */
	;

Init	: ID LEFTHOOK ID RIGHTHOOK ASSIGN NumberExpression SEMICOLON
	  {
	    init_variable($1, $3, $6);
	  }
	| ID LEFTHOOK INDEXVAR RIGHTHOOK ASSIGN NumberExpression SEMICOLON
	  {
	    init_variable($1, "i", $6);
	  }
	| ID ASSIGN NumberExpression SEMICOLON
	  {
	    init_variable($1, NULL, $3);
	  }

NumberExpression
	: NumberExpression PLUS NumberExpression
	  {
	    $$ = $1 + $3;
	  }
	| NumberExpression MINUS NumberExpression
	  {
	    $$ = $1 - $3;
	  }
	| NumberExpression STAR NumberExpression
	  {
	    $$ = $1 * $3;
	  }
	| NumberExpression SLASH NumberExpression
	  {
	    $$ = $1 / $3;
	  }
	| MINUS NumberExpression %prec UMINUS
	  {
	    $$ = -$2;
	  }
	| LEFTPAREN NumberExpression RIGHTPAREN
	  {
	    $$ = $2;
	  }
	| NUMBER
	  {
	    $$ = strtod($1, NULL);
	  }
	;

FormulaBlock
	: FORMULAS LEFTBRACE Formulas RIGHTBRACE
	;

Formulas
	: Formula Formulas
	| /* empty */
	;

Formula	: ID LEFTHOOK ID RIGHTHOOK ASSIGN
	  {
	    declare_formula($1, NEW_FORMULA);
	    declare_formula($3, FORMULA_INDEX);
	  }
	  SimpleExpression SEMICOLON
	  {
	    declare_formula(NULL, FORMULA_END);
	  }
	| ID LEFTHOOK INDEXVAR RIGHTHOOK ASSIGN
	  {
	    declare_formula($1, NEW_FORMULA);
	    declare_formula("i", FORMULA_INDEX);
	  }
	  Expression SEMICOLON
	  {
	    declare_formula(NULL, FORMULA_END);
	  }
	| ID LEFTHOOK N_INDEXVAR RIGHTHOOK ASSIGN
	  {
	    ceprintf("Secondary index (`j') used outside a summation\n");
	  }
	| ID ASSIGN
	  {
	    declare_formula($1, NEW_FORMULA);
	    declare_formula(NULL, FORMULA_INDEX);
	  }
	  SimpleExpression SEMICOLON
	  {
	    declare_formula(NULL, FORMULA_END);
	  }

Expression
	: Expression PLUS Expression
	  {
	    declare_formula("PLUS", FORMULA_OP);
	  }
	| Expression MINUS Expression
	  {
	    declare_formula("MINUS", FORMULA_OP);
	  }
	| Expression STAR Expression
	  {
	    declare_formula("MULTIPLY", FORMULA_OP);
	  }
	| Expression SLASH Expression
	  {
	    declare_formula("DIVIDE", FORMULA_OP);
	  }
	| MINUS Expression %prec UMINUS
	  {
	    declare_formula("UMINUS", FORMULA_OP);
	  }
	| FLOOR LEFTPAREN Expression RIGHTPAREN
	  {
	    declare_formula("FLOOR", FORMULA_OP);
	  }
	| CEILING LEFTPAREN Expression RIGHTPAREN
	  {
	    declare_formula("CEILING", FORMULA_OP);
	  }
	| SIGMA LEFTPAREN HP
	  {
	    declare_formula("SIGMA_HP", FORMULA_OP);
	  }
	  COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("END_SIGMA", FORMULA_OP);
	  }
	| SIGMA LEFTPAREN LP
	  {
	    declare_formula("SIGMA_LP", FORMULA_OP);
	  }
	  COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("END_SIGMA", FORMULA_OP);
	  }
	| SIGMA LEFTPAREN ALL
	  {
	    declare_formula("SIGMA_ALL", FORMULA_OP);
	  }
	  COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("END_SIGMA", FORMULA_OP);
	  }
	| SIGMA LEFTPAREN EP
	  {
	    declare_formula("SIGMA_EP", FORMULA_OP);
	  }
	  COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("END_SIGMA", FORMULA_OP);
	  }
	| MAX LEFTPAREN Expression COMMA Expression RIGHTPAREN
	  {
	    declare_formula("MAX", FORMULA_OP);
	  }
	| MIN LEFTPAREN Expression COMMA Expression RIGHTPAREN
	  {
	    declare_formula("MIN", FORMULA_OP);
	  }
	| LEFTPAREN Expression RIGHTPAREN {}
	| NUMBER
	  {
	    declare_formula($1, FORMULA_CONST);
	  }
	| ID
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula(NULL, FORMULA_INDEX);
	  }
	| ID LEFTHOOK ID RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula($3, FORMULA_INDEX);
	  }
	| ID LEFTHOOK INDEXVAR RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula("i", FORMULA_INDEX);
	  }
	| ID LEFTHOOK N_INDEXVAR RIGHTHOOK
	  {
	    ceprintf("Secondary index (`j') used outside a summation\n");
	  }
	;

SummationExpression
	: SummationExpression PLUS SummationExpression
	  {
	    declare_formula("PLUS", FORMULA_OP);
	  }

	| SummationExpression MINUS SummationExpression
	  {
	    declare_formula("MINUS", FORMULA_OP);
	  }
	| SummationExpression STAR SummationExpression
	  {
	    declare_formula("MULTIPLY", FORMULA_OP);
	  }
	| SummationExpression SLASH SummationExpression
	  {
	    declare_formula("DIVIDE", FORMULA_OP);
	  }
	| MINUS SummationExpression %prec UMINUS
	  {
	    declare_formula("UMINUS", FORMULA_OP);
	  }
	| FLOOR LEFTPAREN SummationExpression RIGHTPAREN
	  {
	    declare_formula("FLOOR", FORMULA_OP);
	  }
	| CEILING LEFTPAREN SummationExpression RIGHTPAREN
	  {
	    declare_formula("CEILING", FORMULA_OP);
	  }
	| SIGMA
	  {
	    ceprintf("Nested summation\n");
	  }
	| MAX LEFTPAREN SummationExpression COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("MAX", FORMULA_OP);
	  }
	| MIN LEFTPAREN SummationExpression COMMA SummationExpression RIGHTPAREN
	  {
	    declare_formula("MIN", FORMULA_OP);
	  }
	| LEFTPAREN SummationExpression RIGHTPAREN {}
	| NUMBER
	  {
	    declare_formula($1, FORMULA_CONST);
	  }
	| ID
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula(NULL, FORMULA_INDEX);
	  }
	| ID LEFTHOOK ID RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula($3, FORMULA_INDEX);
	  }
	| ID LEFTHOOK INDEXVAR RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula("i", FORMULA_INDEX);
	  }
	| ID LEFTHOOK N_INDEXVAR RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula("j", FORMULA_INDEX);
	  }
	;

SimpleExpression
	: SimpleExpression PLUS SimpleExpression
	  {
	    declare_formula("PLUS", FORMULA_OP);
	  }

	| SimpleExpression MINUS SimpleExpression
	  {
	    declare_formula("MINUS", FORMULA_OP);
	  }
	| SimpleExpression STAR SimpleExpression
	  {
	    declare_formula("MULTIPLY", FORMULA_OP);
	  }
	| SimpleExpression SLASH SimpleExpression
	  {
	    declare_formula("DIVIDE", FORMULA_OP);
	  }
	| MINUS SimpleExpression %prec UMINUS
	  {
	    declare_formula("UMINUS", FORMULA_OP);
	  }
	| FLOOR LEFTPAREN SimpleExpression RIGHTPAREN
	  {
	    declare_formula("FLOOR", FORMULA_OP);
	  }
	| CEILING LEFTPAREN SimpleExpression RIGHTPAREN
	  {
	    declare_formula("CEILING", FORMULA_OP);
	  }
	| MAX LEFTPAREN SimpleExpression COMMA SimpleExpression RIGHTPAREN
	  {
	    declare_formula("MAX", FORMULA_OP);
	  }
	| MIN LEFTPAREN SimpleExpression COMMA SimpleExpression RIGHTPAREN
	  {
	    declare_formula("MIN", FORMULA_OP);
	  }
	| LEFTPAREN SimpleExpression RIGHTPAREN {}
	| NUMBER
	  {
	    declare_formula($1, FORMULA_CONST);
	  }
	| ID LEFTHOOK INDEXVAR RIGHTHOOK
	  {
	    ceprintf("Index used in formula with non-indexed result\n");
	  }
	| ID LEFTHOOK N_INDEXVAR RIGHTHOOK
	  {
	    ceprintf("Index used in formula with non-indexed result\n");
	  }
	| ID LEFTHOOK ID RIGHTHOOK
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula($3, FORMULA_INDEX);
	  }
	| ID
	  {
	    declare_formula($1, FORMULA_VAR);
	    declare_formula(NULL, FORMULA_INDEX);
	  }
	;

