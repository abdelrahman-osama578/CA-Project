#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "cpu.h"
#include "parser.h"
#include "pipeline.h"

// 1. Global Instantiation of the CPU and Pipeline Latches
CPU cpu;
IF_ID_Latch  if_id;
ID_EX_Latch  id_ex;
EX_MEM_Latch ex_mem;
MEM_WB_Latch mem_wb;

// 2. Initialization Function
void initialize_cpu() {
    // Clear memory and registers
    memset(&cpu, 0, sizeof(CPU));
    memset(&if_id,  0, sizeof(IF_ID_Latch));
    memset(&id_ex,  0, sizeof(ID_EX_Latch));
    memset(&ex_mem, 0, sizeof(EX_MEM_Latch));
    memset(&mem_wb, 0, sizeof(MEM_WB_Latch));

    // The PC starts from 0 [cite: 384]
    cpu.pc = 0;
    cpu.clock_cycle = 1;
}

// 3. Printing Helper Functions
void print_cycle_stats() {
    printf("\n===================================================\n");
    printf("Clock Cycle: %d\n", cpu.clock_cycle); // [cite: 692]
    printf("===================================================\n");

    // 1. FETCH STAGE PRINT
    if (if_id.is_valid) {
        printf("[Fetch]  Instruction: 0x%08X fetched from PC: %d\n", if_id.instruction, if_id.pc);
    } else {
        printf("[Fetch]  Empty\n");
    }

    // 2. DECODE STAGE PRINT
    if (id_ex.is_valid) {
        printf("[Decode] Decoded PC: %d | Opcode: %d | R1: %d | R2: %d | R3: %d | Imm: %d\n", 
               id_ex.pc, id_ex.opcode, id_ex.r1_addr, id_ex.r2_addr, id_ex.r3_addr, id_ex.immediate);
        printf("         Read Values -> R1_val: %d, R2_val: %d\n", id_ex.r1_val, id_ex.r2_val);
    } else {
        printf("[Decode] Empty\n");
    }

    // 3. EXECUTE STAGE PRINT
    if (ex_mem.is_valid) {
        printf("[Execute] Opcode: %d | Dest Reg: %d | ALU Result: %d\n", 
               ex_mem.opcode, ex_mem.dest_reg, ex_mem.alu_result);
    } else {
        printf("[Execute] Empty\n");
    }

    // 4. MEMORY STAGE PRINT
    if (mem_wb.is_valid) {
        printf("[Memory]  Opcode: %d | Dest Reg: %d | ALU/Mem Result: %d\n", 
               mem_wb.opcode, mem_wb.dest_reg, 
               (mem_wb.opcode == OPCODE_LW) ? mem_wb.mem_read_val : mem_wb.alu_result);
    } else {
        printf("[Memory]  Empty\n");
    }

    // 5. WRITE BACK STAGE PRINT
    printf("[WriteBk] ");
    if (mem_wb.is_valid && mem_wb.dest_reg != 0) { // R0 is hard-wired to 0 [cite: 388-392]
        if (mem_wb.opcode == OPCODE_SW || mem_wb.opcode == OPCODE_BNE || mem_wb.opcode == OPCODE_J) {
            printf("No register written (Memory/Branch instruction)\n");
        } else {
            printf("Wrote value %d to Register R%d\n", 
                   (mem_wb.opcode == OPCODE_LW) ? mem_wb.mem_read_val : mem_wb.alu_result, 
                   mem_wb.dest_reg);
        }
    } else {
        printf("Empty or wrote to R0 (ignored)\n");
    }
}

void print_final_state() {
    printf("\n===================================================\n");
    printf("FINAL CPU STATE\n");
    printf("===================================================\n");

    // Print all registers [cite: 699]
    for (int i = 0; i < NUM_REGISTERS; i++) {
        printf("R%d: %d\n", i, cpu.registers[i]);
    }
    printf("PC: %d\n", cpu.pc);

    // Print instruction memory [cite: 700]
    printf("\n--- Instruction Memory (0 to 1023) ---\n");
    for (int i = 0; i < INSTRUCTION_LIMIT; i++) {
        if (cpu.memory[i] != 0) {
            printf("Mem[%d]: 0x%08X\n", i, cpu.memory[i]);
        }
    }

    // Print data memory [cite: 700]
    printf("\n--- Data Memory (1024 to 2047) ---\n");
    for (int i = INSTRUCTION_LIMIT; i < MEMORY_SIZE; i++) {
        if (cpu.memory[i] != 0) {
            printf("Mem[%d]: %d\n", i, cpu.memory[i]);
        }
    }
}

// Helper to check if any instructions are still in the pipeline
bool is_pipeline_active() {
    return if_id.is_valid || id_ex.is_valid || ex_mem.is_valid || mem_wb.is_valid;
}

// 4. The Main Loop
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <assembly_file.txt>\n", argv[0]);
        return 1;
    }

    // Initialize the processor state
    initialize_cpu();
    
    // Load instructions from the text file into memory
    load_program(argv[1]);

    printf("Program loaded successfully. Beginning simulation...\n");

    // The correct way of stopping is when there are no more instructions to fetch[cite: 417],
    // AND the pipeline has finished processing all currently loaded instructions.
    while ((cpu.pc < INSTRUCTION_LIMIT && cpu.memory[cpu.pc] != 0) || is_pipeline_active()) { 
        
        // Execute stages in REVERSE order to simulate parallel execution correctly
        writeback();
        memory();
        execute();
        decode();
        fetch();
        
        // Print the required outputs for this cycle
        print_cycle_stats();

        // Increment the clock cycle [cite: 689]
        cpu.clock_cycle++;

        // Safety net: stop after 100 cycles
        if (cpu.clock_cycle > 100) {
            printf("\n--- Reached 100 cycle limit (Safety Net) ---\n");
            break;
        }
    }

    // Print the final memory and register dumps after the last clock cycle [cite: 699-700]
    print_final_state();

    return 0;
}