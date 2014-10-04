/* fpscalc.c

   Created: August 4 1995 by Harmen van der Velde

   September 12: HFP - Extra error checking added, core dumps avoided.
   September 15: HFP - Direct indexable variables added.
   September 19: HFP - Multiple system hierarchy added.
   October   17: HFP - Floating exception error conquered and ruled out.
   November   5: HFP - Dynamic priority system recalculation implemented.

   In 1996:

   December  11: HFP - Yacc/Lex Parser added to ease usage of the program.

   In 1997:

   January   15: HFP - Semaphore ceilings and blocking factors have now
                       been implemented as floats (before integers).

*/

/* Includes */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>
#include "fpsmain.h"

/* Constants */
                       
#define MAXDOUBLE DBL_MAX
#define TRUE 1
#define FALSE 0
#define STACK_SIZE 100
#define MAX_SYSTEMS 10
#define MAX_VARIABLES 50
#define MAX_TASKS 50
#define MAX_FORMULAS 50
#define MAX_SEMAPHORES 50
#define INVALID -1
#define UNDEFINED -1
#define STRING_SIZE 80

/* Type defs */

typedef enum { DEF_FIELD, CONST_FIELD, OP_FIELD, VAR_FIELD } form_field_t;
typedef enum { SINGLE_INDEX, I_INDEX, J_INDEX, SCALAR } index_t;
typedef enum { PLUS, MINUS, MULTIPLY, DIVIDE, UMINUS, MIN, MAX, FLOOR, CEILING, SIGMA_HP, SIGMA_LP, SIGMA_ALL, SIGMA_EP, END_SIGMA } op_t;
typedef struct formula *formula_t;
typedef enum { GLOBAL_RESULT, LOCAL_RESULT } result_t;

struct formula {
  int field_type;
  union {
    union {
      int operation;
      double constant;
      struct {
	char variable_name[80];
	char variable_index_task[80];
	int index_type;
      } var_field;
    } op_field;
    struct {
      int index_type;
      char result_variable[80];
      char result_index_task[80];
    } definition;
  } func_field;
  formula_t next;
};

/* Prototypes */

void push(double, int);
double pop(int);
double parse_rpn(formula_t, int, int, int, int);
double sigma_hp(formula_t, int, int, int);
double sigma_lp(formula_t, int, int, int);
double sigma_all(formula_t, int, int, int);
double sigma_ep(formula_t, int, int, int);
double max(double, double);
double min(double, double);
int initialise();
void calculate_blocking(int);
int check_dynamic_blocking (int);
int priority_refresh(int);
char *get_word(char *, int);
char *cmpstr(char *, char *);
int get_variable_index(char *, int);
int get_global_variable_index(char *);
int get_task_index(char *, int);
int get_global_task_index(char *);
int check_variable(char *, int);
int check_global_variable(char *);
int check_task(char *, int);
int check_global_task(char *);
int calculate_task_set(int);
void output_variables(int);
char *get_next_line(char *);
extern int yyparse(void);
extern void ceprintf(char* fmt, ...);

/* Global variables */

double stack[MAX_SYSTEMS][STACK_SIZE];
int stack_pointer[MAX_SYSTEMS];
double variables[MAX_SYSTEMS][MAX_VARIABLES][MAX_TASKS];
double backup_vars[MAX_SYSTEMS][MAX_VARIABLES][MAX_TASKS];
double global_backup_vars[MAX_VARIABLES][MAX_TASKS];
double semaphores[MAX_SYSTEMS][MAX_SEMAPHORES][MAX_TASKS];
int variable_types[MAX_SYSTEMS][MAX_VARIABLES];
char system_names[MAX_SYSTEMS][STRING_SIZE];
char variable_names[MAX_SYSTEMS][MAX_VARIABLES][STRING_SIZE];
char task_names[MAX_SYSTEMS][MAX_TASKS][STRING_SIZE];
char semaphore_names[MAX_SYSTEMS][MAX_SEMAPHORES][STRING_SIZE];
formula_t formulas[MAX_SYSTEMS][MAX_FORMULAS];
int priority_variable[MAX_SYSTEMS];
int blocking_variable[MAX_SYSTEMS];
char global_variable_names[MAX_VARIABLES][STRING_SIZE];
char global_task_names[MAX_TASKS][STRING_SIZE];
int global_variable_types[MAX_VARIABLES];
double global_variables[MAX_VARIABLES][MAX_TASKS];
double ceiling[MAX_SYSTEMS][MAX_SEMAPHORES];
int no_tasks[MAX_SYSTEMS];
int no_vars[MAX_SYSTEMS];
int no_semaphores[MAX_SYSTEMS];
int no_formulas[MAX_SYSTEMS];
int dynamic_blocking[MAX_SYSTEMS];
int blocking[MAX_SYSTEMS];

FILE *input_file;
int verbose, no_global_variables, no_global_tasks;

