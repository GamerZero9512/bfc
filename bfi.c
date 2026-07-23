/*
    +-----+
    | BFI |
    +-----+

    Tiny Brainf*ck interpreter
    Part of the bfc project
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv) {
  int i, j, memsize = 65536, ip = 0, *jump, *stack, sp;
  char *arg, *input = NULL, *output, *code = NULL;
  ssize_t size = 0;
  FILE *in = NULL;

  uint8_t *bfi_buf = NULL;
  uint8_t *bfi_ptr = bfi_buf;

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
          printf("bfi: brainfuck interpreter\n\nusage: %s [options] [--] infile\n"
                 "\n"
                 "options:\n"
                 "  -h            show this help message\n"
                 "  -m <memsize>  set memory tape size\n"
          , argv[0]);
          return 0;
        case 'm':
          if(arg[j + 1]) output = &arg[j + 1];
          else if(++i < argc) output = argv[i];
          else {
            fprintf(stderr, "flag 'm' requires an argument\n");
            return 1;
          }
          j = strlen(arg) - 1;

          memsize = atoi(output);
          if(!memsize) {
            fprintf(stderr, "invalid memory size: %s\n", output);
            return 1;
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
    in = fopen(input, "rb");
    if(!in) {
      fprintf(stderr, "error opening '%s': %s\n", input, strerror(errno));
      return 1;
    }
  } else {
    fprintf(stderr, "no input files\n");
    return 1;
  }

  bfi_buf = malloc(memsize);
  if(!bfi_buf) {
    fprintf(stderr, "error allocating memory: %s\n", strerror(errno));
    fclose(in);
    return 1;
  }
  bfi_ptr = bfi_buf;

  fseek(in, 0, SEEK_END);
  size = ftell(in);
  rewind(in);
  code = malloc(size + 1);
  if(!code) {
    fprintf(stderr, "error allocating file: %s\n", strerror(errno));
    free(bfi_buf);
    fclose(in);
    return 1;
  }
  fread(code, 1, size, in);
  code[size] = '\0';

  jump = malloc(sizeof(int) * size);
  if(!jump) {
    fprintf(stderr, "error allocating jump table: %s\n", strerror(errno));
    free(bfi_buf);
    fclose(in);
    return 1;
  }
  stack = malloc(sizeof(int) * size);
  if(!stack) {
    fprintf(stderr, "error allocating jump stack: %s\n", strerror(errno));
    free(jump);
    free(bfi_buf);
    fclose(in);
    return 1;
  }
  sp = 0;

  for(i = 0; i < size; i++) jump[i] = -1;
  
  for(i = 0; i < size; i++) {
    if(code[i] == '[') stack[sp++] = i;
    else if(code[i] == ']') {
      if(sp == 0) {
        fprintf(stderr, "mismatched parenthesis\n");
        free(jump);
        free(stack);
        free(bfi_buf);
        fclose(in);
        return 1;
      }
      j = stack[--sp];
      jump[i] = j;
      jump[j] = i;
    }
  }
  if(sp != 0) {
    fprintf(stderr, "mismatched parenthesis\n");
    free(jump);
    free(stack);
    free(bfi_buf);
    fclose(in);
    return 1;
  }

  while(ip < size) {
    switch(code[ip]) {
      case '+':
        (*bfi_ptr)++;
        break;
      case '-':
        (*bfi_ptr)--;
        break;
      case '>':
        bfi_ptr++;
        break;
      case '<':
        bfi_ptr--;
        break;
      case '.':
        putchar(*bfi_ptr);
        break;
      case ',':
        *bfi_ptr = getchar();
        break;
      case '[':
        if(!*bfi_ptr) ip = jump[ip];
        break;
      case ']':
        if(*bfi_ptr) ip = jump[ip];
        break;
      default:
        break;
    }
    ip++;
  }

  free(jump);
  free(stack);
  free(bfi_buf);
  fclose(in);
  return 0;
}
