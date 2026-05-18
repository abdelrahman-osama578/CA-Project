#include <stdio.h>
#include <stdbool.h>
#include "cpu.h"
#include "pipeline.h"

// --- GUI Tracker Variables (Guarantees perfect visual sync) ---
bool gui_if_active = false;  uint32_t gui_if_pc = 0;  uint32_t gui_if_inst = 0;
bool gui_id_active = false;  uint32_t gui_id_pc = 0;
bool gui_ex_active = false;  
bool gui_mem_active = false; 
bool gui_wb_active = false;  

bool branch_just_taken = false; // Handles the 1-cycle fetch delay after a branch

void flush_pipeline() {
    if_id.is_valid = false;
    id_ex.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 5: WRITE BACK (WB)
// -----------------------------------------------------------------------------
void writeback() {
    printf("[WriteBk] ");
    gui_wb_active = false;

    if (!mem_wb.is_valid) {
        printf("Empty\n");
        return;
    }

    gui_wb_active = true;
    printf("Input: Opcode %d, Dest R%d, ALU_Res: %d, Mem_Read: %d | ", 
           mem_wb.opcode, mem_wb.dest_reg, mem_wb.alu_result, mem_wb.mem_read_val);

    if (mem_wb.dest_reg != 0) { 
        if (mem_wb.opcode == OPCODE_LW) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.mem_read_val;
            printf("Output: (Changed) Register R%d changed to %d\n", mem_wb.dest_reg, mem_wb.mem_read_val);
        } else if (mem_wb.opcode != OPCODE_SW && mem_wb.opcode != OPCODE_BNE && mem_wb.opcode != OPCODE_J) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.alu_result;
            printf("Output: (Changed) Register R%d changed to %d\n", mem_wb.dest_reg, mem_wb.alu_result);
        } else {
            printf("Output: No register written\n");
        }
    } else {
        printf("Output: Ignored write to R0\n");
    }
    
    mem_wb.is_valid = false; 
}