int main(int argc,
	  char *argv[])
{
  int no_systems,
  current_system,
  current_formula,
  current_index,
  overall_change,
  result_var_index,
  result_var_type;
  char result_var_name[STRING_SIZE];
  double scalar_result;

  fprintf(stderr, "This is fpscalc version 2.02 1997\n");

  verbose = FALSE;
  if (argc > 2) {
    fprintf(stderr, "Usage: fpscalc [-v]\n");
    exit(-1);
  }

  if (argc == 2) {
    if (strcmp(argv[1], "-v") == 0)
      verbose = TRUE;
    else {
      fprintf(stderr, "Usage: fpscalc [-v]\n");
      exit(-1);
    }
  }

  no_systems = initialise();

  if (verbose)
    output_variables(no_systems);

  current_system = 0;
  overall_change = TRUE;

  while (overall_change || (current_system != 0)) {
    if (current_system == 0)
      overall_change = FALSE;
    overall_change = overall_change | calculate_task_set(current_system);

    /* Perform dynamic priority refreshing. After a fully converged calculation,
       check if the priorities have changed. If so, reset the system and
       calculate again with the new priorities. As long as it takes to ultra-
       mega-converge... */

    if (priority_variable[current_system] != INVALID)
      while (priority_refresh(current_system)) {
	if (dynamic_blocking[current_system])
	  calculate_blocking(current_system);
        overall_change = overall_change | calculate_task_set(current_system);
      }
    if (dynamic_blocking[current_system])
      calculate_blocking(current_system);

    if (++current_system == no_systems)
      current_system = 0;
  }

  if (!verbose)
    for (current_system = 0;current_system < no_systems;current_system++) {
      printf("\n\nSystem '%s'\n", system_names[current_system]);
      printf("-------------------\n");
      for (current_formula = 0;current_formula < no_formulas[current_system];current_formula++) {
	strcpy(result_var_name, formulas[current_system][current_formula]->func_field.definition.result_variable);
	if (check_variable(result_var_name, current_system)) {
	  result_var_index = get_variable_index(result_var_name, current_system);
	  result_var_type = variable_types[current_system][result_var_index];
	  scalar_result = variables[current_system][result_var_index][0];
	}
	else {
	  result_var_index = get_global_variable_index(result_var_name);
	  result_var_type = global_variable_types[result_var_index];
	  scalar_result = global_variables[result_var_index][0];
	}

	printf("\n");

	if (result_var_type == INDEXED_VAR) {
	  if (check_variable(result_var_name, current_system)) {
	    for (current_index = 0;current_index < no_tasks[current_system];current_index++)
	      printf("%s[%s] = %f\n", result_var_name, task_names[current_system][current_index], variables[current_system][result_var_index][current_index]);
	  }
	  else {
	    for (current_index = 0;current_index < no_global_tasks; current_index++)
	      printf("%s[%s] = %f\n", result_var_name, global_task_names[current_index], global_variables[result_var_index][current_index]);
	  }
	}
	else
	  printf("%s = %f\n", result_var_name, scalar_result);
      }
    }
    return 0;
}

/* CALCULATE_TASK

   This function iterates the formula(s) given in the input file as long
   as the results of the calculations keep changing
*/

int calculate_task_set(int current_system)
{
  int type,
  result_var_index,
  no_result_tasks,
  global_result_var,
  counter,
  first_iteration,
  current_formula,
  change,
  overall_change;
  double last_result, latest_result;
  char result_var_name[STRING_SIZE];

  current_formula = 0;
  change = TRUE;
  overall_change = FALSE;

  if (no_formulas[current_system] > 0) {
    while (change || (current_formula != 0))  {
      if (current_formula == 0)
      change = FALSE;

      strcpy(result_var_name, formulas[current_system][current_formula]->func_field.definition.result_variable);

      if (check_variable(result_var_name, current_system)) {
	result_var_index = get_variable_index(result_var_name, current_system);
	global_result_var = FALSE;
	no_result_tasks = no_tasks[current_system];
      }
      else {
	result_var_index = get_global_variable_index(result_var_name);
	global_result_var = TRUE;
	no_result_tasks = no_global_tasks;
      }

      type = formulas[current_system][current_formula]->func_field.definition.index_type;
      if (type == SINGLE_INDEX) {
	if (global_result_var)
	  counter = get_global_task_index(formulas[current_system][current_formula]->func_field.definition.result_index_task);
	else
	  counter = get_task_index(formulas[current_system][current_formula]->func_field.definition.result_index_task, current_system);
      }
      else
	counter = 0;
      /* Scalar variables are stored in field 0 of the variable array. */

      do {
	first_iteration = TRUE;
	while ((first_iteration) || (last_result != latest_result)) {
	  first_iteration = FALSE;
	  if (global_result_var) {
	    last_result = global_variables[result_var_index][counter];
	    latest_result = global_variables[result_var_index][counter] = parse_rpn(formulas[current_system][current_formula], counter, 0 , current_system, GLOBAL_RESULT);
	  }
	  else {
	    last_result = variables[current_system][result_var_index][counter];
	    latest_result = variables[current_system][result_var_index][counter] = parse_rpn(formulas[current_system][current_formula], counter, 0, current_system, LOCAL_RESULT);
	  }

	  if ((last_result != latest_result) && (current_formula != 0))
	    overall_change = change = TRUE;
	}

	/* If in verbose mode, output the results after each convergion */

	if (verbose && (counter == 0))
	  printf("\nSystem `%s'\n------------------\n\n", system_names[current_system]);
	if (verbose && (type == SCALAR) && (counter == 0))
	  printf("%s = %f\n", result_var_name, last_result);
	else if (verbose) {
	  if (global_result_var)
	    printf("%s[%s] = %f\n", result_var_name, global_task_names[counter], global_variables[result_var_index][counter]);
	  else
	    printf("%s[%s] = %f\n", result_var_name, task_names[current_system][counter], variables[current_system][result_var_index][counter]);
	}

      } while ((type == I_INDEX) && (++counter < no_result_tasks));
      if (++current_formula == no_formulas[current_system])
	current_formula = 0;
    }
  }
  return(overall_change);
}

/* PARSE_RPN

   This function evaluates a formula according to Reverse Polish Notation
   and returns a numerical value as its result
*/

