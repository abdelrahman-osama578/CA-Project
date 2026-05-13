#ifndef PARSER_H
#define PARSER_H

#include "cpu.h"

// Reads an assembly file and populates the CPU instruction memory
void load_program(const char *filename);

#endif // PARSER_H