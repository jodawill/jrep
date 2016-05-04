#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>

/* Specify GNU source for readline() function */
#define _GNU_SOURCE

/* Maximum number of states in a graph */
#define MAX_EXPR   1024

/* Define all of the possible states */
#define T_CHAR        1
#define T_EOL         2
#define T_RANGE       3
#define T_DOT         4

/* An s_state struct stores a graph of states */
struct s_state {
 int type[MAX_EXPR];
 int count;
 char value[MAX_EXPR];
 bool beg;
 bool optional[MAX_EXPR];
 bool kleene[MAX_EXPR];

 struct {
  int glb[MAX_EXPR];
  int lub[MAX_EXPR];
 } range;
};

int parse(char *expr, struct s_state *state);
int does_char_match(char c, struct s_state *state, int i);
int match_start(char *line, struct s_state *state, bool print, char **buffer);
bool does_match(char *line, struct s_state *state, bool print, char **buffer);
