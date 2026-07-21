# bfc
Code-formatting Brainfuck compiler and transpiler with codegen support for C and Linux x86_64 NASM

```
bfc: brainfuck compiler

usage: bfc [options] [--] [infile]

options:
  -h            show this help message
  -t <c|nasm>   change output language
  -O            enable optimisations
  -d            enable debug mode
  -o <file>     set output filename
  -m <memsize>  set memory tape size
  -i <indsize>  set indentation length
```
