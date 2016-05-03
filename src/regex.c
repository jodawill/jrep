#include "jrep.h"

int copy_state(struct s_state *state, int i) {
 state->type[i+1] = state->type[i];
 state->optional[i+1] = state->optional[i];
 state->kleene[i+1] = state->kleene[i];
 state->value[i+1] = state->value[i];
 state->range.lub[i+1] = state->range.lub[i];
 state->range.glb[i+1] = state->range.glb[i];
 ++state->count;
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

   case '+': {
    if (state->current == 0) {
     fprintf(stderr, "Error: Missing state before +\n");
     return -1;
    }
    copy_state(state, state->current-1);
    state->optional[state->current-1] = false;
    state->kleene[state->current-1] = false;
    state->optional[state->current] = true;
    state->kleene[state->current++] = true;
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
   return (c >= state->range.glb[i] && c <= state->range.lub[i]);
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
   if (!state->kleene[i] ||
     (state->kleene[i] && state->count > i+1 && does_char_match(line[n+1], state, i+1))) {
    ++i;
   }
   ++n;
  } else if (state->optional[i]) {
   ++i;
  } else {
   return jump;
  }
 } while (line[n] != '\0' && i < state->count);
 if (i == state->count) {
  if (print) {
   free(*buffer);
   *buffer = (char *)malloc(n+1);
   for (int j = 0; j < n; ++j) {
    (*buffer)[j] = line[j];
   }
   if ((*buffer)[n-1] != '\n') (*buffer)[n] = '\n';
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
