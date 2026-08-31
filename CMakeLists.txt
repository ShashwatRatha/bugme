cmake_minimum_required(VERSION 3.10)

project(BugMe VERSION 2.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

find_package(PkgConfig)
pkg_check_modules(LIBELF REQUIRED libelf)
pkg_check_modules(CAPSTONE REQUIRED capstone)

file(
  GLOB SOURCES
  "src/cli/src/*.c"
  "src/cli/src/*.cpp"
)

add_executable(
  bugme-cli
  ${SOURCES}
)

target_include_directories(
  bugme-cli PRIVATE
  src/cli/include
  ${LIBELF_INCLUDE_DIRS}
  ${CAPSTONE_INCLUDE_DIRS}
)
target_link_libraries(
  bugme-cli PRIVATE
  ${LIBELF_LIBRARIES}
  ${CAPSTONE_LIBRARIES}
)
