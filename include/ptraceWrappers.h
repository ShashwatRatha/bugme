#ifndef PT_WRAPPERS_H
#define PT_WRAPPERS_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Launches a new process with tracing enabled.
 * * Invokes fork(). The child executes `ptrace(PTRACE_TRACEME, ...)` and
 * suspends itself via `raise(SIGSTOP)` before calling `execvp` to hand control
 * back to the parent debugger loop.
 * * @param program Path targeting the binary program executable location.
 * @param argv Null-terminated list of strings forwarded as runtime
 * configurations.
 * @return The pid identifier of the initialized child process, or negative on
 * failures.
 */
pid_t ptSpawn(const char *program, char *const argv[]);

/**
 * @brief Attaches to a running target process via ptrace hooks.
 * @param pid Process ID of target process.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptAttach(pid_t pid);

/**
 * @brief Detaches from a currently traced target process.
 * @param pid Process ID of target process.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptDetach(pid_t pid);

/**
 * @brief Resumes the tracee execution engine context.
 * @param pid Process ID of target process.
 * @param signal System signal payload choice to deliver to the target process
 * thread.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptContinue(pid_t pid, int signal);

/**
 * @brief Advances execution by a single CPU instruction step boundary.
 * @param pid Process ID of target process.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptSingleStep(pid_t pid);

/**
 * @brief Pulls user architectural register contexts out from active CPU maps.
 * @param pid Process ID of target process.
 * @param regs Struct location pointer assigned to hold incoming hardware state
 * layouts.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptGetRegs(pid_t pid, struct user_regs_struct *regs);

/**
 * @brief Overwrites active CPU architectural register maps with new layout
 * configurations.
 * @param pid Process ID of target process.
 * @param regs Struct pointer enclosing custom values to apply onto hardware
 * registers.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptSetRegs(pid_t pid, struct user_regs_struct *regs);

/**
 * @brief Reads a single 64-bit data word from a given virtual address inside
 * the tracee's space.
 * @param pid Process ID of target process.
 * @param addr Target memory virtual address tracking the target read offset.
 * @return Raw numeric payload contents read out from the memory location.
 */
long ptReadMem(pid_t pid, uint64_t addr);

/**
 * @brief Writes a single 64-bit word onto a target virtual address layer inside
 * the tracee's space.
 * @param pid Process ID of target process.
 * @param addr Target memory virtual address tracking the target write location.
 * @param data Data payload block value to insert.
 * @return Return value tracking active syscall errors or operational feedback.
 */
long ptWriteMem(pid_t pid, uint64_t addr, long data);

#ifdef __cplusplus
}
#endif

#endif
