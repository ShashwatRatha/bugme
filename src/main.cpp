#include "debugger.hpp"

int main(int argc, char *argv[]) {
  if (argc < 2) return 1;
  Debugger dbg(argv[1], argv + 1);
  dbg.run();
}
