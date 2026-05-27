#ifndef PT_WRAPPERS_H
#define PT_WRAPPERS_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>

#ifdef __cplusplus
extern "C" {
#endif

// Launch a new traceable process
pid_t ptSpawn(const char *program, char *const argv[]);

// Attach to a running process
long ptAttach(pid_t pid);
// Detach from an attached process
long ptDetach(pid_t pid);
// Continue the process
long ptContinue(pid_t pid, int signal);
// Single-step through the process
long ptSingleStep(pid_t pid);
// Get registers of the process
long ptGetRegs(pid_t pid, struct user_regs_struct *regs);
// Set the values in registers of the process
long ptSetRegs(pid_t pid, struct user_regs_struct *regs);
// Read the word at offset 'addr' in the process
long ptReadMem(pid_t pid, uint64_t addr);
// Write a word 'data' at offset 'addr' in process
long ptWriteMem(pid_t pid, uint64_t addr, long data);

#ifdef __cplusplus
}
#endif

#endif
