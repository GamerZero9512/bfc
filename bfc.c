/*
    +-----+
    | BFC |
    +-----+

    Code-formatting Brainfuck compiler and transpiler with codegen support for C and Linux x86_64 NASM
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

enum target {
  TGT_C                   = (0 << 1) | 1,

  TGT_X86_64_LINUX_NASM   = (1 << 1) | 0,
  TGT_X86_64_LINUX_AS     = (2 << 1) | 0,
  TGT_X86_64_LINUX_FASM   = (3 << 1) | 0,
  TGT_X86_LINUX_NASM      = (4 << 1) | 0,
  TGT_X86_LINUX_AS        = (5 << 1) | 0,
  TGT_X86_LINUX_FASM      = (6 << 1) | 0,
  
  TGT_AARCH64_LINUX_AS    = (7 << 1) | 0,

  /*
      TODO:
        Other architectures:
        - AARCH64
        - ARM
        - RISCV
        - 6502 (for fun)
  */
};

int main(int argc, char **argv) {
  /* file streams */
  FILE *in = stdin, *out = stdout;
  /* compilation variables */
  int ch, count, indent, orig;
  /* nasm loop stack */
  int loopstack[1024];
  int looptop = 0, loopnext = 0, id;
  /* arg parsing variables */
  int i, j;
  char *arg, *output, *end;
  /* options */
  bool optimise = false, debug = false;
  int memsize = 65536, indsize = 2;
  char *input = NULL;
  enum target target = TGT_C;

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
          printf("bfc: brainfuck compiler\n\nusage: %s [options] [--] [infile]\n"
                 "\n"
                 "options:\n"
                 "  -h            show this help message\n"
                 "  -t <target>   change output language\n"
                 "  -O            enable optimisations\n"
                 "  -d            enable debug mode\n"
                 "  -o <file>     set output filename\n"
                 "  -m <memsize>  set memory tape size\n"
                 "  -i <indsize>  set indentation length\n"
                 "\n"
                 "targets:\n"
                 "  c\n"
                 "  x86_64-linux-nasm\n"
                 "  x86_64-linux-as\n"
                 "  x86_64-linux-fasm\n"
                 "  x86-linux-nasm\n"
                 "  x86-linux-as\n"
                 "  x86-linux-fasm\n"
                 "  aarch64-linux-as\n"
          , argv[0]);
          return 0;
        case 't':
          if(arg[j + 1]) output = &arg[j + 1];
          else if(++i < argc) output = argv[i];
          else {
            fprintf(stderr, "flag 't' requires an argument\n");
            return 1;
          }
          j = strlen(arg) - 1;

          if(strcmp(output, "c") == 0)                      target = TGT_C;
          else if(strcmp(output, "x86_64-linux-nasm") == 0) target = TGT_X86_64_LINUX_NASM;
          else if(strcmp(output, "x86_64-linux-as") == 0)   target = TGT_X86_64_LINUX_AS;
          else if(strcmp(output, "x86_64-linux-fasm") == 0) target = TGT_X86_64_LINUX_FASM;
          else if(strcmp(output, "x86-linux-nasm") == 0)    target = TGT_X86_LINUX_NASM;
          else if(strcmp(output, "x86-linux-as") == 0)      target = TGT_X86_LINUX_AS;
          else if(strcmp(output, "x86-linux-fasm") == 0)    target = TGT_X86_LINUX_FASM;
          else if(strcmp(output, "aarch64-linux-as") == 0)  target = TGT_AARCH64_LINUX_AS;
          else {
            fprintf(stderr, "invalid target: '%s'\n", output);
            return 1;
          }
          break;
        case 'O':
          optimise = true;
          break;
        case 'd':
          debug = true;
          break;
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
        case 'i':
          if(arg[j + 1]) output = &arg[j + 1];
          else if(++i < argc) output = argv[i];
          else {
            fprintf(stderr, "flag 'i' requires an argument\n");
            return 1;
          }
          j = strlen(arg) - 1;

          indsize = strtol(output, &end, 10);
          if(*end != '\0' || indsize < 0) {
              fprintf(stderr, "invalid indentation size: %s\n", output);
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
    if(strcmp(input, "-") == 0) in = stdin;
    else {
      in = fopen(input, "rb");
      if(!in) {
        fprintf(stderr, "error opening '%s': %s\n", input, strerror(errno));
        return 1;
      }
    }
  }

  indent = 1;

  switch(target) {
    case TGT_C:
      fprintf(out, "/* GENERATED BY BFC BRAINFUCK COMPILER */\n\n");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out, "; GENERATED BY BFC BRAINFUCK COMPILER\n\n");
      break;
    case TGT_X86_64_LINUX_AS:
    case TGT_X86_LINUX_AS:
    case TGT_AARCH64_LINUX_AS:
      fprintf(out, "# GENERATED BY BFC BRAINFUCK COMPILER\n\n");
      break;
  }

  if(debug) switch(target) {
    case TGT_C:
      fprintf(out, "/* START BFC PROLOGUE */\n");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out, "; START BFC PROLOGUE\n");
      break;
    case TGT_X86_64_LINUX_AS:
    case TGT_X86_LINUX_AS:
    case TGT_AARCH64_LINUX_AS:
      fprintf(out, "# START BFC PROLOGUE\n");
      break;
  }

  switch(target) {
    case TGT_C:
      fprintf(out,
        "#include <stdio.h>\n"
        "#include <stdint.h>\n"
        "\n"
        "int main(void) {\n"
        "%*suint8_t bfc_buf[%d] = {0};\n"
        "%*suint8_t *bfc_ptr = bfc_buf;\n"
        "%*sint bfc_ch;\n"
      , indent * indsize, "", memsize, indent * indsize, "", indent * indsize, "");
      break;
    case TGT_X86_64_LINUX_NASM:
      fprintf(out,
        "BITS 64\n"
        "global _start\n"
        "\n"
        "section .bss\n"
        "bfc_buf resb %d\n"
        "\n"
        "section .text\n"
        "_start:\n"
        "%*slea r12, [rel bfc_buf]\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_X86_64_LINUX_AS:
      fprintf(out,
        ".bss\n"
        ".lcomm bfc_buf, %d\n"
        "\n"
        ".text\n"
        ".globl _start\n"
        "_start:\n"
        "%*sleaq bfc_buf(%%rip), %%r12\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_X86_64_LINUX_FASM:
      fprintf(out,
        "format ELF64 executable\n"
        "entry _start\n"
        "\n"
        "segment readable writable\n"
        "bfc_buf rb %d\n"
        "\n"
        "segment readable executable\n"
        "_start:\n"
        "%*slea r12, [rel bfc_buf]\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_X86_LINUX_NASM:
      fprintf(out,
        "BITS 32\n"
        "global _start\n"
        "\n"
        "section .bss\n"
        "bfc_buf resb %d\n"
        "\n"
        "section .text\n"
        "_start:\n"
        "%*smov esi, bfc_buf\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_X86_LINUX_AS:
      fprintf(out,
        ".bss\n"
        ".lcomm bfc_buf, %d\n"
        "\n"
        ".text\n"
        ".globl _start\n"
        "_start:\n"
        "%*smovl $bfc_buf, %%esi\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_X86_LINUX_FASM:
      fprintf(out,
        "format ELF executable\n"
        "entry _start\n"
        "\n"
        "segment readable writable\n"
        "bfc_buf rb %d\n"
        "\n"
        "segment readable executable\n"
        "_start:\n"
        "%*smov esi, bfc_buf\n"
      , memsize, indent * indsize, "");
      break;
    case TGT_AARCH64_LINUX_AS:
      fprintf(out,
        ".bss\n"
        ".lcomm bfc_buf, %d\n"
        "\n"
        ".text\n"
        ".globl _start\n"
        "_start:\n"
        "%*sadrp x19, bfc_buf\n"
        "%*sadd x19, x19, :lo12:bfc_buf\n"
      , memsize, indent * indsize, "", indent * indsize, "");
  }

  if(debug) switch(target) {
    case TGT_C:
      fprintf(out, "/* END BFC PROLOGUE */\n");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out, "; END BFC PROLOGUE\n");
      break;
    case TGT_X86_64_LINUX_AS:
    case TGT_X86_LINUX_AS:
    case TGT_AARCH64_LINUX_AS:
      fprintf(out, "# END BFC PROLOGUE\n");
      break;
  }

  fputc('\n', out);

  i = 0;
  while((ch = fgetc(in)) != EOF) {
    orig = ch;
    switch(ch) {
      case '+':
      case '-':
        count = ch == '+' ? 1 : -1;
        if(optimise) {
          while((ch = fgetc(in)) != EOF) {
            if(ch == '+') count++;
            else if(ch == '-') count--;
            else if(!strchr("+-><,.[]", ch)) continue;
            else {
              ungetc(ch, in);
              break;
            }
          }
        }
        switch(target) {
          case TGT_C:
            if(count == 1)       fprintf(out, "%*s(*bfc_ptr)++;", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*s(*bfc_ptr)--;", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*s*bfc_ptr += %d;", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*s*bfc_ptr -= %d;", indent * indsize, "", -count);
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            if(count == 1)       fprintf(out, "%*sinc byte [r12]", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdec byte [r12]", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*sadd byte [r12], %d", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssub byte [r12], %d", indent * indsize, "", -count);
            break;
          case TGT_X86_64_LINUX_AS:
            if(count == 1)       fprintf(out, "%*sincb (%%r12)", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdecb (%%r12)", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*saddb $%d, (%%r12)", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssubb $%d, (%%r12)", indent * indsize, "", -count);
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            if(count == 1)       fprintf(out, "%*sinc byte [esi]", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdec byte [esi]", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*sadd byte [esi], %d", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssub byte [esi], %d", indent * indsize, "", -count);
            break;
          case TGT_X86_LINUX_AS:
            if(count == 1)       fprintf(out, "%*sincb (%%esi)", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdecb (%%esi)", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*saddb $%d, (%%esi)", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssubb $%d, (%%esi)", indent * indsize, "", -count);
            break;
          case TGT_AARCH64_LINUX_AS:
            if(count > 0)       fprintf(out, "%*sldrb w9, [x19]\n"
                                              "%*sadd w9, w9, #%d\n"
                                              "%*sstrb w9, [x19]", indent * indsize, "", indent * indsize, "", count, indent * indsize, "");
            else if(count < 0)  fprintf(out, "%*sldrb w9, [x19]\n"
                                             "%*ssub w9, w9, #%d\n"
                                             "%*sstrb w9, [x19]", indent * indsize, "", indent * indsize, "", -count, indent * indsize, "");
        }

        if(debug) switch(target) {
          case TGT_C:
            if(optimise) fprintf(out, " /* (%d) %c */", count > 0 ? count : -count, orig);
            else         fprintf(out, " /* %c */", orig);
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            if(optimise) fprintf(out, " ; (%d) %c", count > 0 ? count : -count, orig);
            else         fprintf(out, " ; %c", orig);
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            if(optimise) fprintf(out, " # (%d) %c", count > 0 ? count : -count, orig);
            else         fprintf(out, " # %c", orig);
            break;
        }
        fputc('\n', out);
        break;
      case '>':
      case '<':
        count = ch == '>' ? 1 : -1;
        if(optimise) {
          while((ch = fgetc(in)) != EOF) {
            if(ch == '>') count++;
            else if(ch == '<') count--;
            else if(!strchr("+-><,.[]", ch)) continue;
            else {
              ungetc(ch, in);
              break;
            }
          }
        }
        switch(target) {
          case TGT_C:
            if(count == 1)       fprintf(out, "%*sbfc_ptr++;", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sbfc_ptr--;", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*sbfc_ptr += %d;", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*sbfc_ptr -= %d;", indent * indsize, "", -count);
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            if(count == 1)       fprintf(out, "%*sinc r12", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdec r12", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*sadd r12, %d", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssub r12, %d", indent * indsize, "", -count);
            break;
          case TGT_X86_64_LINUX_AS:
            if(count == 1)       fprintf(out, "%*sincq %%r12", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdecq %%r12", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*saddq $%d, %%r12", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssubq $%d, %%r12", indent * indsize, "", -count);
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            if(count == 1)       fprintf(out, "%*sinc esi", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdec esi", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*sadd esi, %d", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssub esi, %d", indent * indsize, "", -count);
            break;
          case TGT_X86_LINUX_AS:
            if(count == 1)       fprintf(out, "%*sincl %%esi", indent * indsize, "");
            else if(count == -1) fprintf(out, "%*sdecl %%esi", indent * indsize, "");
            else if(count > 0)   fprintf(out, "%*saddl $%d, %%esi", indent * indsize, "", count);
            else if(count < 0)   fprintf(out, "%*ssubl $%d, %%esi", indent * indsize, "", -count);
            break;
          case TGT_AARCH64_LINUX_AS:
            if(count > 0)       fprintf(out, "%*sadd x19, x19, #%d", indent * indsize, "", count);
            else if(count < 0)  fprintf(out, "%*ssub x19, x19, #%d", indent * indsize, "", -count);
        }

        if(debug) switch(target) {
          case TGT_C:
            if(optimise) fprintf(out, " /* (%d) %c */", count > 0 ? count : -count, orig);
            else         fprintf(out, " /* %c */", orig);
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            if(optimise) fprintf(out, " ; (%d) %c", count > 0 ? count : -count, orig);
            else         fprintf(out, " ; %c", orig);
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            if(optimise) fprintf(out, " # (%d) %c", count > 0 ? count : -count, orig);
            else         fprintf(out, " # %c", orig);
            break;
        }
        fputc('\n', out);
        break;
      case '.':
        switch(target) {
          case TGT_C:
            fprintf(out, "%*sputchar(*bfc_ptr);", indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            fprintf(out, "%*smov rax, 1\n"
                         "%*smov rdi, 1\n"
                         "%*smov rsi, r12\n"
                         "%*smov rdx, 1\n"
                         "%*ssyscall",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_AS:
            fprintf(out, "%*smovq $1, %%rax\n"
                         "%*smovq $1, %%rdi\n"
                         "%*smovq %%r12, %%rsi\n"
                         "%*smovq $1, %%rdx\n"
                         "%*ssyscall",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, "%*smov eax, 4\n"
                         "%*smov ebx, 1\n"
                         "%*smov ecx, esi\n"
                         "%*smov edx, 1\n"
                         "%*sint 0x80",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_LINUX_AS:
            fprintf(out, "%*smovl $4, %%eax\n"
                         "%*smovl $1, %%ebx\n"
                         "%*smovl %%esi, %%ecx\n"
                         "%*smovl $1, %%edx\n"
                         "%*sint $0x80",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, "%*smov x8, #64\n"
                         "%*smov x0, #1\n"
                         "%*smov x1, x19\n"
                         "%*smov x2, #1\n"
                         "%*ssvc #0",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
        }
        if(debug) switch(target) {
          case TGT_C:
            fprintf(out, " /* . */");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, " ; .");
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, " # .");
            break;
        }
        fputc('\n', out);
        break;
      case ',':
        switch(target) {
          case TGT_C:
            fprintf(out, "%*sif((bfc_ch = getchar()) == EOF) *bfc_ptr = 0;\n"
                         "%*selse *bfc_ptr = bfc_ch;", indent * indsize, "", indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            fprintf(out, "%*smov byte [r12], 0\n"
                         "%*smov rax, 0\n"
                         "%*smov rdi, 0\n"
                         "%*smov rsi, r12\n"
                         "%*smov rdx, 1\n"
                         "%*ssyscall",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_AS:
            fprintf(out, "%*smovb $0, (%%r12)\n"
                         "%*smovq $0, %%rax\n"
                         "%*smovq $0, %%rdi\n"
                         "%*smovq %%r12, %%rsi\n"
                         "%*smovq $1, %%rdx\n"
                         "%*ssyscall",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, "%*smov byte [esi], 0\n"
                         "%*smov eax, 3\n"
                         "%*smov ebx, 0\n"
                         "%*smov ecx, esi\n"
                         "%*smov edx, 1\n"
                         "%*sint 0x80",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_X86_LINUX_AS:
            fprintf(out, "%*smovl $0, (%%esi)\n"
                         "%*smovl $3, %%eax\n"
                         "%*smovl $0, %%ebx\n"
                         "%*smovl %%esi, %%ecx\n"
                         "%*smovl $1, %%edx\n"
                         "%*sint $0x80",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, "%*sstrb wzr, [x19]\n"
                         "%*smov x8, #63\n"
                         "%*smov x0, #0\n"
                         "%*smov x1, x19\n"
                         "%*smov x2, #1\n"
                         "%*ssvc #0",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "",
                         indent * indsize, "");
            break;
        }
        if(debug) switch(target) {
          case TGT_C:
            fprintf(out, " /* , */");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, " ; ,");
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, " # ,");
            break;
        }
        fputc('\n', out);
        break;
      case '[':
        switch(target) {
          case TGT_C:
            fprintf(out, "%*swhile(*bfc_ptr) {", indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            id = loopnext++;
            loopstack[looptop++] = id;
            fprintf(out, "loop%d:\n"
                         "%*scmp byte [r12], 0\n"
                         "%*sje end%d", id, indent * indsize, "", indent * indsize, "", id);
            break;
          case TGT_X86_64_LINUX_AS:
            id = loopnext++;
            loopstack[looptop++] = id;
            fprintf(out, "loop%d:\n"
                         "%*scmpb $0, (%%r12)\n"
                         "%*sje end%d", id, indent * indsize, "", indent * indsize, "", id);
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            id = loopnext++;
            loopstack[looptop++] = id;
            fprintf(out, "loop%d:\n"
                         "%*scmp byte [esi], 0\n"
                         "%*sje end%d", id, indent * indsize, "", indent * indsize, "", id);
            break;
          case TGT_X86_LINUX_AS:
            id = loopnext++;
            loopstack[looptop++] = id;
            fprintf(out, "loop%d:\n"
                         "%*scmpb $0, (%%esi)\n"
                         "%*sje end%d", id, indent * indsize, "", indent * indsize, "", id);
            break;
          case TGT_AARCH64_LINUX_AS:
            id = loopnext++;
            loopstack[looptop++] = id;
            fprintf(out, "loop%d:\n"
                         "%*sldrb w9, [x19]\n"
                         "%*scbz w9, end%d", id, indent * indsize, "", indent * indsize, "", id);
            break;
        }
        if(debug) switch(target) {
          case TGT_C:
            fprintf(out, " /* [ */");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, " ; [");
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, " # [");
            break;
        }
        fputc('\n', out);
        if(target & 1) indent++;
        break;
      case ']':
        if(indent <= 1 && target & 1) {
          fprintf(stderr, "mismatched parenthesis%s\n", debug ? " (close)" : "");
          return 1;
        }
        if(target & 1) indent--;
        switch(target) {
          case TGT_C:
            fprintf(out, "%*s}", indent * indsize, "");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
            id = loopstack[--looptop];
            fprintf(out, "%*scmp byte [r12], 0\n"
                         "%*sjne loop%d\n"
                         "end%d:", indent * indsize, "", indent * indsize, "", id, id);
            break;
          case TGT_X86_64_LINUX_AS:
            id = loopstack[--looptop];
            fprintf(out, "%*scmpb $0, (%%r12)\n"
                         "%*sjne loop%d\n"
                         "end%d:", indent * indsize, "", indent * indsize, "", id, id);
            break;
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            id = loopstack[--looptop];
            fprintf(out, "%*scmp byte [esi], 0\n"
                         "%*sjne loop%d\n"
                         "end%d:", indent * indsize, "", indent * indsize, "", id, id);
            break;
          case TGT_X86_LINUX_AS:
            id = loopstack[--looptop];
            fprintf(out, "%*scmpb $0, (%%esi)\n"
                         "%*sjne loop%d\n"
                         "end%d:", indent * indsize, "", indent * indsize, "", id, id);
            break;
          case TGT_AARCH64_LINUX_AS:
            id = loopstack[--looptop];
            fprintf(out, "%*sldrb w9, [x19]\n"
                         "%*scbnz w9, loop%d\n"
                         "end%d:", indent * indsize, "", indent * indsize, "", id, id);
            break;
        }
        if(debug) switch(target) {
          case TGT_C:
            fprintf(out, " /* ] */");
            break;
          case TGT_X86_64_LINUX_NASM:
          case TGT_X86_64_LINUX_FASM:
          case TGT_X86_LINUX_NASM:
          case TGT_X86_LINUX_FASM:
            fprintf(out, " ; ]");
            break;
          case TGT_X86_64_LINUX_AS:
          case TGT_X86_LINUX_AS:
          case TGT_AARCH64_LINUX_AS:
            fprintf(out, " # ]");
            break;
        }
        fputc('\n', out);
        break;
      default:
        break;
    }
    i++;
  }
  if(indent != 1 && target & 1) {
    fprintf(stderr, "mismatched parenthesis%s\n", debug ? " (open)" : "");
    return 1;
  }
  fputc('\n', out);
  if(debug) switch(target) {
    case TGT_C:
      fprintf(out, "/* START BFC EPILOGUE */\n");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out, "; START BFC EPILOGUE\n");
      break;
    case TGT_X86_64_LINUX_AS:
    case TGT_X86_LINUX_AS:
    case TGT_AARCH64_LINUX_AS:
      fprintf(out, "# START BFC EPILOGUE\n");
      break;
  }

  switch(target) {
    case TGT_C:
      fprintf(out,
        "%*sreturn 0;\n"
        "}\n"
      , indent * indsize, "");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
      fprintf(out,
        "%*smov rax, 60\n"
        "%*smov rdi, 0\n"
        "%*ssyscall\n"
      , indent * indsize, "", indent * indsize, "", indent * indsize, "");
      break;
    case TGT_X86_64_LINUX_AS:
      fprintf(out,
        "%*smovq $60, %%rax\n"
        "%*smovq $0, %%rdi\n"
        "%*ssyscall\n"
      , indent * indsize, "", indent * indsize, "", indent * indsize, "");
      break;
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out,
        "%*smov eax, 1\n"
        "%*smov ebx, 0\n"
        "%*sint 0x80\n"
      , indent * indsize, "", indent * indsize, "", indent * indsize, "");
      break;
    case TGT_X86_LINUX_AS:
      fprintf(out,
        "%*smovl $1, %%eax\n"
        "%*smovl $0, %%ebx\n"
        "%*sint $0x80\n"
      , indent * indsize, "", indent * indsize, "", indent * indsize, "");
      break;
    case TGT_AARCH64_LINUX_AS:
      fprintf(out,
        "%*smov x8, #93\n"
        "%*smov x0, #0\n"
        "%*ssvc #0\n"
      , indent * indsize, "", indent * indsize, "", indent * indsize, "");
  }
  if(debug) switch(target) {
    case TGT_C:
      fprintf(out, "/* END BFC EPILOGUE */\n");
      break;
    case TGT_X86_64_LINUX_NASM:
    case TGT_X86_64_LINUX_FASM:
    case TGT_X86_LINUX_NASM:
    case TGT_X86_LINUX_FASM:
      fprintf(out, "; END BFC EPILOGUE\n");
      break;
    case TGT_X86_64_LINUX_AS:
    case TGT_X86_LINUX_AS:
    case TGT_AARCH64_LINUX_AS:
      fprintf(out, "# END BFC EPILOGUE\n");
      break;
  }
  if(in != stdin) fclose(in);
  if(out != stdout) fclose(out);
  return 0;
}
