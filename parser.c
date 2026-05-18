#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "parser.h"

void load_program(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open %s\n", filename);
        exit(1);
    }

    char line[256];
    int inst_index = 0;

    while (fgets(line, sizeof(line), file)) {
        if (inst_index >= INSTRUCTION_LIMIT) {
            printf("Warning: Reached maximum instruction limit of %d. Stopping parse.\n", INSTRUCTION_LIMIT);
            break;
        }
        // Remove newline characters
        line[strcspn(line, "\r\n")] = 0;

        // Skip empty lines
        if (strlen(line) == 0) continue;

        char opcode_str[10];
        int r1 = 0, r2 = 0, r3 = 0, imm = 0, shamt = 0, address = 0;
        uint32_t instruction = 0;

        // Read the mnemonic first to determine the format
        sscanf(line, "%s", opcode_str);

        // Parse the operands based on the instruction format
        if (strcmp(opcode_str, "ADD") == 0 || strcmp(opcode_str, "SUB") == 0) {
            // Re-route the sscanf mapping so Dest goes to r3, Src1 to r1, Src2 to r2
            sscanf(line, "%*s R%d R%d R%d", &r3, &r1, &r2); 
            uint8_t op = (strcmp(opcode_str, "ADD") == 0) ? OPCODE_ADD : OPCODE_SUB;
            instruction = (op << 28) | (r1 << 23) | (r2 << 18) | (r3 << 13);
        }
        else if (strcmp(opcode_str, "ADDI") == 0 || strcmp(opcode_str, "MULI") == 0 ||
                 strcmp(opcode_str, "ANDI") == 0 || strcmp(opcode_str, "XORI") == 0 ||
                 strcmp(opcode_str, "BNE") == 0 || strcmp(opcode_str, "LW") == 0 ||
                 strcmp(opcode_str, "SW") == 0) {
            sscanf(line, "%*s R%d R%d %d", &r1, &r2, &imm);
        } 
        else if (strcmp(opcode_str, "SLL") == 0 || strcmp(opcode_str, "SRL") == 0) {
            sscanf(line, "%*s R%d R%d %d", &r1, &r2, &shamt);
        } 
        else if (strcmp(opcode_str, "J") == 0) {
            sscanf(line, "%*s %d", &address);
        }

        // ---------------------------------------------------------
        // PACK INTO 32-BIT BINARY FORMAT
        // ---------------------------------------------------------
        if (strcmp(opcode_str, "ADD") == 0) {
            instruction = (OPCODE_ADD << 28) | (r1 << 23) | (r2 << 18) | (r3 << 13);
        } 
        else if (strcmp(opcode_str, "SUB") == 0) {
            instruction = (OPCODE_SUB << 28) | (r1 << 23) | (r2 << 18) | (r3 << 13);
        } 
        else if (strcmp(opcode_str, "ADDI") == 0) {
            instruction = (OPCODE_ADDI << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "MULI") == 0) {
            instruction = (OPCODE_MULI << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "ANDI") == 0) {
            instruction = (OPCODE_ANDI << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "XORI") == 0) {
            instruction = (OPCODE_XORI << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "BNE") == 0) {
            instruction = (OPCODE_BNE << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "LW") == 0) {
            instruction = (OPCODE_LW << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "SW") == 0) {
            instruction = (OPCODE_SW << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        } 
        else if (strcmp(opcode_str, "SLL") == 0) {
            instruction = (OPCODE_SLL << 28) | (r1 << 23) | (r2 << 18) | (0 << 13) | (shamt & 0x1FFF);
        } 
        else if (strcmp(opcode_str, "SRL") == 0) {
            instruction = (OPCODE_SRL << 28) | (r1 << 23) | (r2 << 18) | (0 << 13) | (shamt & 0x1FFF);
        } 
        else if (strcmp(opcode_str, "J") == 0) {
            instruction = (OPCODE_J << 28) | (address & 0xFFFFFFF);
        }

        
        // Store the instruction in memory
        cpu.memory[inst_index] = instruction;
        inst_index++;
    }

    extern int num_parsed_instructions;
    num_parsed_instructions = inst_index;
    fclose(file);
}