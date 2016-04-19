#include <stdio.h>
#include <stdbool.h>
#include <strings.h>

bool isend(char c) {
 bool ret = (c == '\0' || c == EOF);
 if (ret) printf("\n");
 return ret;
}

bool isspecial(char c) {
 return (c == '^' || c == '$' || c == '.' || c == '*' || c == '\n');
}

int main(int argc, char *argv[]) {
 if (argc < 2) {
  printf("jrep requires a regular expression\n");
  return 1;
 }

 /* Initialize variables */
 char *regex = argv[1];
 char c, seekfor, lastchar;
 int i = 0;
 bool matched = false;
 bool findnext = true;
 bool matchstarted = false;
 seekfor = regex[0];
 lastchar = '\n';

 /* Loop through every character in input to match it against regex */
 for (c = getchar(); c != EOF && c != '\0'; c = getchar()) {
  /* Quit if end of regex is found */
  if (isend(seekfor)) return 0;

  /* We have not found a match yet. We'll set this to true if this iteration
     of the loop finds something. */
  matched = false;

  if (c == '\n' && seekfor == '.') {
   if (regex[i+1] == '*') {
    if (isend(regex[i+2])) return 0;
    else {
     i += 2;
     seekfor = regex[i];
    }
   } else {
    printf("Pattern not matched\n");
    return 1;
   }
  }

  if (!matched && seekfor == '^') {
   if (lastchar == '\n') {
    matchstarted = true;
    printf("^");

    /* Don't mark as matched because ^ matches the last character, not
       the current. Instead, check to see if the expression has ended.
       If not, move onto the expressions matching the current character. */
    if (isend(findnext)) return 0;
    seekfor = regex[++i];
    if (isend(seekfor)) return 0;
   }
  }

  if (!matched && seekfor == '$') {
   if ((matchstarted && c == '\n') || (c == '\n' && lastchar == '\n')) {
    printf("$");
    seekfor = regex[++i];
    if (isend('\0')) return 0;
   } else {
    printf("\n$ not matched\n");
    i = 0;
    seekfor = regex[i];
    matchstarted = false;
    matched = false;
    ungetc(c, stdin);
    continue;
   }
  }

  if (!matched && !isspecial(seekfor)) {
   if (c == seekfor) {
    printf("%c", c);
    seekfor = regex[++i];
    if (isend(seekfor)) return 0;
    matched = true;
   } else if (matchstarted) {
    printf(" # aborting partial match\n");
    i = 0;
    seekfor = regex[i];
    matched = false;
    matchstarted = false;
    ungetc(c, stdin);
    continue;
   }
  }

  if (!matched && seekfor == '.') {
   matched = true;
   if (regex[i+1] == '*' && regex[i+2] == c) {
    printf("%c", c);
    i += 3;
    seekfor = regex[i];
   } else {
    printf("%c", c);
    if (regex[i+1] != '*') seekfor = regex[++i];
   }
  }

  if (!matched) {
   if (matchstarted) {
    if (regex[i+1] == '*') {
     i += 2;
     seekfor = regex[i];
     if (isend(seekfor)) return 0;
    } return 1;
   }
  } else {
   matchstarted = true;
  }
  lastchar = c;
 }
 return 0;
}