double parse_rpn(formula_t position,
		 int index_i,
		 int index_j,
		 int current_system,
		 int result_type)
{
  formula_t current_position;
  int current_var, direct_index, stop_parsing;
  double carry;
  char var_name[STRING_SIZE];

  current_var = INVALID;
  direct_index = FALSE;
  current_position = position;
  stop_parsing = FALSE;

  while ((current_position != NULL) && (stop_parsing == FALSE))
    {
      switch(current_position->field_type)
	{
	case DEF_FIELD :
	  break;

	case CONST_FIELD :
	  push(current_position->func_field.op_field.constant, current_system);
	  break;

	case OP_FIELD :
	  switch (current_position->func_field.op_field.operation)
	    {
	    case MINUS :
	      carry = pop(current_system);
	      push(pop(current_system) - carry, current_system);
	      break;

	    case PLUS :
	      push(pop(current_system) + pop(current_system), current_system);
	      break;

	    case MAX :
	      push(max(pop(current_system), pop(current_system)), current_system);
	      break;

	    case MIN :
	      push(min(pop(current_system), pop(current_system)), current_system);
	      break;

	    case MULTIPLY :
	      push(pop(current_system) * pop(current_system), current_system);
	      break;

	    case DIVIDE :
	      carry = pop(current_system);
	      if (carry == 0.0) {
		fprintf(stderr, "Division by zero error in system `%s'.\n", system_names[current_system]);
		exit(-1);
	      }
	      push(pop(current_system) / carry, current_system);
	      break;

	    case UMINUS :
	      push(-pop(current_system), current_system);
	      break;

	    case CEILING :
	      push(ceil(pop(current_system)), current_system);
	      break;

	    case FLOOR :
	      push(floor(pop(current_system)), current_system);
	      break;

	    case SIGMA_HP :
	      push(sigma_hp(current_position, index_i, current_system, result_type), current_system);
	      do {
		current_position = current_position->next;
		while (current_position->field_type != OP_FIELD)
		  current_position = current_position->next;
	      } while (current_position->func_field.op_field.operation != END_SIGMA);
	      break;

	    case SIGMA_LP :
	      push(sigma_lp(current_position, index_i, current_system, result_type), current_system);
	      do {
		current_position = current_position->next;
		while (current_position->field_type != OP_FIELD)
		  current_position = current_position->next;
	      } while (current_position->func_field.op_field.operation != END_SIGMA);
	      break;

	    case SIGMA_EP :
	      push(sigma_ep(current_position, index_i, current_system, result_type), current_system);
	      do {
		current_position = current_position->next;
		while (current_position->field_type != OP_FIELD)
		  current_position = current_position->next;
	      } while (current_position->func_field.op_field.operation != END_SIGMA);
	      break;

	    case SIGMA_ALL :
	      push(sigma_all(current_position, index_i, current_system, result_type), current_system);
	      do {
		current_position = current_position->next;
		while (current_position->field_type != OP_FIELD)
		  current_position = current_position->next;
	      } while (current_position->func_field.op_field.operation != END_SIGMA);
	      break;

	    case END_SIGMA :
	      stop_parsing = TRUE; /* Return to the calling sigma function! */
	      break;
	    }
	  break;

	case VAR_FIELD :
	  strcpy(var_name, current_position->func_field.op_field.var_field.variable_name);
	  switch (current_position->func_field.op_field.var_field.index_type)
	    {
	    case SCALAR :
	      if (check_variable(var_name, current_system))
		push(variables[current_system][get_variable_index(var_name, current_system)][0], current_system);
	      else
		push(global_variables[get_global_variable_index(var_name)][0], current_system);
	      break;

	    case SINGLE_INDEX :
	      if (check_variable(var_name, current_system))
		push(variables[current_system][get_variable_index(var_name, current_system)][get_task_index(current_position->func_field.op_field.var_field.variable_index_task, current_system)], current_system);
	      else
		push(global_variables[get_global_variable_index(var_name)][get_global_task_index(current_position->func_field.op_field.var_field.variable_index_task)], current_system);

	      break;

	    case I_INDEX :
	      if (check_variable(var_name, current_system)) {
		if (result_type == GLOBAL_RESULT)
		  push(variables[current_system][get_variable_index(var_name, current_system)][get_task_index(global_task_names[index_i], current_system)], current_system);
		else
		  push(variables[current_system][get_variable_index(var_name, current_system)][index_i], current_system);
	      }
	      else {
		if (result_type == GLOBAL_RESULT)
		  push(global_variables[get_global_variable_index(var_name)][index_i], current_system);
		else
		  push(global_variables[get_global_variable_index(var_name)][get_global_task_index(task_names[current_system][index_i])], current_system);
	      }
	      break;

	    case J_INDEX :
	      if (check_variable(var_name, current_system)) {
		if (result_type == GLOBAL_RESULT)
		  push(variables[current_system][get_variable_index(var_name, current_system)][get_task_index(global_task_names[index_j], current_system)], current_system);
		else
		  push(variables[current_system][get_variable_index(var_name, current_system)][index_j], current_system);
	      }
	      else {
		if (result_type == GLOBAL_RESULT)
		  push(global_variables[get_global_variable_index(var_name)][index_j], current_system);
		else
		  push(global_variables[get_global_variable_index(var_name)][get_global_task_index(task_names[current_system][index_j])], current_system);
	      }
	      break;
	    }
	  break;
	}
      current_position = current_position->next;
    }
  return(pop(current_system));
}

/* SIGMA_HP

   Performs a summation of elements of the indexed variables with
   a higher priority (Pj) than the current task (Pi)
*/

double sigma_hp(formula_t position, int index_i, int current_system, int result_type)
{
  int counter, priority_var, max_index;
  double sigma;

  sigma = 0.0;
  priority_var = priority_variable[current_system];

  if (result_type == GLOBAL_RESULT)
    max_index = no_global_tasks;
  else
    max_index = no_tasks[current_system];

  for (counter = 0; counter < max_index; counter++)
    if ((counter != index_i) &&
	(variables[current_system][priority_var][counter] < variables[current_system][priority_var][index_i]))
      sigma += parse_rpn(position->next, index_i, counter, current_system, result_type);
  return(sigma);
}

/* SIGMA_LP

   Performs a summation of elements of the indexed variables with
   a lower priority (Pj) than the current task (Pi)
*/

double sigma_lp(formula_t position, int index_i, int current_system, int result_type)
{
  int counter, priority_var, max_index;
  double sigma;

  sigma = 0.0;
  priority_var = priority_variable[current_system];

  if (result_type == GLOBAL_RESULT)
    max_index = no_global_tasks;
  else
    max_index = no_tasks[current_system];

  for (counter = 0;counter < max_index;counter++)
    if (variables[current_system][priority_var][counter] > variables[current_system][priority_var][index_i])
      sigma += parse_rpn(position->next, index_i, counter, current_system, result_type);
  return(sigma);
}

/* SIGMA_EP

   Performs a summation of elements of the indexed variables with
   the same priority (Pj) as the current task (Pi)
   */

double sigma_ep(formula_t position, int index_i, int current_system, int result_type)
{
  int counter, priority_var, max_index;
  double sigma;

  sigma = 0.0;
  priority_var = priority_variable[current_system];

  if (result_type == GLOBAL_RESULT)
    max_index = no_global_tasks;
  else
    max_index = no_tasks[current_system];

  for (counter = 0;counter < max_index;counter++)
    if (variables[current_system][priority_var][counter] == variables[current_system][priority_var][index_i])
      sigma += parse_rpn(position->next, index_i, counter, current_system, result_type);
  return(sigma);
}

/* SIGMA_ALL

   Performs a summation of all elements of the indexed variables
*/

