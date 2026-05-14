#include <stdio.h>
#include <stdbool.h>
#include "cpu.h"
#include "pipeline.h"

void flush_pipeline() {
    if_id.is_valid = false;
    id_ex.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 5: WRITE BACK (WB)
// -----------------------------------------------------------------------------
void writeback() {
    printf("[WriteBk] ");
    if (!mem_wb.is_valid) {
        printf("Empty\n");
        return;
    }

    printf("Input: Opcode %d, Dest R%d, ALU_Res: %d, Mem_Read: %d | ", 
           mem_wb.opcode, mem_wb.dest_reg, mem_wb.alu_result, mem_wb.mem_read_val);

    if (mem_wb.dest_reg != 0) { 
        if (mem_wb.opcode == OPCODE_LW) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.mem_read_val;
            printf("Output: (Changed) Register R%d changed to %d in Write Back stage\n", mem_wb.dest_reg, mem_wb.mem_read_val);
        } else if (mem_wb.opcode != OPCODE_SW && mem_wb.opcode != OPCODE_BNE && mem_wb.opcode != OPCODE_J) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.alu_result;
            printf("Output: (Changed) Register R%d changed to %d in Write Back stage\n", mem_wb.dest_reg, mem_wb.alu_result);
        } else {
            printf("Output: No register written (Memory/Branch instruction)\n");
        }
    } else {
        printf("Output: Ignored write to hardwired R0\n");
    }
    
    mem_wb.is_valid = false; 
}

