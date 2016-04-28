#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Specify GNU source for readline() function */
#define _GNU_SOURCE

/* Maximum number of states in a graph */
#define MAX_EXPR   1024

/* Define all of the possible states */
#define T_BEG_LINE    1
#define T_CHAR        2
#define T_EOL         3
#define T_RANGE       4
#define T_DOT         5

/* An s_state struct stores a graph of states */
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

/* Parses the regular expression and creates a graph of states in struct */
int parse(char *expr, struct s_state *state) {
 state->current = 0;
 state->count = 0;

 /* Parse the regular expression to create a sequence of states */
 for (int i = 0; expr[i] != '\0'; i++) {
  state->optional[state->current] = false;
  switch (expr[i]) {
   /* Match the beginning of a line or complain if the pattern wants the
      beginning of the line to come after another character */
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

   /* Match the end of line or complain if the patter doesn't end at $ */
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

   /* Makes the previous state optional*/
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

    /* Obtain least upper bound of range or complain if it's not there */
    if (expr[i] != '\0') ++i; else {
     fprintf(stderr, "Error: Unterminated range\n");
     return -1;
    }
    if (expr[i] == '\0' || expr[i] == ']') fprintf(stderr, "Error: Malformed range\n");
    state->range.lub[state->current] = expr[i];

    /* Brackets should always match */
    if (expr[++i] != ']') {
     fprintf(stderr, "Error: Expected ], got '%c' (%d)\n", expr[i], (int)expr[i]);
     return -1;
    }

    /* If lub < glb, the range is invalid */
    if (state->range.lub[state->current] < state->range.glb[state->current]) {
     fprintf(stderr, "Error: Least upper bound of range is less than its greatest lower bound\n");
     return -1;
    }

    ++state->current;
    ++state->count;
    continue;
   }

   /* The [ case reads the matching ], so we shouldn't see another one
      unless it's escaped. */
   case ']': {
    fprintf(stderr, "Error: Missing opening [\n");
    return -1;
   }

   /* The dot is a catch all; it matches any character. */
   case '.': {
    state->type[state->current] = T_DOT;
    ++state->current;
    ++state->count;
    continue;
   }

   /* If we find a \, the next character is stored as a T_CHAR state. In
      other words, the following character is escaped. */
   case '\\': {
    if (expr[++i] == '\0') {
     fprintf(stderr, "Error: Trailing \\\n");
     return -1;
    }
    state->type[state->current] = T_CHAR;
    state->value[state->current] = expr[i];
    ++state->count;
    ++state->current;
    continue;
   }
  }

  /* If the switch statement didn't find a special character, we take the
     current character literally as a T_CHAR state. */
  state->type[state->current] = T_CHAR;
  state->value[state->current] = expr[i];
  ++state->count;
  ++state->current;
 }
 return 0;
}

/* Returns whether the line matches the pattern by traversing the graph
   stored in the state struct */
bool does_match(char *line, struct s_state *state) {
 state->current = 0;
 bool matches = false;
 int n = 0;

 /* We use a do-while so we can catch blank lines. Otherwise, the loop
    would break before we're able to parse the blank line. */
 do {
  switch (state->type[state->current]) {
   /* ^ -- Matches begining of line */
   case T_BEG_LINE: {
    if (n != 0) {
     return false;
    }
    matches = true;
    ++state->current;
    continue;
   }

   /* $ -- Matches the end of line */
   case T_EOL: {
    return ((line[n] == '\n' && matches) || state->current == 0);
   }

   /* [a-z] -- Matches any range of characters */
   case T_RANGE: {
    /* Check whether current character is inside the range */
    if (line[n] >= state->range.glb[state->current] &&
        line[n] <= state->range.lub[state->current]) {

     ++n;
     ++state->current;
     matches = true;
     continue;
    }
    /* Character was not inside the range. Was the state optional? */
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

   /* . -- Matches any character */
   case T_DOT: {
    /* If we're at the end of the line, don't match; \n doesn't count as a
       character in the line. */
    if (line[n] != '\n') {
     matches = true;
     ++state->current;
     ++n;
     continue;
    }
    /* End of line reached. If the state was optional, that's ok! */
    if (state->optional[state->current]) {
     matches = true;
     --n;
     continue;
    }
    matches = false;
    ++n;
    continue;
   }

   /* Match the character specified by state->value[]. This could be any
      character because the escapes have already been parsed. */
   case T_CHAR: {
    if (line[n] == state->value[state->current]) {
     ++state->current;
     ++n;
     matches = true;
     continue;
    } 
    /* Character didn't match, but that's ok if it was optional. */
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

 /* It's not enough for the current state to be matched; we had to have
    also reached the end of the graph. */
 return matches && (state->current == state->count);
}

int main(int argc, char *argv[]) {
 /* If argc == 1, no regular expression was supplied, so we display the
    usage screen. */
 if (argc < 2) return usage();

 FILE *fp;

 if (argc > 2) {
  fp = fopen(argv[2], "r");
  if (fp == NULL) {
   fprintf(stderr, "Error opening file: %s\n", argv[2]);
   return 1;
  }
 } else fp = stdin;

 /* Default string size; this number doesn't really matter */
 size_t def_bytes = 1024;

 /* String storing the current line of the file */
 char *line = (char *)malloc(def_bytes);

 /* Number of bytes read by getline() */
 int bytes_read = getline(&line, &def_bytes, fp);

 /* Line counter */
 int lc = 0;
 
 /* Whether the file matches the pattern at any point */
 bool matches = false;

 /* This struct stores every state in the graph */
 struct s_state state;

 /* Parse the expression or throw an error */
 if (parse(argv[1], &state) < 0) {
  return 2;
 }

 /* Check for matches in each line of the file */
 while (bytes_read > 0) {
  ++lc;
  if (does_match(line, &state)) {
   matches = true;
   printf("%4d: %s", lc, line);
  }
  bytes_read = getline(&line, &def_bytes, fp);
 }

 /* The OS should be doing the cleanup for us, but we'll go ahead and close
    the file and free the string's memory anyway. */
 free(line);
 fclose(fp);

 /* Return 1 if no match was found so we can use this as a test in bash
    scripts. */
 if (matches) return 0;
 else return 1;
}
