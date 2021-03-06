#include "jrep.h"

int usage() {
 printf("usage: jrep [-cHnov] [pattern] [file]\n");
 return 0;
}

int version() {
 printf("jrep (Just a Regular Expression Parser 1.0.0\n");
 return 0;
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
 bool count = false;
 bool print_names = false;
 int c = 0;

 if (argv[1][0] == '-') {
  ++pattern;
  for (int i = 1; argv[1][i] != '\0'; ++i) {
   switch (argv[1][i]) {
    case 'c': {
     count = true;
     break;
    }
    case 'H': {
     print_names = true;
     break;
    }
    case 'v': {
     inverse = true;
     break;
    }
    case 'V': {
     return version();
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
 char *fn;

 if (argc > pattern + 1) {
  fn = malloc(strlen(argv[pattern + 1]) + 1);
  fn = argv[pattern + 1];
  fp = fopen(argv[pattern + 1], "r");
  if (fp == NULL) {
   fprintf(stderr, "Error opening file: %s\n", argv[2]);
   return 1;
  }
 } else {
  fp = stdin;
  fn = malloc(18);
  strcpy(fn, "(standard input)");
 }

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

 /* Count the number of or statements in pattern */
 int orcount = 0;
 for (int j = 0; argv[pattern][j] != '\0'; ++j)
  if (argv[pattern][j] == '|') ++orcount;

 /* Allocate memory for array of expressions */
 char *regex[orcount];

 /* This struct stores every state in the graph */
 struct s_state *state = malloc((orcount + 1) * sizeof *state);

 /* Separate pattern by OR tokens */
 int patlen = 0;
 int patc = 0;
 regex[0] = malloc(strlen(argv[pattern] + 1));
 for (int j = 0; argv[pattern][j] != '\0'; ++j) {
  if (argv[pattern][j] != '|') {
   regex[patc][patlen] = argv[pattern][j];
   ++patlen;
  } else {
   regex[patc][patlen] = '\0';
   patlen = 0;
   ++patc;
   regex[patc] = malloc(strlen(argv[pattern] - j + 1));
  }
 }
 regex[patc][patlen] = '\0';

 /* Parse the expression or throw an error */
 for (int j = 0; j < orcount + 1; ++j) {
  if (parse(regex[j], &state[j]) < 0) {
   return 2;
  }
 }

 /* Check for matches in each line of the file */
 while (bytes_read > 0) {
  ++lc;
  matches = false;
  for (int j = 0; j < orcount+1; ++j) {
   matches |= inverse ^ does_match(line, &state[j], only, &buffer);
   if (!only && matches) break;
  }
  if (matches) {
   ++c;
   if (!count) {
    if (print_names) printf("%s:", fn);
    if (linecount) printf("%d:", lc);
    if (only && !inverse) printf("%s", buffer);
    else printf("%s", line);
   }
  }
  bytes_read = getline(&line, &def_bytes, fp);
 }

 if (count) printf("%d\n", c);

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