// -----------------------------------------------------------------------------
// STAGE 4: MEMORY (MEM)
// -----------------------------------------------------------------------------
void memory() {
    printf("[Memory]  ");
    if (!ex_mem.is_valid) {
        printf("Empty\n");
        mem_wb.is_valid = false; 
        return;
    }

    printf("Input: Opcode %d, Dest R%d, ALU_Res: %d, R1_val: %d | ", 
           ex_mem.opcode, ex_mem.dest_reg, ex_mem.alu_result, ex_mem.r1_val);

    mem_wb.is_valid = true;
    mem_wb.opcode = ex_mem.opcode;
    mem_wb.dest_reg = ex_mem.dest_reg;
    mem_wb.alu_result = ex_mem.alu_result;

    if (ex_mem.opcode == OPCODE_LW) {
        mem_wb.mem_read_val = cpu.memory[ex_mem.alu_result]; 
        printf("Output: Read %d from Mem[%d]\n", mem_wb.mem_read_val, ex_mem.alu_result);
    } 
    else if (ex_mem.opcode == OPCODE_SW) {
        cpu.memory[ex_mem.alu_result] = ex_mem.r1_val;
        printf("Output: (Changed) Memory location Mem[%d] changed to %d in Memory stage\n", ex_mem.alu_result, ex_mem.r1_val);
    } else {
        printf("Output: Passed ALU result %d forward\n", ex_mem.alu_result);
    }

    ex_mem.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 3: EXECUTE (EX)
// -----------------------------------------------------------------------------
void execute() {
    printf("[Execute] ");
    if (!id_ex.is_valid) {
        printf("Empty\n");
        ex_mem.is_valid = false;  
        return;
    }

    if (id_ex.execute_cycle_count == 1) {
        id_ex.execute_cycle_count++;
        ex_mem.is_valid = false; 
        printf("Input: Opcode %d | Output: Executing (Cycle 1 of 2)\n", id_ex.opcode);
        return; 
    }

    printf("Input: Opcode %d, R1_val: %d, R2_val: %d, Imm: %d | ", id_ex.opcode, id_ex.r1_val, id_ex.r2_val, id_ex.immediate);

    ex_mem.is_valid = true;
    ex_mem.opcode = id_ex.opcode;
    
    // Corrected Destination Logic (SLL/SRL use R1 as destination in Package 1)
    if (id_ex.opcode == OPCODE_ADD || id_ex.opcode == OPCODE_SUB) {
        ex_mem.dest_reg = id_ex.r3_addr;
    } else {
        ex_mem.dest_reg = id_ex.r1_addr;
    }

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
                printf("Output: Branch Taken! Flushed pipeline, new PC: %d\n", cpu.pc);
                id_ex.is_valid = false;
                return;
            }
            break;
        case OPCODE_J:
            cpu.pc = (id_ex.pc & 0xF0000000) | id_ex.address;
            flush_pipeline();
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
    if (!if_id.is_valid) {
        printf("Empty\n");
        return;
    }
    if (id_ex.is_valid) {
        printf("Stalled (EX is busy)\n");
        return; 
    }

    if (if_id.decode_cycle_count == 1) {
        if_id.decode_cycle_count++;
        printf("Input: Inst 0x%08X | Output: Decoding (Cycle 1 of 2)\n", if_id.instruction);
        return;
    }

    uint32_t inst = if_id.instruction;
    uint8_t op    = (inst >> 28) & 0xF;
    uint8_t r1    = (inst >> 23) & 0x1F;
    uint8_t r2    = (inst >> 18) & 0x1F;
    uint8_t r3    = (inst >> 13) & 0x1F;
    
    // Load-Use Hazard Stalling
    if (ex_mem.is_valid && ex_mem.opcode == OPCODE_LW) {
        if (ex_mem.dest_reg == r1 || ex_mem.dest_reg == r2) {
            printf("Stalled (Load-Use Hazard Detected)\n");
            return; 
        }
    }

    printf("Input: Inst 0x%08X (Op:%d, R1:%d, R2:%d) | ", inst, op, r1, r2);

    id_ex.is_valid = true;
    id_ex.pc = if_id.pc;
    id_ex.execute_cycle_count = 1; 
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
    if (ex_mem.is_valid && ex_mem.dest_reg != 0) {
        if (ex_mem.dest_reg == id_ex.r1_addr) id_ex.r1_val = ex_mem.alu_result;
        if (ex_mem.dest_reg == id_ex.r2_addr) id_ex.r2_val = ex_mem.alu_result;
    }

    if (mem_wb.is_valid && mem_wb.dest_reg != 0) {
        int32_t wb_val = (mem_wb.opcode == OPCODE_LW) ? mem_wb.mem_read_val : mem_wb.alu_result;
        if (mem_wb.dest_reg == id_ex.r1_addr && !(ex_mem.is_valid && ex_mem.dest_reg == id_ex.r1_addr)) {
            id_ex.r1_val = wb_val;
        }
        if (mem_wb.dest_reg == id_ex.r2_addr && !(ex_mem.is_valid && ex_mem.dest_reg == id_ex.r2_addr)) {
            id_ex.r2_val = wb_val;
        }
    }

    printf("Output: Forwarded/Read Values -> R1_val: %d, R2_val: %d\n", id_ex.r1_val, id_ex.r2_val);
    if_id.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 1: INSTRUCTION FETCH (IF)
// -----------------------------------------------------------------------------
void fetch() {
    printf("[Fetch]   ");
    
    // New Package 1 Rule: Only fetch on ODD clock cycles
    if (cpu.clock_cycle % 2 == 0) {
        printf("Idle (Even Clock Cycle)\n");
        return;
    }

    if (ex_mem.is_valid && (ex_mem.opcode == OPCODE_LW || ex_mem.opcode == OPCODE_SW)) {
        printf("Stalled (Memory structural hazard)\n");
        return; 
    }

    if (!if_id.is_valid && cpu.pc < INSTRUCTION_LIMIT && cpu.memory[cpu.pc] != 0) {
        if_id.is_valid = true;
        if_id.instruction = cpu.memory[cpu.pc];
        if_id.pc = cpu.pc;
        if_id.decode_cycle_count = 1; // Start the 2-cycle decode timer
        
        printf("Input: PC %d | Output: Fetched Instruction 0x%08X\n", cpu.pc, if_id.instruction);
        cpu.pc++;
    } else {
        printf("Empty (No valid instruction or buffer full)\n");
    }
}