#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// Helper function to extract the integer value from a string like "R15"
int parse_register(const char* reg_str) {
    if (reg_str == NULL || reg_str[0] != 'R') return 0;
    return atoi(&reg_str[1]); // Convert the number part to an integer
}

void load_program(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open %s\n", filename);
        exit(1);
    }

    char line[256];
    uint32_t current_address = 0;

    // Read the file line by line
    while (fgets(line, sizeof(line), file)) {
        if (current_address >= INSTRUCTION_LIMIT) {
            printf("Error: Instruction memory overflow!\n");
            break;
        }

        // Ignore empty lines
        if (strlen(line) <= 1) continue;

        char mnemonic[10];
        char op1[20] = "", op2[20] = "", op3[20] = "";
        
        // Parse the line (assuming space-separated values like "ADD R1 R2 R3")
        int tokens = sscanf(line, "%s %s %s %s", mnemonic, op1, op2, op3);
        if (tokens < 1) continue;

        uint32_t instruction = 0;
        uint32_t opcode = 0, r1 = 0, r2 = 0, r3 = 0, shamt = 0;
        int32_t imm = 0;
        uint32_t address = 0;

        // 1. Identify Opcode and Format [cite: 54, 61]
        if (strcmp(mnemonic, "ADD") == 0) {
            opcode = OPCODE_ADD;
            r1 = parse_register(op1);
            r2 = parse_register(op2);
            r3 = parse_register(op3);
            // R-Format: Shift values into their respective bit positions
            instruction = (opcode << 28) | (r1 << 23) | (r2 << 18) | (r3 << 13) | (shamt & 0x1FFF);
        } 
        else if (strcmp(mnemonic, "ADDI") == 0) {
            opcode = OPCODE_ADDI;
            r1 = parse_register(op1);
            r2 = parse_register(op2);
            imm = atoi(op3); // Immediate value
            // I-Format: Mask immediate to 18 bits to handle negatives correctly
            instruction = (opcode << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        }
        else if (strcmp(mnemonic, "J") == 0) {
            opcode = OPCODE_J;
            address = atoi(op1);
            // J-Format: 28-bit address
            instruction = (opcode << 28) | (address & 0xFFFFFFF);
        }
        // ... (You will need to add the remaining else-if blocks for SUB, MULI, BNE, ANDI, XORI, SLL, SRL, LW, SW) ...
        
        // Special case to remember: SLL and SRL have R3 = 0 in the instruction format [cite: 62]

        // Store the 32-bit concatenated instruction into main memory [cite: 30]
        cpu.memory[current_address] = instruction;
        current_address++;
    }

    fclose(file);
    printf("Program loaded successfully. Total instructions: %d\n", current_address);
}