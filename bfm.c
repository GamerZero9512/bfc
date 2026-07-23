/*
    +-----+
    | BFM |
    +-----+

    Tiny brainfuck minifier
    Part of the bfc project
*/

#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv) {
  int i, j, ch;
  char *arg, *input = NULL, *output = NULL;
  FILE *in = stdin, *out = stdout;

  for(i = 1; i < argc; i++) {
    arg = argv[i];
    if(strcmp(arg, "--") == 0) {
      i++;
      break;
    }
    if(arg[0] != '-' || arg[1] == '\0') {
      input = arg;
      i++;
      break;
    }
    for(j = 1; arg[j]; j++) {
      switch(arg[j]) {
        case 'h':
          printf("bfm: brainfuck minifier\n\nusage: %s [options] [--] [infile]\n"
                 "\n"
                 "options:\n"
                 "  -h            show this help message\n"
                 "  -o <file>     set output filename\n"
          , argv[0]);
          return 0;
        case 'o':
          if(arg[j + 1]) output = &arg[j + 1];
          else if(++i < argc) output = argv[i];
          else {
            fprintf(stderr, "flag 'o' requires an argument\n");
            return 1;
          }
          j = strlen(arg) - 1;

          if(strcmp(output, "-") == 0) out = stdout;
          else {
            out = fopen(output, "wb");
            if(!out) {
              fprintf(stderr, "error opening '%s': %s\n", output, strerror(errno));
              return 1;
            }
          }
          break;
        default:
          fprintf(stderr, "unrecognised flag: '%c'\n"
                          "try '-h' for help\n", arg[j]);
          return 1;
      }
    }
  }

  while(i < argc) {
    if(input) {
      fprintf(stderr, "too many input files\n");
      return 1;
    }
    input = argv[i++];
  }

  if(input) {
    if(strcmp(input, "-") == 0) in = stdin;
    else {
      in = fopen(input, "rb");
      if(!in) {
        fprintf(stderr, "error opening '%s': %s\n", input, strerror(errno));
        return 1;
      }
    }
  }

  while((ch = fgetc(in)) != EOF) if(strchr("+-><.,[]", ch)) fputc(ch, out);

  if(in != stdin) fclose(in);
  if(out != stdout) fclose(out);
  return 0;
}
