#include "ptraceWrappers.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

// Attach to a running process
long ptAttach(pid_t pid) { return ptrace(PTRACE_ATTACH, pid, NULL, NULL); }

// Detach from process
long ptDetach(pid_t pid) { return ptrace(PTRACE_DETACH, pid, NULL, NULL); }

// Continue execution
long ptContinue(pid_t pid, int signal) {
  return ptrace(PTRACE_CONT, pid, NULL, (void *)(long)signal);
}

// Single-step one instruction
long ptSingleStep(pid_t pid) {
  return ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
}

// Read all registers
long ptGetRegs(pid_t pid, struct user_regs_struct *regs) {
  return ptrace(PTRACE_GETREGS, pid, NULL, regs);
}

// Write all registers
long ptSetRegs(pid_t pid, struct user_regs_struct *regs) {
  return ptrace(PTRACE_SETREGS, pid, NULL, regs);
}

// Read a word from tracee memory
long ptReadMem(pid_t pid, uint64_t addr) {
  errno = 0;
  long word = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
  if (errno) {
    perror("ptrace");
    return -1;
  }
  return word;
}

// Write a word to tracee memory
long ptWriteMem(pid_t pid, uint64_t addr, long data) {
  return ptrace(PTRACE_POKEDATA, pid, (void *)addr, (void *)data);
}

// Fork and exec a new traced process
pid_t ptSpawn(const char *program, char *const argv[]) {
  pid_t pid = fork();

  if (pid == 0) {
    // in child
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    execvp(program, argv);
    perror("execve");
    exit(1);
  }
  // in parent
  int status;
  waitpid(pid, &status, 0);
  return pid;
}
