#ifndef PIPELINE_H
#define PIPELINE_H

#include "cpu.h"

// Core pipeline stages
void fetch();
void decode();
void execute();
void memory();
void writeback();

// Helper to push data from one stage's latch to the next at the end of a cycle
void update_latches(); 

// Helper to handle control hazards (flushing IF and ID)
void flush_pipeline();

#endif // PIPELINE_H