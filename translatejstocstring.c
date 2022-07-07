#import <stdio.h>

void translateLine(char *line);

int main(int argc, char *argv[]) {
  char line[1001];
  printf("#import <stdint.h>\n\n");
  printf("static char %s[] = {\n", argc > 1 ? argv[1] : "data");
  while(1) {
    if (fgets(line, 1000, stdin) == NULL) {
      break;
    }
    translateLine(line);
  }
  printf("0 };\n");
  return 0;
}

void translateLine(char *line) {
  for (char *pos = line; pos && *pos; pos++) {
    printf("%d, ", (int)*pos);
  }
}