double sigma_all(formula_t position, int index_i, int current_system, int result_type)
{
  int counter, max_index;
  double sigma;

  sigma = 0.0;

  if (result_type == GLOBAL_RESULT)
    max_index = no_global_tasks;
  else
    max_index = no_tasks[current_system];

  for (counter = 0;counter < max_index;counter++)
    sigma += parse_rpn(position->next, index_i, counter, current_system, result_type);
  return(sigma);
}

/* INITIALISE

   Initialises the program by parsing the input file
*/

int initialise(int argc,
		char *argv[])
{
  int no_systems, counter, counter2, counter3; /* Intuitive variable names */

  for (counter = 0; counter < MAX_SYSTEMS; counter++) {
    for (counter2 = 0; counter2 < MAX_FORMULAS; counter2++)
      formulas[counter][counter2] = NULL;
    for (counter2 = 0; counter2 < MAX_VARIABLES; counter2++)
      for (counter3 = 0; counter3 < MAX_TASKS; counter3++) {
	variables[counter][counter2][counter3] = 0.0;
	backup_vars[counter][counter2][counter3] = 0.0;
      }
    blocking_variable[counter] = INVALID;
    priority_variable[counter] = INVALID;
    blocking[counter] = FALSE;
    no_formulas[counter] = 0;
    no_tasks[counter] = 0;
    no_vars[counter] = 0;
    no_semaphores[counter] = 0;
  }

  for (counter2 = 0; counter2 < MAX_VARIABLES; counter2++)
    for (counter3 = 0; counter3 < MAX_TASKS; counter3++) {
      global_variables[counter2][counter3] = 0.0;
      global_backup_vars[counter2][counter3] = 0.0;
    }
  no_global_variables = 0;
  no_global_tasks = 0;

  for (counter = 0;counter < MAX_SYSTEMS; counter++)
    for (counter2 = 0;counter2 < MAX_SEMAPHORES;counter2++)
      for (counter3 = 0;counter3 < MAX_TASKS;counter3++)
	semaphores[counter][counter2][counter3] = INVALID;

  no_systems = yyparse();
  for (counter = 0; counter < no_systems; counter++) {
    if (blocking[counter]) {
      calculate_blocking(counter);
      dynamic_blocking[counter] = check_dynamic_blocking(counter);
    }
    else if (blocking_variable[counter] != INVALID) {
      fprintf(stderr, "System `%s':\nBlocking factor variable declared, but no semaphores found.\n", system_names[counter]);
      exit(-1);
    }
  }

  return(no_systems);
}

/* Function system_registry keeps the index of the current
   system during parsing. */

int system_registry(int action)
{
  static int current_system = INVALID;

  switch (action)
    {
    case GET_SYSTEM :
      return(current_system);
      break;
    case NEXT_SYSTEM :
      if (current_system == INVALID)
	current_system = 0;
      else
	current_system++;
      return(current_system);
    break;
    }
  /* Shouldn't reach this point */
  return(-1);
}

/* Function declare_system is called by the parser to define systems
   and their specifics. */

void declare_system(char *system_name)
{
  int current_system;
  int counter;

  current_system = system_registry(GET_SYSTEM);
  for (counter = 0; counter <= current_system; counter++) {
    if (strcmp(system_name, system_names[counter]) == 0) {
      ceprintf("`%s' system already defined\n", system_name);
      exit(-1);
    }
  }
  current_system = system_registry(NEXT_SYSTEM);
  if (current_system == MAX_SYSTEMS) {
    ceprintf("maximum number of systems exceeded.\n");
    exit(-1);
  }
  strcpy(system_names[current_system], system_name);
}

/* Function declare_variables is called by the parser to set up variables
   within the system that is currently being defined. */

void declare_variable(char *variable_name, int variable_type)
{
  int current_system;

  current_system = system_registry(GET_SYSTEM);
  if (check_variable(variable_name, current_system) ||
      check_global_variable(variable_name)) {
    ceprintf("`%s' variable already defined\n", variable_name);
    exit(-1);
  }
  if (no_vars[current_system] == MAX_VARIABLES) {
    ceprintf("maximum number of variables exceeded.\n");
    exit(-1);
  }
  strcpy(variable_names[current_system][no_vars[current_system]], variable_name);

  if (variable_type == SCALAR_VAR)
    variable_types[current_system][no_vars[current_system]] = SCALAR_VAR;

  else if (variable_type == INDEXED_VAR)
    variable_types[current_system][no_vars[current_system]] = INDEXED_VAR;

  else if (variable_type == PRIORITY_VAR) {
    if (priority_variable[current_system] != INVALID) {
      ceprintf("`%s' priority variable already defined\n", variable_name);
      exit(-1);
    }
    else
      priority_variable[current_system] = no_vars[current_system];
    variable_types[current_system][no_vars[current_system]] = INDEXED_VAR;
  }

  else if (variable_type == BLOCKING_VAR) {
    if (blocking_variable[current_system] != INVALID) {
      ceprintf("`%s' blocking variable already defined\n", variable_name);
      exit(-1);
    }
    else
      blocking_variable[current_system] = no_vars[current_system];
    variable_types[current_system][no_vars[current_system]] = INDEXED_VAR;
  }
  ++no_vars[current_system];
}

