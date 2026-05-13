#include <stdio.h>
#include "pipeline.h"

// -----------------------------------------------------------------------------
// STAGE 5: WRITE BACK (WB)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// STAGE 5: WRITE BACK (WB)
// -----------------------------------------------------------------------------
void writeback() {
    if (!mem_wb.is_valid) return; // Stage is empty

    if (mem_wb.dest_reg != 0) { // R0 is hardwired to 0
        if (mem_wb.opcode == OPCODE_LW) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.mem_read_val;
        } else if (mem_wb.opcode != OPCODE_SW && mem_wb.opcode != OPCODE_BNE && mem_wb.opcode != OPCODE_J) {
            cpu.registers[mem_wb.dest_reg] = mem_wb.alu_result;
        }
    }
    
    // The instruction has completely finished executing. Clear the latch!
    mem_wb.is_valid = false; 
}

// -----------------------------------------------------------------------------
// STAGE 4: MEMORY (MEM)
// -----------------------------------------------------------------------------
void memory() {
    if (!ex_mem.is_valid) {
        mem_wb.is_valid = false; // Pass the empty "bubble" forward
        return;
    }

    mem_wb.is_valid = true;
    mem_wb.opcode = ex_mem.opcode;
    mem_wb.dest_reg = ex_mem.dest_reg;
    mem_wb.alu_result = ex_mem.alu_result;

    if (ex_mem.opcode == OPCODE_LW) {
        mem_wb.mem_read_val = cpu.memory[ex_mem.alu_result]; 
    } 
    else if (ex_mem.opcode == OPCODE_SW) {
        cpu.memory[ex_mem.alu_result] = ex_mem.r1_val;
    }

    // The instruction has moved to the MEM_WB latch. Clear this latch!
    ex_mem.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 3: EXECUTE (EX)
// -----------------------------------------------------------------------------
void execute() {
    if (!id_ex.is_valid) {
        ex_mem.is_valid = false;  // Pass the empty "bubble" forward
        return;
    }

    if (id_ex.execute_cycle_count == 1) {
        id_ex.execute_cycle_count++;
        ex_mem.is_valid = false; // Nothing outputted to memory yet
        return; // Wait for the second cycle [cite: 89, 515-516]
    }

    // --- SECOND CYCLE OF EXECUTION ---
    ex_mem.is_valid = true;
    ex_mem.opcode = id_ex.opcode;
    
    if (id_ex.opcode == OPCODE_ADD || id_ex.opcode == OPCODE_SUB || 
        id_ex.opcode == OPCODE_SLL || id_ex.opcode == OPCODE_SRL) {
        ex_mem.dest_reg = id_ex.r3_addr;
    } else {
        ex_mem.dest_reg = id_ex.r1_addr;
    }

    ex_mem.r1_val = id_ex.r1_val; 

    // Perform ALU Operation based on Opcode
    switch (id_ex.opcode) {
        case OPCODE_ADD:
            ex_mem.alu_result = id_ex.r1_val + id_ex.r2_val;
            break;
        case OPCODE_SUB:
            ex_mem.alu_result = id_ex.r1_val - id_ex.r2_val;
            break;
        case OPCODE_MULI:
            ex_mem.alu_result = id_ex.r2_val * id_ex.immediate;
            break;
        case OPCODE_ADDI:
            ex_mem.alu_result = id_ex.r2_val + id_ex.immediate;
            break;
        case OPCODE_ANDI:
            ex_mem.alu_result = id_ex.r2_val & id_ex.immediate;
            break;
        case OPCODE_XORI:
            ex_mem.alu_result = id_ex.r2_val ^ id_ex.immediate;
            break;
        case OPCODE_SLL:
            // Shift Left Logical
            ex_mem.alu_result = id_ex.r2_val << id_ex.shamt;
            break;
        case OPCODE_SRL:
            // Shift Right Logical (Assuming unsigned shift. If signed in C, might need cast to uint32_t)
            ex_mem.alu_result = (int32_t)((uint32_t)id_ex.r2_val >> id_ex.shamt);
            break;
        case OPCODE_LW:
        case OPCODE_SW:
            // Calculate memory address: R2 + IMM
            ex_mem.alu_result = id_ex.r2_val + id_ex.immediate;
            break;
        case OPCODE_BNE:
            // Branch if Not Equal
            if (id_ex.r1_val != id_ex.r2_val) {
                // Update PC and flush the pipeline [cite: 484-485]
                cpu.pc = id_ex.pc + 1 + id_ex.immediate; 
                flush_pipeline();
            }
            break;
        case OPCODE_J:
            // Unconditional Jump
            // Jump Address = PC[31:28] || ADDRESS
            cpu.pc = (id_ex.pc & 0xF0000000) | id_ex.address;
            flush_pipeline();
            break;
        default:
            // Unrecognized opcode (or bubble)
            ex_mem.alu_result = 0;
            break;
    }

    // The instruction has moved to the EX_MEM latch. Clear this latch!
    id_ex.is_valid = false; 
}

// -----------------------------------------------------------------------------
// STAGE 2: INSTRUCTION DECODE (ID)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// STAGE 2: INSTRUCTION DECODE (ID)
// -----------------------------------------------------------------------------
void decode() {
    if (!if_id.is_valid) return;
    
    if (id_ex.is_valid) return; // Stall if EX is busy doing its 2-cycle execution

    uint32_t inst = if_id.instruction;
    
    // Decode ALL POSSIBLE FORMATS simultaneously
    uint8_t op    = (inst >> 28) & 0xF;
    uint8_t r1    = (inst >> 23) & 0x1F;
    uint8_t r2    = (inst >> 18) & 0x1F;
    uint8_t r3    = (inst >> 13) & 0x1F;
    
    // --- 1. STALLING LOGIC (Load-Use Hazard) ---
    // If the instruction currently in the Execute stage is a Load Word (LW)...
    if (ex_mem.is_valid && ex_mem.opcode == OPCODE_LW) {
        // ...and its destination matches a source register of the instruction we are decoding
        if (ex_mem.dest_reg == r1 || ex_mem.dest_reg == r2) {
            // STALL! Do not move instruction from IF to ID. Do not update PC.
            // Just return and let the LW move into the MEM stage next cycle.
            return; 
        }
    }
    // Note: Since Package 1 executes in 2 cycles, the instruction in id_ex is technically 
    // also "in execute". If id_ex holds a LW, we are already stalling because id_ex.is_valid 
    // is true (handled at the top of this function).

    // --- MOVE DATA TO ID_EX LATCH ---
    id_ex.is_valid = true;
    id_ex.pc = if_id.pc;
    id_ex.execute_cycle_count = 1; 

    id_ex.opcode    = op;
    id_ex.r1_addr   = r1;
    id_ex.r2_addr   = r2;
    id_ex.r3_addr   = r3;
    id_ex.shamt     = inst & 0x1FFF;
    
    // Sign extend the 18-bit immediate
    int32_t imm = inst & 0x3FFFF;
    if (imm & 0x20000) imm |= 0xFFFC0000; 
    id_ex.immediate = imm;
    id_ex.address = inst & 0xFFFFFFF;

    // --- 2. DEFAULT REGISTER READ ---
    id_ex.r1_val = cpu.registers[id_ex.r1_addr];
    id_ex.r2_val = cpu.registers[id_ex.r2_addr];

    // --- 3. FORWARDING LOGIC (Data Bypassing) ---
    // A. Forward from the EX/MEM Latch (The instruction right in front of us)
    if (ex_mem.is_valid && ex_mem.dest_reg != 0) {
        if (ex_mem.dest_reg == id_ex.r1_addr) id_ex.r1_val = ex_mem.alu_result;
        if (ex_mem.dest_reg == id_ex.r2_addr) id_ex.r2_val = ex_mem.alu_result;
    }

    // B. Forward from the MEM/WB Latch (The instruction two steps ahead)
    if (mem_wb.is_valid && mem_wb.dest_reg != 0) {
        // Determine what value to forward (LW reads from memory, others use ALU result)
        int32_t wb_val = (mem_wb.opcode == OPCODE_LW) ? mem_wb.mem_read_val : mem_wb.alu_result;
        
        // Only forward from MEM/WB if EX/MEM didn't ALREADY forward a newer value
        if (mem_wb.dest_reg == id_ex.r1_addr && !(ex_mem.is_valid && ex_mem.dest_reg == id_ex.r1_addr)) {
            id_ex.r1_val = wb_val;
        }
        if (mem_wb.dest_reg == id_ex.r2_addr && !(ex_mem.is_valid && ex_mem.dest_reg == id_ex.r2_addr)) {
            id_ex.r2_val = wb_val;
        }
    }

    // Successfully decoded, so we clear the fetch buffer to allow the next fetch
    if_id.is_valid = false;
}

// -----------------------------------------------------------------------------
// STAGE 1: INSTRUCTION FETCH (IF)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// STAGE 1: INSTRUCTION FETCH (IF)
// -----------------------------------------------------------------------------
void fetch() {
    // IF and MEM cannot run in parallel. 
    if (ex_mem.is_valid && (ex_mem.opcode == OPCODE_LW || ex_mem.opcode == OPCODE_SW)) {
        return; 
    }

    // Only fetch if the buffer is empty AND we haven't reached the end of the program.
    // We check if cpu.memory[cpu.pc] is 0 (assuming 0 is not a valid user instruction) 
    // to know when we have hit empty memory.
    if (!if_id.is_valid && cpu.pc < INSTRUCTION_LIMIT && cpu.memory[cpu.pc] != 0) {
        if_id.is_valid = true;
        if_id.instruction = cpu.memory[cpu.pc];
        if_id.pc = cpu.pc;
        cpu.pc++;
    }
}

// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------
void flush_pipeline() {
    // Used during BNE or J to drop instructions fetched erroneously [cite: 485, 509-511]
    if_id.is_valid = false;
    // Note: In a real architecture, you might also need to flush ID depending on exact cycle timing,
    // but clearing if_id ensures the next instructions are dropped.
}

void update_latches() {
    // Because we call stages in reverse order (WB -> MEM -> EX -> ID -> IF) in main.c,
    // the latch data naturally propagates without needing a massive update function here.
    // However, if you add complex stall logic, you manage state transitions here.
}