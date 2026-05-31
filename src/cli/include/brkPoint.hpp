#ifndef BRKPOINT_HPP_
#define BRKPOINT_HPP_

#include <sys/types.h>

#include <cstdint>

class BreakPoint {
 public:
  // constructors and destructor
  BreakPoint(pid_t pid, std::uint64_t addr);
  BreakPoint() = delete;
  BreakPoint(const BreakPoint&) = delete;
  BreakPoint& operator=(const BreakPoint&) = delete;
  BreakPoint(BreakPoint&&) =
      default;  // unordered_map required MoveConstructible keys and values
  BreakPoint& operator=(BreakPoint&&) = delete;
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