void init_variable(char *variable_name, char *index_task, double init_value)
{
  int variable_index, task_index, current_system, counter, variable_type, global_variable;

  current_system = system_registry(GET_SYSTEM);

  if (check_variable(variable_name, current_system)) {
    variable_index = get_variable_index(variable_name, current_system);
    variable_type = variable_types[current_system][variable_index];
    global_variable = FALSE;
  }
  else if (check_global_variable(variable_name)) {
    variable_index = get_global_variable_index(variable_name);
    variable_type = global_variable_types[variable_index];
    global_variable = TRUE;
  }
  else {
    ceprintf("`%s' undefined variable\n", variable_name);
    exit(-1);
  }

  if (index_task == NULL) {

    if (variable_type != SCALAR_VAR) {
      ceprintf("`%s' variable used as scalar, but declared indexed\n", variable_name);
      exit(-1);
    }

    if (global_variable) {
      global_variables[variable_index][0] = init_value;
      global_backup_vars[variable_index][0] = init_value;
    }
    else {
      variables[current_system][variable_index][0] = init_value;
      backup_vars[current_system][variable_index][0] = init_value;
    }
  }

  else {

    if (variable_type != INDEXED_VAR) {
      ceprintf("`%s' variable used as indexed, but declared scalar\n", variable_name);
      exit(-1);
    }
    if (strcmp(index_task, "i") == 0) {
      if (global_variable)
	for (counter = 0; counter < no_global_tasks; counter++) {
	  global_variables[variable_index][counter] = init_value;
	  global_backup_vars[variable_index][counter] = init_value;
	}
      else
	for (counter = 0; counter < no_tasks[current_system]; counter++) {
	  variables[current_system][variable_index][counter] = init_value;
	  backup_vars[current_system][variable_index][counter] = init_value;
	}
    }
    else {
      if (global_variable) {
	if (!check_global_task(index_task)) {
	  ceprintf("`%s' task not previously declared\n", index_task);
	  exit(-1);
	}
	task_index = get_global_task_index(index_task);
	global_variables[variable_index][task_index] = init_value;
	global_backup_vars[variable_index][task_index] = init_value;
      }
      else {
	if (!check_task(index_task, current_system)) {
	  ceprintf("`%s' task not previously declared\n", index_task);
	  exit(-1);
	}
	task_index = get_task_index(index_task, current_system);
	variables[current_system][variable_index][task_index] = init_value;
	backup_vars[current_system][variable_index][task_index] = init_value;
      }
    }
  }
}

/* Function declare_task is called by the parser to set up task names
   within the system that is currently being defined. */

void declare_task(char *task_name)
{
  int current_system;

  current_system = system_registry(GET_SYSTEM);
  if (check_task(task_name, current_system)) {
    ceprintf("`%s' task already defined\n", task_name);
    exit(-1);
  }
  if (no_tasks[current_system] == MAX_TASKS) {
    ceprintf("maximum number of tasks exceeded.\n");
    exit(-1);
  }
  strcpy(task_names[current_system][no_tasks[current_system]], task_name);
  ++no_tasks[current_system];
}

/* Function declare_global_task is called by the parser to set up
   global task names. */

void declare_global_task(char *task_name)
{
  if (check_global_task(task_name)) {
    ceprintf("`%s' task already defined\n", task_name);
    exit(-1);
  }
  if (no_global_tasks == MAX_TASKS) {
    ceprintf("maximum number of tasks exceeded.\n");
    exit(-1);
  }
  strcpy(global_task_names[no_global_tasks], task_name);
  ++no_global_tasks;
}

/* Function add_semaphore is called by the parser to set up semaphores
   and their specifics. */

void add_semaphore(char *semaphore_name, char *task_name, double hold_time)
{
  int current_system, counter, task_index;

  current_system = system_registry(GET_SYSTEM);

  if (blocking_variable[current_system] == INVALID)
    ceprintf("Missing blocking factor variable declaration");

  if (priority_variable[current_system] == INVALID)
    ceprintf("Missing priority variable declaration");

  blocking[current_system] = TRUE;

  counter = 0;
  while ((counter < no_semaphores[current_system]) &&
	 (strcmp(semaphore_name, semaphore_names[current_system][counter]) != 0))
    counter++;

  if (counter == no_semaphores[current_system]) {
    if (counter == MAX_SEMAPHORES) {
      ceprintf("maximum number of semaphores exceeded.\n");
      exit(-1);
    }
    strcpy(semaphore_names[current_system][counter], semaphore_name);
    ++no_semaphores[current_system];
  }

  if (!check_task(task_name, current_system)) {
    ceprintf("`%s' task not previously declared\n", task_name);
    exit(-1);
  }
  task_index = get_task_index(task_name, current_system);
  if (semaphores[current_system][counter][task_index] != INVALID) {
    ceprintf("`%s' semaphore already defined held by this task.\n", task_name);
    exit(-1);
  }
  semaphores[current_system][counter][task_index] = hold_time;
}

void declare_global_variable(char *variable_name, int variable_type)
{
  if (no_global_variables == MAX_VARIABLES) {
    ceprintf("maximum number of variables exceeded.\n");
    exit(-1);
  }

  if (check_global_variable(variable_name)) {
    ceprintf("`%s' variable already defined\n", variable_name);
    exit(-1);
  }

  strcpy(global_variable_names[no_global_variables], variable_name);

  if (variable_type == SCALAR_VAR)
    global_variable_types[no_global_variables] = SCALAR_VAR;

  else if (variable_type == INDEXED_VAR)
    global_variable_types[no_global_variables] = INDEXED_VAR;

  ++no_global_variables;
}

/* Function declare_formula fills in the operands and operators
   of a formula. 'operand' is a pointer to either the result
   variable's name, a string containing the desired operation,
   a constant, or the result variable's index. The action field
   contains the sort of declaration to be performed. It can be
   either of the following:

   NEW_FORMULA: This will initiate the declaration of a new formula.
   operand will contain the name of the result variable.

   FORMULA_END: This will conclude the declaration of a formula.

   FORMULA_INDEX: This will declare the type of result variable.
   if a task name is given, the formula will index to only this
   task. If the reserved word 'i' is parsed, the parser
   won't call 'declare_formula' with this action value at all.
   The function assumes on first declaration, that the result
   variable is of the standard 'indexed' type, meaning that the
   formula will execute for each element of the result variable.

   FORMULA_OP: This will declare a certain operation the formula
   should execute, such as plus, minus etc...

   FORMULA_VAR: This will declare a variable to be used in the
   formula.

   FORMULA_CONST: This will declare a constant to be used in
   the formula.

   */

