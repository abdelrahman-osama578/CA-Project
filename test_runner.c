#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cpu.h"
#include "pipeline.h"

// ---------------------------------------------------------
// GLOBALS & INITIALIZATION (Moved from main.c for testing)
// ---------------------------------------------------------
CPU cpu;
IF_ID_Latch if_id;
ID_EX_Latch id_ex;
EX_MEM_Latch ex_mem;
MEM_WB_Latch mem_wb;
bool mem_active_this_cycle = false;
int num_parsed_instructions = 0;

void initialize_cpu()
{
    memset(&cpu, 0, sizeof(CPU));
    memset(&if_id, 0, sizeof(IF_ID_Latch));
    memset(&id_ex, 0, sizeof(ID_EX_Latch));
    memset(&ex_mem, 0, sizeof(EX_MEM_Latch));
    memset(&mem_wb, 0, sizeof(MEM_WB_Latch));
    cpu.clock_cycle = 1;
}

extern bool mem_active_this_cycle;

// ---------------------------------------------------------
// 1. TEST BLUEPRINT
// ---------------------------------------------------------
typedef struct
{
    char test_name[100];
    char assembly_code[2048];
    char target_type; // 'R' for Register, 'M' for Memory
    int target_address;
    int32_t expected_value;
} TestCase;

// ---------------------------------------------------------
// 2. IN-MEMORY PARSER (Replaces parser.c for testing)
// ---------------------------------------------------------
void load_program_from_string(const char *code)
{
    char code_copy[2048];
    strcpy(code_copy, code);

    char *line = strtok(code_copy, "\n");
    int inst_index = 0;

    while (line != NULL)
    {
        if (strlen(line) == 0 || line[0] == '\r')
        {
            line = strtok(NULL, "\n");
            continue;
        }

        char opcode_str[10];
        int r1 = 0, r2 = 0, r3 = 0, imm = 0, shamt = 0, address = 0;
        uint32_t instruction = 0;

        sscanf(line, "%s", opcode_str);

        if (strcmp(opcode_str, "ADD") == 0 || strcmp(opcode_str, "SUB") == 0)
        {
            sscanf(line, "%*s R%d R%d R%d", &r1, &r2, &r3);
            uint8_t op = (strcmp(opcode_str, "ADD") == 0) ? OPCODE_ADD : OPCODE_SUB;
            instruction = (op << 28) | (r1 << 23) | (r2 << 18) | (r3 << 13);
        }
        else if (strcmp(opcode_str, "ADDI") == 0 || strcmp(opcode_str, "MULI") == 0 ||
                 strcmp(opcode_str, "ANDI") == 0 || strcmp(opcode_str, "XORI") == 0 ||
                 strcmp(opcode_str, "BNE") == 0 || strcmp(opcode_str, "LW") == 0 ||
                 strcmp(opcode_str, "SW") == 0)
        {
            sscanf(line, "%*s R%d R%d %d", &r1, &r2, &imm);
            uint8_t op = (strcmp(opcode_str, "ADDI") == 0) ? OPCODE_ADDI : (strcmp(opcode_str, "MULI") == 0) ? OPCODE_MULI
                                                                       : (strcmp(opcode_str, "ANDI") == 0)   ? OPCODE_ANDI
                                                                       : (strcmp(opcode_str, "XORI") == 0)   ? OPCODE_XORI
                                                                       : (strcmp(opcode_str, "BNE") == 0)    ? OPCODE_BNE
                                                                       : (strcmp(opcode_str, "LW") == 0)     ? OPCODE_LW
                                                                                                             : OPCODE_SW;
            instruction = (op << 28) | (r1 << 23) | (r2 << 18) | (imm & 0x3FFFF);
        }
        else if (strcmp(opcode_str, "SLL") == 0 || strcmp(opcode_str, "SRL") == 0)
        {
            sscanf(line, "%*s R%d R%d %d", &r1, &r2, &shamt);
            uint8_t op = (strcmp(opcode_str, "SLL") == 0) ? OPCODE_SLL : OPCODE_SRL;
            instruction = (op << 28) | (r1 << 23) | (r2 << 18) | (0 << 13) | (shamt & 0x1FFF);
        }
        else if (strcmp(opcode_str, "J") == 0)
        {
            sscanf(line, "%*s %d", &address);
            instruction = (OPCODE_J << 28) | (address & 0xFFFFFFF);
        }

        cpu.memory[inst_index++] = instruction;
        line = strtok(NULL, "\n");
    }
    num_parsed_instructions = inst_index;
}

// ---------------------------------------------------------
// 3. CORE TESTING ENGINE
// ---------------------------------------------------------
bool run_test(TestCase test)
{
    initialize_cpu();
    load_program_from_string(test.assembly_code);

    while (1)
    {
        if (cpu.clock_cycle > 150)
            break; // Infinite loop safety net
        mem_active_this_cycle = false;

        writeback();
        memory();
        execute();
        decode();
        fetch();

        if (!if_id.is_valid && !id_ex.is_valid && !ex_mem.is_valid && !mem_wb.is_valid && cpu.pc >= num_parsed_instructions)
        {
            break;
        }
        cpu.clock_cycle++;
    }

    int32_t actual_value = (test.target_type == 'R') ? cpu.registers[test.target_address] : cpu.memory[test.target_address];

    if (actual_value == test.expected_value)
    {
        printf("[PASS] %s\n", test.test_name);
        return true;
    }
    else
    {
        printf("[FAIL] %s\n       -> Expected %c[%d] to be %d, got %d\n",
               test.test_name, test.target_type, test.target_address, test.expected_value, actual_value);
        return false;
    }
}

