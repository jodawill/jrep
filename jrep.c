#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
 int current;
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

int usage() {
 printf("usage: [-nov] jrep [pattern] [file]\n");
 return 0;
}

/* Parses the regular expression and creates a graph of states in struct */
int parse(char *expr, struct s_state *state) {
 state->current = 0;
 state->count = 0;

 /* Parse the regular expression to create a sequence of states */
 for (int i = 0; expr[i] != '\0'; i++) {
  state->optional[state->current] = false;
  state->kleene[state->current] = false;
  switch (expr[i]) {
   /* Match the beginning of a line or complain if the pattern wants the
      beginning of the line to come after another character */
   case '^': {
    if (i != 0) {
     fprintf(stderr, "Error: Leading characters before ^\n");
     return -1;
    }
    state->beg = true;
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

   case '*': {
    if (state->current == 0) {
     fprintf(stderr, "Error: Missing state before *\n");
     return -1;
    }
    state->optional[state->current-1] = true;
    state->kleene[state->current-1] = true;
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

/* Returns whether a character c matches a particular state i */
int does_char_match(char c, struct s_state *state, int i) {
 switch (state->type[i]) {
  case T_CHAR: {
   return (state->value[i] == c);
  }

  case T_EOL: {
   return (c == '\n');
  }

  case T_RANGE: {
   return ((c >= state->range.glb[i] && c <= state->range.lub[i]));
  }

  case T_DOT: {
   return (c != '\n');
  }
 }
 return -1;
}

/* Return 0 if an entire match is found from line[0]. Otherwise, return the
   number of characters to jump forward. If no match is found, we jump
   forward by one character. If a partial match is found, we return the
   position of the first character matching state 0 *after* line[0]. */
int match_start(char *line, struct s_state *state, bool print, char **buffer) {
 int i = 0;
 int n = 0;
 int jump = 1;
 int b = false;
 do {
  if (jump == 1 && i > 0 && !b && does_char_match(line[n], state, 0)) {
   b = true;
   jump = n;
  }
  bool match = does_char_match(line[n], state, i);

  if (match) {
   if (!state->kleene[i]) ++i;
   ++n;
  } else if (state->optional[i]) {
   ++i;
  } else {
   return jump;
  }
 } while (line[n] != '\0' && i < state->count);
 if (i == state->count) {
  if (print) {
   *buffer = (char *)malloc(n+1);
   for (int j = 0; j < n; ++j) {
    (*buffer)[j] = line[j];
   }
   (*buffer)[n] = '\n';
   (*buffer)[n+1] = '\0';
  }
  return 0;
 } else return jump;
}

/* Returns whether the line matches the pattern by traversing the graph
   stored in the state struct */
bool does_match(char *line, struct s_state *state, bool print, char **buffer) {
 int n = 0;
 int jump;
 do {
  jump = match_start(&line[n], state, print, buffer);
  if (jump == 0) {
   return true;
  } else {
   if (state->beg) return false;
   n += jump;
  }
 } while (line[n] != '\0');
 return false;
}

int main(int argc, char *argv[]) {
 /* If argc == 1, no regular expression was supplied, so we display the
    usage screen. */
 if (argc < 2) return usage();

 /* Which argv the pattern is stored in */
 int pattern = 1;

 bool inverse = false;
 bool linecount = false;
 bool only = false;

 if (argv[1][0] == '-') {
  ++pattern;
  for (int i = 1; argv[1][i] != '\0'; ++i) {
   switch (argv[1][i]) {
    case 'v': {
     inverse = true;
     break;
    }
    case 'n': {
     linecount = true;
     break;
    }
    case 'o': {
     only = true;
     break;
    }
    default: {
     fprintf(stderr, "Error: Unknown option: %c\n", argv[1][i]);
     return 1;
    }
   }
  }
 }

 FILE *fp;

 if (argc > pattern + 1) {
  fp = fopen(argv[pattern + 1], "r");
  if (fp == NULL) {
   fprintf(stderr, "Error opening file: %s\n", argv[2]);
   return 1;
  }
 } else fp = stdin;

 /* Default string size; this number doesn't really matter */
 size_t def_bytes = 1024;

 /* String storing the current line of the file */
 char *line = (char *)malloc(def_bytes);

 /* Stores temporary output */
 char *buffer = (char *)malloc(def_bytes);

 /* Number of bytes read by getline() */
 int bytes_read = getline(&line, &def_bytes, fp);

 /* Line counter */
 int lc = 0;
 
 /* Whether the file matches the pattern at any point */
 bool matches = false;

 /* This struct stores every state in the graph */
 struct s_state state;

 /* Parse the expression or throw an error */
 if (parse(argv[pattern], &state) < 0) {
  return 2;
 }

 /* Check for matches in each line of the file */
 while (bytes_read > 0) {
  ++lc;
  if (inverse ^ does_match(line, &state, only, &buffer)) {
   matches = true;
   if (linecount) printf("%d:", lc);
   if (!only) printf("%s", line);
   else printf("%s", buffer);
  }
  bytes_read = getline(&line, &def_bytes, fp);
 }

 /* The OS should be doing the cleanup for us, but we'll go ahead and close
    the file and free the string's memory anyway. */
 free(line);
 free(buffer);
 fclose(fp);

 /* Return 1 if no match was found so we can use this as a test in bash
    scripts. */
 if (matches) return 0;
 else return 1;
}