void declare_formula(char *operand, int action)
{
  static formula_t dummy = NULL;
  static formula_t current_formula;
  int current_system, counter, variable_type;
  char variable_name[80];

  current_system = system_registry(GET_SYSTEM);

  switch (action)
    {
    case NEW_FORMULA :
      dummy = malloc(sizeof(struct formula));
      if (dummy == NULL) {
	fprintf(stderr, "Failed to allocate memory for formula.\n");
	exit(-1);
      }
      if (no_formulas[current_system] == MAX_FORMULAS) {
	ceprintf("Too many formula definitions\n");
	exit(-1);
      }

      if (!check_variable(operand, current_system))
	if (!check_global_variable(operand)) {
	  ceprintf("`%s' undefined variable\n", operand);
	  exit(-1);
	}

      formulas[current_system][no_formulas[current_system]++] = dummy;
      current_formula = dummy;
      dummy->field_type = DEF_FIELD;
      strcpy(dummy->func_field.definition.result_variable, operand);
      dummy->func_field.definition.index_type = INVALID;
      break;

    case FORMULA_END :
      dummy->next = NULL;
      dummy = NULL; /* Get ready for the next formula definition */
      break;

    case FORMULA_OP :
      dummy->next = malloc(sizeof(struct formula));
      dummy = dummy->next;
      if (dummy == NULL) {
	fprintf(stderr, "Failed to allocate memory for formula.\n");
	exit(-1);
      }
      if (strcmp(operand, "MIN") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = MIN;
      }
      if (strcmp(operand, "MAX") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = MAX;
      }
      if (strcmp(operand, "CEILING") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = CEILING;
      }
      else if (strcmp(operand, "FLOOR") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = FLOOR;
      }
      else if (strcmp(operand, "PLUS") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = PLUS;
      }
      else if (strcmp(operand, "MINUS") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = MINUS;
      }
      else if (strcmp(operand, "UMINUS") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = UMINUS;
      }
      else if (strcmp(operand, "MULTIPLY") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = MULTIPLY;
      }
      else if (strcmp(operand, "DIVIDE") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = DIVIDE;
      }
      else if (strcmp(operand, "SIGMA_HP") == 0) {
	if (priority_variable[current_system] == INVALID)
	  ceprintf("`sigma' Using prioritised summation without declaring a priority variable");
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = SIGMA_HP;
      }
      else if (strcmp(operand, "SIGMA_LP") == 0) {
	if (priority_variable[current_system] == INVALID)
	  ceprintf("`sigma' Using prioritised summation without declaring a priority variable");
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = SIGMA_LP;
      }
      else if (strcmp(operand, "SIGMA_EP") == 0) {
	if (priority_variable[current_system] == INVALID)
	  ceprintf("`sigma' Using prioritised summation without declaring a priority variable");
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = SIGMA_EP;
      }
      else if (strcmp(operand, "SIGMA_ALL") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = SIGMA_ALL;
      }
      else if (strcmp(operand, "END_SIGMA") == 0) {
	dummy->field_type = OP_FIELD;
	dummy->func_field.op_field.operation = END_SIGMA;
      }
      break;

    case FORMULA_CONST :
      dummy->next = malloc(sizeof(struct formula));
      dummy = dummy->next;
      if (dummy == NULL) {
	fprintf(stderr, "Failed to allocate memory for formula.\n");
	exit(-1);
      }
      dummy->field_type = CONST_FIELD;
      dummy->func_field.op_field.constant = strtod(operand, NULL);
      break;

    case FORMULA_VAR :
      dummy->next = malloc(sizeof(struct formula));
      dummy = dummy->next;
      if (dummy == NULL) {
	fprintf(stderr, "Failed to allocate memory for formula.\n");
	exit(-1);
      }
      dummy->field_type = VAR_FIELD;
      counter = 0; /* Look up the variable */
      if (!check_variable(operand, current_system))
	if (!check_global_variable(operand)) {
	  ceprintf("`%s' undefined variable\n", operand);
	  exit(-1);
	}

      strcpy(dummy->func_field.op_field.var_field.variable_name, operand);
      dummy->func_field.op_field.var_field.index_type = INVALID;
      break;

    case FORMULA_INDEX :
      /* This action is used directly after NEW_FORMULA, to
	 indicate the index type of the result variable. It is also
	 used to declare the index type for other variables,
	 used in a formula. If NULL is given as the indexing task
	 name (contained in operand) SCALAR is assumed to be
	 the variable type. */

      switch (dummy->field_type)
	/* Tie an index to a variable. If this is the definition field
	   (the first field of a formula), then the index concerns the
	   result variable, and can thus be only I_INDEX, SINGLE_INDEX
	   or SCALAR */
	{
	case DEF_FIELD :
	  if (check_variable(dummy->func_field.definition.result_variable, current_system))
	    variable_type = variable_types[current_system][get_variable_index(dummy->func_field.definition.result_variable, current_system)];
	  else
	    variable_type = global_variable_types[get_global_variable_index(dummy->func_field.definition.result_variable)];

	  if (operand == NULL) {
	    dummy->func_field.definition.index_type = SCALAR;

	    if (variable_type != SCALAR_VAR) {
	      ceprintf("`%s' variable used as scalar, but declared indexed\n", dummy->func_field.definition.result_variable);
	      exit(-1);
	    }
	  }
	  else {
	    if (variable_type != INDEXED_VAR) {
	      ceprintf("`%s' variable used as indexed, but declared scalar\n", dummy->func_field.definition.result_variable);
	      exit(-1);
	    }

	    if (strcmp("i", operand) == 0)
	      dummy->func_field.definition.index_type = I_INDEX;

	    else {
	      dummy->func_field.definition.index_type = SINGLE_INDEX;
	      if (check_variable(dummy->func_field.definition.result_variable, current_system)) {
		if (!check_task(operand, current_system)) {
		  ceprintf("`%s' task not previously declared\n", operand);
		  exit(-1);
		}
	      }
	      else if (check_global_variable(dummy->func_field.definition.result_variable)) {
		if (!check_global_task(operand)) {
		  ceprintf("`%s' task not previously declared\n", operand);
		  exit(-1);
		}
	      }
	      strcpy(dummy->func_field.definition.result_index_task, operand);
	    }
	  }
	  break;

	case VAR_FIELD :
	  if (check_variable(dummy->func_field.op_field.var_field.variable_name, current_system)) {
	    counter = get_variable_index(dummy->func_field.op_field.var_field.variable_name, current_system);
	    variable_type = variable_types[current_system][counter];
	    strcpy(variable_name, variable_names[current_system][counter]);
	  }
	  else {
	    counter = get_global_variable_index(dummy->func_field.op_field.var_field.variable_name);
	    variable_type = global_variable_types[counter];
	    strcpy(variable_name, global_variable_names[counter]);
	  }

	  if (operand == NULL) {
	    if (variable_type != SCALAR_VAR) {
	      ceprintf("`%s' variable used as scalar, but declared indexed\n", variable_name);
	      exit(-1);
	    }
	    dummy->func_field.op_field.var_field.index_type = SCALAR;
	  }
	  else {
	    if (variable_type != INDEXED_VAR) {
	      ceprintf("`%s' variable used as indexed, but declared scalar\n", variable_name);
	      exit(-1);
	    }

	    if (strcmp("i", operand) == 0) {
	      dummy->func_field.op_field.var_field.index_type = I_INDEX;
	      if ((check_global_variable(current_formula->func_field.definition.result_variable) && check_variable(variable_name, current_system)) ||
		  (check_global_variable(variable_name) && check_variable(current_formula->func_field.definition.result_variable, current_system))) {

		/* This huge` if' checks if the result variable and the other variable
		   used in the formula have a different set of indexing tasks. It should
		   then check if these indexing tasks are the same. */

		if (no_tasks[current_system] != no_global_tasks) {
		    ceprintf("`%s' and `%s': Variables have different dimensions.\nMake sure the number of indexing tasks match.", current_formula->func_field.definition.result_variable, variable_name);
		    exit(-1);
		}
		else for (counter = 0; counter < no_tasks[current_system]; counter++)
		  if (!check_global_task(task_names[current_system][counter])) {
		    ceprintf("`%s' and `%s': Conflicting variables.\nTask index name `%s' not found in both system `%s' and global declaration.\nMake sure the names of indexing tasks match.",current_formula->func_field.definition.result_variable, variable_name, task_names[current_system][counter], system_names[current_system]);
		    exit(-1);
		  }
	      }
	    }

	    else if (strcmp("j", operand) == 0) {
	      dummy->func_field.op_field.var_field.index_type = J_INDEX;
	      if ((check_global_variable(current_formula->func_field.definition.result_variable) && check_variable(variable_name, current_system)) ||
		  (check_global_variable(variable_name) && check_variable(current_formula->func_field.definition.result_variable, current_system))) {

		/* This huge` if' checks if the result variable and the other variable
		   used in the formula have a different set of indexing tasks. It should
		   then check if these indexing tasks are the same. */

		if (no_tasks[current_system] != no_global_tasks) {
		    ceprintf("`%s' and `%s': Variables have different dimensions.\nMake sure the number of indexing tasks match.", current_formula->func_field.definition.result_variable, variable_name);
		    exit(-1);
		}
		else for (counter = 0; counter < no_tasks[current_system]; counter++)
		  if (!check_global_task(task_names[current_system][counter])) {
		    ceprintf("`%s' and `%s': Conflicting variables.\nTask index name `%s' not found in both system `%s' and global declaration.\nMake sure the names of indexing tasks match.",current_formula->func_field.definition.result_variable, variable_name, task_names[current_system][counter], system_names[current_system]);
		    exit(-1);
		  }
	      }
	    }

	    else {
	      dummy->func_field.op_field.var_field.index_type = SINGLE_INDEX;
	      if (check_variable(dummy->func_field.op_field.var_field.variable_name, current_system)) {
		if (!check_task(operand, current_system)) {
		  ceprintf("`%s' task not previously declared\n", operand);
		  exit(-1);
		}
	      }
	      else if (check_global_variable(dummy->func_field.op_field.var_field.variable_name)) {
		if (!check_global_task(operand)) {
		  ceprintf("`%s' task not previously declared\n", operand);
		  exit(-1);
		}
	      }
	      strcpy(dummy->func_field.op_field.var_field.variable_index_task, operand);
	    }
	  }
	  break;
	}
    }
}

