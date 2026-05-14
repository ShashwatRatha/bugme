#ifndef BRKPOINT_H_
#define BRKPOINT_H_

#include <sys/types.h>

#include <cstdint>

class BreakPoint {
 public:
  // constructors and destructor
  BreakPoint(pid_t pid, std::uint64_t addr);
  BreakPoint(const BreakPoint&) = default;
  BreakPoint& operator=(const BreakPoint&) = default;
  BreakPoint(BreakPoint&&) = default;
  BreakPoint& operator=(BreakPoint&&) = default;
  ~BreakPoint() = default;

  // getters
  bool isEnabled() const;
  std::uint64_t getAddr() const;

  // methods
  void enableBP();
  void disableBP();

 private:
  pid_t mPid = -1;
  bool mEnabled = false;
  std::uint64_t mAddr = 0;
  std::uint8_t mReadByte = 0;
};

#endif