// ---------------------------------------------------------
// 4. THE MASTER TEST SUITE
// ---------------------------------------------------------
int main()
{
    TestCase tests[] = {

        // --- GROUP 1: HAPPY PATH ---
        {
            "1.1 Pure Arithmetic",
            "ADDI R1 R0 5\n"
            "ADDI R2 R0 10\n"
            "ADD R0 R0 R0\n"
            "ADD R0 R0 R0\n"
            "ADD R3 R1 R2\n",
            'R', 3, 15},
        {"1.2 Standard Memory Access",
         "ADDI R1 R0 1024\n"
         "ADDI R2 R0 99\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "SW R2 R1 0\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "LW R3 R1 0\n",
         'R', 3, 99},
        {"1.3 Untaken Branch",
         "BNE R0 R0 5\n"
         "ADDI R1 R0 10\n",
         'R', 1, 10},
        {"1.4 Bitwise & Sign Extension",
         "ADDI R1 R0 15\n"
         "ADDI R2 R0 -5\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "ANDI R3 R1 10\n"
         "XORI R4 R1 5\n",
         'R', 4, 10},
        {"1.5 Shift Mechanics",
         "ADDI R1 R0 16\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "SRL R2 R1 2\n"
         "SLL R3 R2 3\n",
         'R', 3, 32},
        {"1.6 Multiplication",
         "ADDI R1 R0 5\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "MULI R2 R1 -3\n",
         'R', 2, -15},
        {"1.7 Unconditional Jump",
         "J 4\n"
         "ADDI R1 R0 99\n" // Ghost
         "ADDI R2 R0 99\n" // Ghost
         "ADDI R3 R0 99\n" // Ghost
         "ADDI R4 R0 1\n", // Target
         'R', 4, 1},
        {"1.8 Taken BNE",
         "ADDI R1 R0 1\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "BNE R1 R0 2\n"
         "ADDI R5 R0 99\n" // Ghost
         "ADDI R6 R0 99\n" // Ghost
         "ADDI R7 R0 1\n", // Target
         'R', 7, 1},

        // --- GROUP 2: BOUNDARY TESTS ---
        {
            "2.1 Immediate Extremes (Max Pos/Neg 18-bit)",
            "ADDI R1 R0 131071\n"
            "ADDI R2 R0 -131072\n",
            'R', 2, -131072},
        {"2.2 Memory Upper Bound (Address 2047)",
         "ADDI R1 R0 2047\n"
         "ADDI R6 R0 99\n"
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "SW R6 R1 0\n",
         'M', 2047, 99},
        {"2.3 Shift Cast Trap (Logical vs Arithmetic)",
         "ADDI R1 R0 -1\n" // 1111...1111
         "ADD R0 R0 R0\n"
         "ADD R0 R0 R0\n"
         "SRL R2 R1 31\n",
         'R', 2, 1},
        {"2.4 Von Neumann Bottleneck (Back-to-Back Memory)",
         "ADDI R9 R0 42\n"
         "SW R9 R0 1025\n"
         "LW R1 R0 1024\n"
         "LW R2 R0 1025\n",
         'R', 2, 42},
        {"2.5 Zero-Offset Branch (Infinite Loop Check)",
         "ADDI R1 R0 1\n"
         "BNE R1 R0 -1\n",
         'R', 1, 1},

        // --- GROUP 3: EXCEPTIONS / RULES ---
        {
            "3.1 Instruction Memory Out of Bounds",
            "J 1025\n",
            'R', 0, 0},
        {"3.2 Data Write Out of Bounds (Protecting Inst Mem)",
         // The instruction "ADDI R1 R0 5" translates to decimal 813694981.
         // If the SW command incorrectly overwrites Address 0, MEM[0] will equal 5.
         // If the boundary check is secure, MEM[0] will remain the original instruction.
         "ADDI R1 R0 5\n"
         "SW R1 R0 0\n",
         'M', 0, 813694981},
        {
            "3.3 The R0 Integrity Trap",
            "ADDI R0 R0 500\n"
            "ADD R5 R0 R0\n",
            'R', 5, 0 // R0 must remain 0, so R5 must become 0, not 1000.
        }};

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    printf("\n======================================================\n");
    printf("         STARTING AUTOMATED TEST SUITE                \n");
    printf("======================================================\n\n");

    for (int i = 0; i < num_tests; i++)
    {
        if (run_test(tests[i]))
            passed++;
    }

    printf("\n======================================================\n");
    printf("RESULTS: %d/%d Tests Passed\n", passed, num_tests);
    printf("======================================================\n\n");

    return 0;
}