void output_variables(int no_systems)
{
  int current_var, current_index, current_semaphore, current_system;

  printf("Number of systems: %d\n\n", no_systems);
  for (current_system = 0;current_system < no_systems;current_system++) {
    printf("System `%s'\n", system_names[current_system]);
    printf("------------------\n");
    for (current_var = 0;current_var < no_vars[current_system];current_var++) {
      printf("\nVariable `%s'\n", variable_names[current_system][current_var]);
      if (variable_types[current_system][current_var] == INDEXED_VAR)
	for (current_index = 0;current_index < no_tasks[current_system];current_index++)
	  printf("%s[%s] = %f\n", variable_names[current_system][current_var], task_names[current_system][current_index], variables[current_system][current_var][current_index]);
      else
	printf("%s = %f\n", variable_names[current_system][current_var], variables[current_system][current_var][0]);
    }
    if (blocking[current_system]) {
      printf("\nSemaphores:\n\nName\tLocked by\tTime held\t\tceiling\n");
      for (current_semaphore = 0;current_semaphore < MAX_SEMAPHORES;current_semaphore++)
	for (current_index = 0;current_index < no_tasks[current_system];current_index++)
	  if (semaphores[current_system][current_semaphore][current_index] != INVALID)
	    printf("%s\t%s\t\t%f\t\t%f\n", semaphore_names[current_system][current_semaphore], task_names[current_system][current_index], semaphores[current_system][current_semaphore][current_index], ceiling[current_system][current_semaphore]);
    }
    printf("\n\n");
  }
}

/* CALCULATE_BLOCKING

   If blocking factores were defined, calculate them before starting the
   iterations
*/

void calculate_blocking(int current_system)
{
  int current_task, current_semaphore, blocking_task;
  double min_value, max_block;

  /* First, calculate the ceilings of all semaphores (assume that there
     is really a MAX_SEMAPHORES number of semaphores... */

  for(current_semaphore = 0;current_semaphore < MAX_SEMAPHORES;current_semaphore++) {
    min_value = MAXDOUBLE;
    for (current_task = 0;current_task < no_tasks[current_system];current_task++)
      if (semaphores[current_system][current_semaphore][current_task] != INVALID)
	if (variables[current_system][priority_variable[current_system]][current_task] < min_value)
	  min_value = variables[current_system][priority_variable[current_system]][current_task];
    ceiling[current_system][current_semaphore] = min_value;
  }

  /* Then, calculate the blocking factor */

  for (current_task = 0;current_task < no_tasks[current_system];current_task++) {
    max_block = 0;
    for (current_semaphore = 0;current_semaphore < MAX_SEMAPHORES;current_semaphore++)
      for (blocking_task = 0;blocking_task < no_tasks[current_system];blocking_task++)
	if (current_task != blocking_task)
	  if ((variables[current_system][priority_variable[current_system]][blocking_task] > variables[current_system][priority_variable[current_system]][current_task]) && (ceiling[current_system][current_semaphore] <= variables[current_system][priority_variable[current_system]][current_task]))
	    if (semaphores[current_system][current_semaphore][blocking_task] > max_block)
	      max_block = semaphores[current_system][current_semaphore][blocking_task];
    variables[current_system][blocking_variable[current_system]][current_task] = max_block;
  }
}

