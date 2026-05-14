#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

// Memory and Register definitions for Package 1
#define MEMORY_SIZE 2048      // 2048 rows 
#define INSTRUCTION_LIMIT 1024 // 0 to 1023 for instructions 
#define NUM_REGISTERS 32      // R0 to R31 [cite: 33, 39]

// Opcodes [cite: 54, 61]
#define OPCODE_ADD  0
#define OPCODE_SUB  1
#define OPCODE_MULI 2
#define OPCODE_ADDI 3
#define OPCODE_BNE  4
#define OPCODE_ANDI 5
#define OPCODE_XORI 6
#define OPCODE_J    7
#define OPCODE_SLL  8
#define OPCODE_SRL  9
#define OPCODE_LW   10
#define OPCODE_SW   11

// The Processor State
typedef struct {
    int32_t memory[MEMORY_SIZE]; // Main memory: shared for instructions and data [cite: 25]
    int32_t registers[NUM_REGISTERS]; // R0-R31. R0 is hard-wired to 0 [cite: 33, 41]
    uint32_t pc; // Program Counter [cite: 42]
    int clock_cycle; // Global clock cycle counter
} CPU;

/* * Pipeline Latch Structures
 * These structs act as buffers between stages to hold the state
 * of an instruction as it moves through the 5 stages.
 */

typedef struct {
    bool is_valid;       // Tells us if there is an active instruction here
    int32_t instruction; // The raw 32-bit instruction fetched
    uint32_t pc;         // The PC of this specific instruction (useful for BNE/J) [cite: 524]
    int decode_cycle_count;
} IF_ID_Latch;

typedef struct {
    bool is_valid;
    uint32_t pc;
    uint8_t opcode;
    uint8_t r1_addr; // Destination or Source 1
    uint8_t r2_addr; // Source 2
    uint8_t r3_addr; // Destination for R-Type
    int32_t r1_val;  // Read value from register file
    int32_t r2_val;  // Read value from register file
    int32_t immediate; // Sign-extended 18-bit immediate [cite: 51, 396]
    uint16_t shamt;    // 13-bit shift amount [cite: 50]
    uint32_t address;  // 28-bit jump address [cite: 52]
    int execute_cycle_count; // Tracks the 2 cycles needed in EX stage [cite: 89, 95]
} ID_EX_Latch;

typedef struct {
    bool is_valid;
    uint8_t opcode;
    uint8_t dest_reg;    // Where the result will go (R1 or R3)
    int32_t alu_result;  // Result of ADD, SUB, etc., or calculated Memory Address
    int32_t r1_val;      // Needed for SW (Store Word)
} EX_MEM_Latch;

typedef struct {
    bool is_valid;
    uint8_t opcode;
    uint8_t dest_reg;
    int32_t alu_result;  // Result passed through if no memory read
    int32_t mem_read_val;// Value read from memory (for LW)
} MEM_WB_Latch;

// Global instantiation of the CPU and Latches (to be defined in cpu.c or main.c)
extern CPU cpu;
extern IF_ID_Latch if_id;
extern ID_EX_Latch id_ex;
extern EX_MEM_Latch ex_mem;
extern MEM_WB_Latch mem_wb;

// Utility functions
void initialize_cpu();
void print_cpu_state();

#endif // CPU_H