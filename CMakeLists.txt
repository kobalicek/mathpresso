cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "mathpresso")
  project(mathpresso CXX)
endif()

include(GNUInstallDirs)

# MathPresso - Configuration
# ==========================

if (NOT DEFINED MATHPRESSO_DIR)
  set(MATHPRESSO_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

if (NOT DEFINED ASMJIT_DIR)
  set(ASMJIT_DIR "${MATHPRESSO_DIR}/../asmjit")
endif()

set(MATHPRESSO_DIR    "${MATHPRESSO_DIR}" CACHE PATH "Location of 'mathpresso'")
set(ASMJIT_DIR        "${ASMJIT_DIR}"     CACHE PATH "Location of 'asmjit'")

set(MATHPRESSO_TEST   FALSE               CACHE BOOL "Build 'mathpresso' test applications")
set(MATHPRESSO_EMBED  FALSE               CACHE BOOL "Embed 'mathpresso' library (no targets)")
set(MATHPRESSO_STATIC ${MATHPRESSO_EMBED} CACHE BOOL "Build 'mathpresso' library as static")

# MathPresso - Project
# ====================

set(MATHPRESSO_INCLUDE_DIRS ${MATHPRESSO_DIR}/src)
set(MATHPRESSO_DEPS "")
set(MATHPRESSO_LIBS "")
set(MATHPRESSO_CFLAGS "")
set(MATHPRESSO_PRIVATE_CFLAGS "")
set(MATHPRESSO_PRIVATE_CFLAGS_DBG "")
set(MATHPRESSO_PRIVATE_CFLAGS_REL "")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  set(MATHPRESSO_PRIVATE_LFLAGS "/OPT:REF /OPT:ICF")

  list(APPEND MATHPRESSO_PRIVATE_CFLAGS -MP)        # [+] Multi-process compilation
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS -GF)        # [+] Eliminate duplicate strings.
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS -GR-)       # [-] Runtime type information.
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_DBG -GS)    # [+] Buffer security-check.
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_REL -GS-)   # [-] Buffer security-check.
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_REL -O2)    # [+] Favor speed over size.
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS -Wall -Wextra)
endif()

if (NOT DEFINED ASMJIT_EMBED)
  set(ASMJIT_EMBED TRUE)
endif()
include("${ASMJIT_DIR}/CMakeLists.txt")

list(APPEND MATHPRESSO_DEPS ${ASMJIT_LIBS})
list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${ASMJIT_CFLAGS}")

list(APPEND MATHPRESSO_LIBS ${MATHPRESSO_DEPS})
if (NOT MATHPRESSO_EMBED)
  list(INSERT MATHPRESSO_LIBS 0 mathpresso)
endif()

if (MATHPRESSO_EMBED)
  set(MATHPRESSO_TARGET_TYPE "EMBED")
elseif (MATHPRESSO_STATIC)
  set(MATHPRESSO_TARGET_TYPE "STATIC")
else()
  set(MATHPRESSO_TARGET_TYPE "SHARED")
endif()

if (MATHPRESSO_EMBED OR MATHPRESSO_STATIC)
  list(APPEND MATHPRESSO_CFLAGS -DMATHPRESSO_STATIC)
endif()

list(REMOVE_DUPLICATES MATHPRESSO_DEPS)
list(REMOVE_DUPLICATES MATHPRESSO_PRIVATE_CFLAGS)

# MathPresso - Sources
# ====================

set(MATHPRESSO_SRC_LIST
  mathpresso/mathpresso.cpp
  mathpresso/mathpresso.h
  mathpresso/mathpresso_p.h
  mathpresso/mpast.cpp
  mathpresso/mpast_p.h
  mathpresso/mpatomic_p.h
  mathpresso/mpcompiler.cpp
  mathpresso/mpcompiler_p.h
  mathpresso/mpeval_p.h
  mathpresso/mphash.cpp
  mathpresso/mphash_p.h
  mathpresso/mpoptimizer.cpp
  mathpresso/mpoptimizer_p.h
  mathpresso/mpparser.cpp
  mathpresso/mpparser_p.h
  mathpresso/mpstrtod_p.h
  mathpresso/mptokenizer.cpp
  mathpresso/mptokenizer_p.h
)

set(MATHPRESSO_SRC)
foreach(_src_file ${MATHPRESSO_SRC_LIST})
  list(APPEND MATHPRESSO_SRC src/${_src_file})
endforeach()
source_group(TREE "${MATHPRESSO_DIR}" FILES ${MATHPRESSO_SRC})

# MathPresso - Targets
# ====================

if (NOT MATHPRESSO_EMBED)
  add_library(mathpresso ${MATHPRESSO_TARGET_TYPE} ${MATHPRESSO_SRC} ${ASMJIT_SRC})
  target_link_libraries(mathpresso PRIVATE ${MATHPRESSO_DEPS})
  target_compile_options(mathpresso PUBLIC ${MATHPRESSO_CFLAGS})
  target_compile_options(mathpresso PRIVATE ${MATHPRESSO_PRIVATE_CFLAGS}
    $<$<CONFIG:Debug>:${MATHPRESSO_PRIVATE_CFLAGS_DBG}>
    $<$<NOT:$<CONFIG:Debug>>:${MATHPRESSO_PRIVATE_CFLAGS_REL}>)
  target_compile_features(mathpresso PUBLIC cxx_std_17)
  set_property(TARGET mathpresso PROPERTY CXX_VISIBILITY_PRESET hidden)

  target_include_directories(mathpresso BEFORE PRIVATE ${ASMJIT_INCLUDE_DIRS})
  # target_include_directories(mathpresso BEFORE PUBLIC ${MATHPRESSO_INCLUDE_DIRS})

  target_include_directories(mathpresso BEFORE INTERFACE
                             $<BUILD_INTERFACE:${MATHPRESSO_INCLUDE_DIRS}>
                             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


  add_library(mathpresso::mathpresso ALIAS mathpresso)

  foreach(_src_file ${MATHPRESSO_SRC_LIST})
    if ("${_src_file}" MATCHES "\\.h$" AND NOT "${_src_file}" MATCHES "_p\\.h$")
      get_filename_component(_src_dir ${_src_file} PATH)
      install(FILES "${MATHPRESSO_DIR}/src/${_src_file}" DESTINATION "include/${_src_dir}")
    endif()
  endforeach()

  install(TARGETS mathpresso
          EXPORT mathpresso-config
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
          ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

  install(EXPORT mathpresso-config
          NAMESPACE mathpresso::
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mathpresso")
endif()

if (MATHPRESSO_TEST)
  foreach(_target mpeval mptest mptutorial)
    add_executable(${_target} test/${_target}.cpp)
    target_link_libraries(${_target} ${MATHPRESSO_LIBS})
    target_compile_options(${_target} PRIVATE ${MATHPRESSO_PRIVATE_CFLAGS}
      $<$<CONFIG:Debug>:${MATHPRESSO_PRIVATE_CFLAGS_DBG}>
      $<$<NOT:$<CONFIG:Debug>>:${MATHPRESSO_PRIVATE_CFLAGS_REL}>)
    target_compile_features(${_target} PUBLIC cxx_std_17)
    set_property(TARGET ${_target} PROPERTY CXX_VISIBILITY_PRESET hidden)
  endforeach()
endif()
