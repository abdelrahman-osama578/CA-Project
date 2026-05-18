

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "parser.h"
#include "pipeline.h"

CPU cpu;
IF_ID_Latch if_id;
ID_EX_Latch id_ex;
EX_MEM_Latch ex_mem;
MEM_WB_Latch mem_wb;

// Global flags and trackers
bool mem_active_this_cycle = false;
int num_parsed_instructions = 0; 

void initialize_cpu() {
    memset(&cpu, 0, sizeof(CPU));
    memset(&if_id, 0, sizeof(IF_ID_Latch));
    memset(&id_ex, 0, sizeof(ID_EX_Latch));
    memset(&ex_mem, 0, sizeof(EX_MEM_Latch));
    memset(&mem_wb, 0, sizeof(MEM_WB_Latch));
    cpu.clock_cycle = 1;
}

void print_final_state() {
    printf("\n===================================================\n");
    printf("FINAL CPU STATE\n");
    printf("===================================================\n");
    for (int i = 0; i < 32; i++) {
        printf("R%d: %d\n", i, cpu.registers[i]);
    }
    printf("PC: %d\n\n", cpu.pc);
    
    printf("--- Instruction Memory (0 to 1023) ---\n");
    for (int i = 0; i < 1024; i++) {
        if (cpu.memory[i] != 0) {
            printf("Mem[%d]: 0x%08X\n", i, cpu.memory[i]);
        }
    }
    
    printf("\n--- Data Memory (1024 to 2047) ---\n");
    for (int i = 1024; i < 2048; i++) {
        if (cpu.memory[i] != 0) {
            printf("Mem[%d]: %d\n", i, cpu.memory[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <program.txt>\n", argv[0]);
        return 1;
    }

    initialize_cpu();
    load_program(argv[1]);

    printf("Program loaded successfully. Beginning simulation...\n");

    while (1) {
        // Safety net: stop after 200 cycles to prevent infinite loops
        if (cpu.clock_cycle > 200) {
            printf("\n--- Reached 200 cycle limit (Safety Net) ---\n");
            break;
        }

        printf("\n===================================================\n");
        printf("Clock Cycle: %d\n", cpu.clock_cycle);
        printf("===================================================\n");

        // RESET THE FLAG AT THE START OF EVERY CYCLE
        mem_active_this_cycle = false;

        // Execute stages in reverse order to simulate latches
        writeback();
        memory();
        execute();
        decode();
        fetch();

        // Stop condition: all latches empty and PC has exceeded loaded instructions
        if (!if_id.is_valid && !id_ex.is_valid && !ex_mem.is_valid && !mem_wb.is_valid && cpu.pc >= num_parsed_instructions) {
            break;
        }

        cpu.clock_cycle++;
    }

    print_final_state();
    return 0;
}