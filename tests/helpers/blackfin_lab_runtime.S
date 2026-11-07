.text
.global _start
_start:
  SP.H = 0x01f0;
  SP.L = 0;
  FP = SP;
  R0 = 0;
  L0 = R0; L1 = R0; L2 = R0; L3 = R0;
  CALL _lab_main;
  SP += -16;
  [SP] = R0;
  R0 = SP;
  P0 = 1;
  EXCPT 0;
  ABORT;
.global _lab_counters
_lab_counters:
  P0 = R0;
  R0 = CYCLES;
  [P0] = R0;
  [P0 + 4] = R1;
  [P0 + 8] = R2;
  RTS;
.global _lab_write
_lab_write:
  SP += -16;
  R2 = 1;
  [SP] = R2;
  [SP + 4] = R0;
  [SP + 8] = R1;
  R0 = SP;
  P0 = 5;
  EXCPT 0;
  SP += 16;
  RTS;
