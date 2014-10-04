#ifndef __FPSMAIN
#define __FPSMAIN

/* typedef enum { UNDEFINED, OK };
typedef enum { FORM_CREATE };
*/

typedef enum { INDEXED_VAR, SCALAR_VAR, BLOCKING_VAR, PRIORITY_VAR } var_t;
typedef enum { NEW_FORMULA, FORMULA_END, FORMULA_INDEX, FORMULA_OP, FORMULA_CONST, FORMULA_VAR } form_dec_t;
typedef enum { GET_SYSTEM, NEXT_SYSTEM } sys_req_t;

/* Prototypes */

void declare_system(char *);
void declare_variable(char *, int);
void add_semaphore(char *, char *, double);
void declare_task(char *);
void declare_global_task(char *);
void declare_formula(char *, int);
void declare_global_variable(char *, int);
int system_registry(int);
void init_variable(char *, char *, double);

#endif