// -----------------------------------------------------------------------------
// STAGE 4: MEMORY (MEM)
// -----------------------------------------------------------------------------
void memory() {
    printf("[Memory]  ");
    gui_mem_active = false;

    if (!ex_mem.is_valid) {
        printf("Empty\n");
        mem_wb.is_valid = false; 
        return;
    }

    gui_mem_active = true;
    printf("Input: Opcode %d, Dest R%d, ALU_Res: %d, R1_val: %d | ", 
           ex_mem.opcode, ex_mem.dest_reg, ex_mem.alu_result, ex_mem.r1_val);

    mem_wb.is_valid = true;
    mem_wb.opcode = ex_mem.opcode;
    mem_wb.dest_reg = ex_mem.dest_reg;
    mem_wb.alu_result = ex_mem.alu_result;

    if (ex_mem.opcode == OPCODE_LW) {
        if (ex_mem.alu_result >= 1024 && ex_mem.alu_result < 2048) {
            mem_wb.mem_read_val = cpu.memory[ex_mem.alu_result]; 
            printf("Output: Read %d from Mem[%d]\n", mem_wb.mem_read_val, ex_mem.alu_result);
        } else {
            printf("Output: SEG FAULT\n");
            mem_wb.mem_read_val = 0;
        }
    } 
    else if (ex_mem.opcode == OPCODE_SW) {
        if (ex_mem.alu_result >= 1024 && ex_mem.alu_result < 2048) {
            cpu.memory[ex_mem.alu_result] = ex_mem.r1_val;
            printf("Output: (Changed) Mem[%d] changed to %d\n", ex_mem.alu_result, ex_mem.r1_val);
        } else {
            printf("Output: SEG FAULT\n");
        }
    } else {
        printf("Output: Passed Through\n");
    }

    ex_mem.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 3: EXECUTE (EX)
// -----------------------------------------------------------------------------
void execute() {
    printf("[Execute] ");
    gui_ex_active = false;

    if (!id_ex.is_valid) {
        printf("Empty\n");
        return;
    }

    gui_ex_active = true;
    if (id_ex.execute_cycle_count == 0) {
        id_ex.execute_cycle_count = 1;
        printf("Input: Opcode %d | Output: Executing (Cycle 1 of 2)\n", id_ex.opcode);
        return; 
    }

    printf("Input: Opcode %d, R1_val: %d, R2_val: %d, Imm: %d | ", id_ex.opcode, id_ex.r1_val, id_ex.r2_val, id_ex.immediate);

    ex_mem.is_valid = true;
    ex_mem.opcode = id_ex.opcode;
    ex_mem.dest_reg = (id_ex.opcode == OPCODE_ADD || id_ex.opcode == OPCODE_SUB) ? id_ex.r3_addr : id_ex.r1_addr;
    ex_mem.r1_val = id_ex.r1_val; 

    switch (id_ex.opcode) {
        case OPCODE_ADD:  ex_mem.alu_result = id_ex.r1_val + id_ex.r2_val; break;
        case OPCODE_SUB:  ex_mem.alu_result = id_ex.r1_val - id_ex.r2_val; break;
        case OPCODE_MULI: ex_mem.alu_result = id_ex.r2_val * id_ex.immediate; break;
        case OPCODE_ADDI: ex_mem.alu_result = id_ex.r2_val + id_ex.immediate; break;
        case OPCODE_ANDI: ex_mem.alu_result = id_ex.r2_val & id_ex.immediate; break;
        case OPCODE_XORI: ex_mem.alu_result = id_ex.r2_val ^ id_ex.immediate; break;
        case OPCODE_SLL:  ex_mem.alu_result = id_ex.r2_val << id_ex.shamt; break;
        case OPCODE_SRL:  ex_mem.alu_result = (int32_t)((uint32_t)id_ex.r2_val >> id_ex.shamt); break;
        case OPCODE_LW:
        case OPCODE_SW:   ex_mem.alu_result = id_ex.r2_val + id_ex.immediate; break;
        case OPCODE_BNE:
            if (id_ex.r1_val != id_ex.r2_val) {
                cpu.pc = id_ex.pc + 1 + id_ex.immediate; 
                flush_pipeline();
                branch_just_taken = true; // Trigger fetch delay
                printf("Output: Branch Taken! Flushed pipeline, new PC: %d\n", cpu.pc);
                id_ex.is_valid = false;
                return;
            }
            break;
        case OPCODE_J:
            cpu.pc = (id_ex.pc & 0xF0000000) | id_ex.address;
            flush_pipeline();
            branch_just_taken = true; // Trigger fetch delay
            printf("Output: Jump Taken! Flushed pipeline, new PC: %d\n", cpu.pc);
            id_ex.is_valid = false;
            return;
        default:
            ex_mem.alu_result = 0; break;
    }

    printf("Output: ALU calculated %d, Dest R%d\n", ex_mem.alu_result, ex_mem.dest_reg);
    id_ex.is_valid = false; 
}

// -----------------------------------------------------------------------------
// STAGE 2: INSTRUCTION DECODE (ID)
// -----------------------------------------------------------------------------
void decode() {
    printf("[Decode]  ");
    gui_id_active = false;

    if (!if_id.is_valid) {
        printf("Empty\n");
        return;
    }

    gui_id_active = true;
    gui_id_pc = if_id.pc;

    if (if_id.decode_cycle_count == 0) {
        if_id.decode_cycle_count = 1;
        printf("Input: Inst 0x%08X | Output: Decoding (Cycle 1 of 2)\n", if_id.instruction);
        return;
    }

    uint32_t inst = if_id.instruction;
    uint8_t op    = (inst >> 28) & 0xF;
    uint8_t r1    = (inst >> 23) & 0x1F;
    uint8_t r2    = (inst >> 18) & 0x1F;
    uint8_t r3    = (inst >> 13) & 0x1F;
    
    // Load-Use Hazard check against what EX just outputted
    if (ex_mem.is_valid && ex_mem.opcode == OPCODE_LW) {
        if (ex_mem.dest_reg == r1 || ex_mem.dest_reg == r2) {
            printf("Stalled (Load-Use Hazard Detected)\n");
            return; // We wait in Cycle 1 state without emptying the latch
        }
    }

    printf("Input: Inst 0x%08X | ", inst);

    id_ex.is_valid = true;
    id_ex.pc = if_id.pc;
    id_ex.execute_cycle_count = 0; 
    id_ex.opcode    = op;
    id_ex.r1_addr   = r1;
    id_ex.r2_addr   = r2;
    id_ex.r3_addr   = r3;
    id_ex.shamt     = inst & 0x1FFF;
    
    int32_t imm = inst & 0x3FFFF;
    if (imm & 0x20000) imm |= 0xFFFC0000; 
    id_ex.immediate = imm;
    id_ex.address = inst & 0xFFFFFFF;

    id_ex.r1_val = cpu.registers[id_ex.r1_addr];
    id_ex.r2_val = cpu.registers[id_ex.r2_addr];

    // Forwarding Logic
    if (mem_wb.is_valid && mem_wb.dest_reg != 0) {
        int32_t wb_val = (mem_wb.opcode == OPCODE_LW) ? mem_wb.mem_read_val : mem_wb.alu_result;
        if (mem_wb.dest_reg == id_ex.r1_addr) id_ex.r1_val = wb_val;
        if (mem_wb.dest_reg == id_ex.r2_addr) id_ex.r2_val = wb_val;
    }
    if (ex_mem.is_valid && ex_mem.dest_reg != 0) {
        if (ex_mem.dest_reg == id_ex.r1_addr) id_ex.r1_val = ex_mem.alu_result;
        if (ex_mem.dest_reg == id_ex.r2_addr) id_ex.r2_val = ex_mem.alu_result;
    }

    printf("Output: Forwarded/Read Values -> R1_val: %d, R2_val: %d\n", id_ex.r1_val, id_ex.r2_val);
    if_id.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 1: INSTRUCTION FETCH (IF)
// -----------------------------------------------------------------------------
void fetch() {
    printf("[Fetch]   ");
    gui_if_active = false;
    
    if (branch_just_taken) {
        branch_just_taken = false;
        printf("Idle (Branch Delay required by rubric)\n");
        return; 
    }

    // Natural Latch Rhythm check - IF only fetches if the latch is empty
    if (if_id.is_valid) {
        printf("Idle (Wait for decode)\n");
        return; 
    }

    extern int num_parsed_instructions;
    if (cpu.pc < INSTRUCTION_LIMIT && cpu.pc < num_parsed_instructions) {
        if_id.is_valid = true;
        if_id.instruction = cpu.memory[cpu.pc];
        if_id.pc = cpu.pc;
        if_id.decode_cycle_count = 0; 
        
        gui_if_active = true;
        gui_if_pc = cpu.pc;
        gui_if_inst = if_id.instruction;

        printf("Input: PC %d | Output: Fetched Instruction 0x%08X\n", cpu.pc, if_id.instruction);
        cpu.pc++;
    } else {
        printf("Empty\n");
    }
}