#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_EXPR   1024
#define T_BEG_LINE    1
#define T_CHAR        2
#define T_EOL         3
#define T_RANGE       4
#define T_DOT         5

struct s_state {
 int type[MAX_EXPR];
 int current;
 int count;
 char value[MAX_EXPR];
 bool optional[MAX_EXPR];

 struct {
  int glb[MAX_EXPR];
  int lub[MAX_EXPR];
 } range;

};

int usage() {
 printf("usage: jrep [pattern] [file]\n");
 return 0;
}

int parse(char *expr, struct s_state *state) {
 state->current = 0;
 state->count = 0;

 /* Parse the regular expression to create a sequence of states */
 for (int i = 0; expr[i] != '\0'; i++) {
  state->optional[state->current] = false;
  switch (expr[i]) {
   case '^': {
    if (i != 0) {
     fprintf(stderr, "Error: Leading characters before ^\n");
     return -1;
    }
    state->type[0] = T_BEG_LINE;
    ++state->count;
    ++state->current;
    continue;
   }

   case '$': {
    if (expr[i+1] != '\0') {
     fprintf(stderr, "Error: Trailing characters after $\n");
     return -1;
    }
    state->type[state->current] = T_EOL;
    ++state->count;
    ++state->current;
    continue;
   }

   case '?': {
    if (state->current == 0) {
     fprintf(stderr, "Error: Missing state before ?\n");
     return -1;
    }
    state->optional[state->current-1] = true;
    continue;
   }

   case '[': {
    state->type[state->current] = T_RANGE;

    /* Obtain greatest lower bound of range */
    ++i;
    if (expr[i] == '\0' || expr[i] == ']') {
     fprintf(stderr, "Error: Malformed range\n");
     return -1;
    }
    state->range.glb[state->current] = expr[i];

    /* Look for - separating range */
    ++i;
    if (expr[i] != '-') {
     fprintf(stderr, "Error: Range missing separator\n");
     return -1;
    }

    /* Obtain least upper bound of range */
    if (expr[i] != '\0') ++i; else {
     fprintf(stderr, "Error: Unterminated range\n");
     return -1;
    }
    if (expr[i] == '\0' || expr[i] == ']') fprintf(stderr, "Error: Malformed range\n");
    state->range.lub[state->current] = expr[i];

    if (expr[++i] != ']') {
     fprintf(stderr, "Error: Expected ], got '%c' (%d)\n", expr[i], (int)expr[i]);
     return -1;
    }

    if (state->range.lub[state->current] < state->range.glb[state->current]) {
     fprintf(stderr, "Error: Least upper bound of range is less than its greatest lower bound\n");
     return -1;
    }

    ++state->current;
    ++state->count;
    continue;
   }

   case ']': {
    fprintf(stderr, "Error: Missing opening [\n");
    return -1;
   }

   case '.': {
    state->type[state->current] = T_DOT;
    ++state->current;
    ++state->count;
    continue;
   }
  }

  state->type[state->current] = T_CHAR;
  state->value[state->current] = expr[i];

  ++state->count;
  ++state->current;
 }
 state->current = 0;
 return 0;
}

bool does_match(char *line, struct s_state *state) {
 state->current = 0;
 bool matches = false;
 int n = 0;

 do {
  switch (state->type[state->current]) {
   case T_BEG_LINE: {
    if (n != 0) {
     return false;
    }
    matches = true;
    ++state->current;
    continue;
   }

   case T_EOL: {
    return ((line[n] == '\n' && matches) || state->current == 0);
   }

   case T_RANGE: {
    if (line[n] >= state->range.glb[state->current] &&
        line[n] <= state->range.lub[state->current]) {

     ++n;
     ++state->current;
     matches = true;
     continue;
    }
    if (state->optional[state->current]) {
     ++state->current;
     matches = true;
    } else {
     state->current = 0;
     matches = false;
     ++n;
    }
    continue;
   }

   case T_DOT: {
    if (line[n] != '\n') {
     matches = true;
     ++state->current;
     ++n;
     continue;
    }
    if (state->optional[state->current]) {
     matches = true;
     --n;
     continue;
    }
    matches = false;
    ++n;
    continue;
   }

   case T_CHAR: {
    if (line[n] == state->value[state->current]) {
     ++state->current;
     ++n;
     matches = true;
     continue;
    } 
    if (state->optional[state->current]) {
     matches = true;
     ++state->current;
     if (line[n+1] == '\n') ++n;
     continue;
    } else {
     matches = false;
     state->current = 0;
     ++n;
    }
    continue;
   }
  }
 } while (line[n] != '\0' && state->current < state->count);

 return matches && (state->current == state->count);
}

int main(int argc, char *argv[]) {
 FILE *fp;

 if (argc < 2) return usage();

 if (argc > 2) {
  fp = fopen(argv[2], "r");
  if (fp == NULL) {
   fprintf(stderr, "Error opening file: %s\n", argv[2]);
   return 1;
  }
 } else fp = stdin;

 size_t def_bytes = 1024;
 char *line = (char *)malloc(def_bytes);
 int bytes_read = getline(&line, &def_bytes, fp);
 int lc = 0;
 bool matches = false;
 struct s_state state;
 if (parse(argv[1], &state) < 0) {
  return 2;
 }

 while (bytes_read > 0) {
  ++lc;
  int result = does_match(line, &state);
  if (result == true) {
   matches = true;
   printf("%4d: %s", lc, line);
  }
  bytes_read = getline(&line, &def_bytes, fp);
 }

 free(line);

 if (matches) return 0;
 else return 1;
}