/*
   Check for the presence of the priority variable as a result value of one of the
   formulas of the system. If present, and if blocking factors are part of the system,
   the blocking factors should be calculated after every iteration because of the
   dynamic priorities.
*/

int check_dynamic_blocking(int current_system)
{
  int current_formula,
  dynamic_blocking;

  dynamic_blocking = FALSE;

  for (current_formula = 0;current_formula < no_formulas[current_system];current_formula++)
    /* First, make sure the result variable is not a global, which makes it
       uneligible for a priority variable. */
    if (check_variable(formulas[current_system][current_formula]->func_field.definition.result_variable, current_system))
      if (get_variable_index(formulas[current_system][current_formula]->func_field.definition.result_variable, current_system) == priority_variable[current_system])
	dynamic_blocking = TRUE;

  return(dynamic_blocking);
}

/* priority_refresh checks if the priorities of a system were different
   than the original priorities, after the system calculation has
   converged. If it has, the system is reset, but the new priority
   values are kept. Then, the system is run again until the next
   conversion. When stable priorities have been found, the system
   can be considered completely converged... This is a tricky little
   algorithm, that's why I wrote this comment... */

int priority_refresh(int current_system)
{
  int var_counter,
  formula_counter,
  counter,
  changed;

  changed = FALSE;

  for (counter = 0; counter < no_tasks[current_system]; counter++)
    if (variables[current_system][priority_variable[current_system]][counter] !=
	backup_vars[current_system][priority_variable[current_system]][counter])
      changed = TRUE;
  if (changed) {
    for (var_counter = 0; var_counter < no_vars[current_system]; var_counter++)
      if (var_counter == priority_variable[current_system])
	for (counter = 0; counter < no_tasks[current_system]; counter++)
	  backup_vars[current_system][priority_variable[current_system]][counter] = variables[current_system][priority_variable[current_system]][counter];
      else
	for (counter = 0; counter < no_tasks[current_system]; counter++)
	  variables[current_system][var_counter][counter] = backup_vars[current_system][var_counter][counter];

    /* Now check if a global variable is the result of one of the system
       formulae. In such a case, this global variable has to be backed up
       from the original values as well. */

    for (formula_counter = 0; formula_counter < no_formulas[current_system]; formula_counter++) {

      if (check_global_variable(formulas[current_system][formula_counter]->func_field.definition.result_variable)) {
	var_counter = get_global_variable_index(formulas[current_system][formula_counter]->func_field.definition.result_variable);
	for (counter = 0; counter < no_global_tasks; counter++)
	  global_variables[var_counter][counter] = global_backup_vars[var_counter][counter];
      }
    }
  }
  return(changed);
}

int get_variable_index(char *var, int current_system)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_vars[current_system]) && !found)
    if (strcmp(var, variable_names[current_system][counter]) == 0)
      found = TRUE;
    else
      counter++;

  if (found)
    return(counter);
  else { /* The parser should cover this error! */
    fprintf(stderr, "Bad error: Variable %s not found in system %s.\n", var, system_names[current_system]);
    exit(-1);
  }
}

int get_global_variable_index(char *var)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_global_variables) && !found)
    if (strcmp(var, global_variable_names[counter]) == 0)
      found = TRUE;
    else
      counter++;

  if (found)
    return(counter);
  else { /* The parser should cover this error! */
    fprintf(stderr, "Bad error: Global variable %s not found.\n", var);
    exit(-1);
  }
}

int get_task_index(char *task, int current_system)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_tasks[current_system]) && !found)
    if (strcmp(task, task_names[current_system][counter]) == 0)
      found = TRUE;
    else
      counter++;

  if (found)
    return(counter);
  else { /* The parser should cover this error! */
    fprintf(stderr, "Bad error: Task %s not found in system %s.\n", task, system_names[current_system]);
    exit(-1);
  }
}

int get_global_task_index(char *task)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_global_tasks) && !found)
    if (strcmp(task, global_task_names[counter]) == 0)
      found = TRUE;
    else
      counter++;

  if (found)
    return(counter);
  else { /* The parser should cover this error! */
    fprintf(stderr, "Bad error: Task not found in global declarations.\n");
    exit(-1);
  }
}

int check_variable(char *var, int current_system)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_vars[current_system]) && !found)
    if (strcmp(var, variable_names[current_system][counter]) == 0)
      found = TRUE;
    else
      counter++;
  return(found);
}

int check_global_variable(char *var)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_global_variables) && !found)
    if (strcmp(var, global_variable_names[counter]) == 0)
      found = TRUE;
    else
      counter++;
  return(found);
}

int check_task(char *task, int current_system)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_tasks[current_system]) && !found)
    if (strcmp(task, task_names[current_system][counter]) == 0)
      found = TRUE;
    else
      counter++;
  return(found);
}

int check_global_task(char *task)
{
  int counter, found;

  found = FALSE;
  counter = 0;

  while ((counter < no_global_tasks) && !found)
    if (strcmp(task, global_task_names[counter]) == 0)
      found = TRUE;
    else
      counter++;
  return(found);
}

double max(double x,
	   double y)
{
  if (x>y)
    return(x);
  else
    return(y);
}

double min(double x,
	   double y)
{
  if (x>y)
    return(y);
  else
    return(x);
}

void push(double data,
	  int current_system)
{
  if (stack_pointer[current_system] >= STACK_SIZE - 1) {
    fprintf(stderr, "Stack overflow in system no. %d!\n", current_system + 1);
    exit(-1);
  }
  else
    stack[current_system][++stack_pointer[current_system]]=data;
}

double pop(int current_system)
{
  double data;

  if (stack_pointer[current_system] == 0) {
    fprintf(stderr, "Stack underflow in system no. %d!\n", current_system + 1);
    exit(-1);
  }
  else
    data = stack[current_system][stack_pointer[current_system]--];

  return(data);
}
