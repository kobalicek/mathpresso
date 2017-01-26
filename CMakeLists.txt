cmake_minimum_required(VERSION 3.1)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "mathpresso")
  project(mathpresso C CXX)
endif()

# =============================================================================
# [MathPresso - Configuration]
# =============================================================================

set(MATHPRESSO_DIR         "${CMAKE_CURRENT_LIST_DIR}"   CACHE PATH "Location of 'mathpresso'")
set(ASMJIT_DIR             "${MATHPRESSO_DIR}/../asmjit" CACHE PATH "Location of 'asmjit'")

set(MATHPRESSO_EMBED       FALSE               CACHE BOOLEAN "Embed 'mathpresso' library (no targets)")
set(MATHPRESSO_STATIC      ${MATHPRESSO_EMBED} CACHE BOOLEAN "Build 'mathpresso' library as static")
set(MATHPRESSO_BUILD_TEST  FALSE               CACHE BOOLEAN "Build 'mathpresso_test' applications")

if(NOT DEFINED ASMJIT_EMBED)
  set(ASMJIT_EMBED TRUE CACHE BOOLEAN "")
endif()

# =============================================================================
# [MathPresso - Project]
# =============================================================================

include("${MATHPRESSO_DIR}/CxxProject.cmake")
cxx_project(mathpresso)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS /GF)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_DBG /GS /GR-)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_REL /Oi /Oy /GS- /GR-)

  if(NOT MSVC60 AND NOT MSVC70 AND NOT MSVC71)
    list(APPEND MATHPRESSO_PRIVATE_CFLAGS /MP)        # Enable multi-process compilation.
  endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)$")
  cxx_detect_standard(MATHPRESSO_PRIVATE_CFLAGS)
  cxx_detect_cflags(MATHPRESSO_PRIVATE_CFLAGS
    "-fvisibility=hidden"
    "-fno-exceptions"
    "-fno-tree-vectorize"
    "-Winconsistent-missing-override")
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_REL -fmerge-all-constants)
endif()

include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND MATHPRESSO_DEPS ${ASMJIT_LIBS})
list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${ASMJIT_CFLAGS}")

# Use Unicode by default on Windows target.
if(WIN32)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${CXX_DEFINE}_UNICODE")
endif()

set(MATHPRESSO_LIBS ${MATHPRESSO_DEPS})
if(NOT MATHPRESSO_EMBED)
  list(INSERT MATHPRESSO_LIBS 0 mathpresso)
endif()

list(REMOVE_DUPLICATES MATHPRESSO_DEPS)
list(REMOVE_DUPLICATES MATHPRESSO_PRIVATE_CFLAGS)

cxx_project_info(mathpresso)

# =============================================================================
# [MathPresso - Sources]
# =============================================================================

cxx_add_source(mathpresso MATHPRESSO_SRC mathpresso
  mathpresso.cpp
  mathpresso.h
  mathpresso_p.h
  mpast.cpp
  mpast_p.h
  mpatomic_p.h
  mpcompiler.cpp
  mpcompiler_p.h
  mpeval_p.h
  mphash.cpp
  mphash_p.h
  mpoptimizer.cpp
  mpoptimizer_p.h
  mpparser.cpp
  mpparser_p.h
  mpstrtod_p.h
  mptokenizer.cpp
  mptokenizer_p.h
)

# =============================================================================
# [MathPresso - Targets]
# =============================================================================

if(NOT MATHPRESSO_EMBED)
  cxx_add_library(mathpresso
    mathpresso
    "${MATHPRESSO_SRC};${ASMJIT_SRC}"
    "${MATHPRESSO_DEPS}"
    "${MATHPRESSO_PRIVATE_CFLAGS}"
    "${MATHPRESSO_PRIVATE_CFLAGS_REL}"
    "${MATHPRESSO_PRIVATE_CFLAGS_DBG}")
  message("II=${ASMJIT_INCLUDE_DIR}")
  target_include_directories(mathpresso PRIVATE BEFORE ${ASMJIT_INCLUDE_DIR})

  foreach(i ${MATHPRESSO_SRC})
    get_filename_component(path ${i} PATH)
    get_filename_component(name ${i} NAME)
    string(REGEX REPLACE "^${MATHPRESSO_SOURCE_DIR}/" "" targetpath "${path}")
    if(${name} MATCHES ".h$")
      if(NOT "${name}" MATCHES "_p.h$")
        install(FILES ${i} DESTINATION "include/${targetpath}")
      endif()
    endif()
  endforeach()
endif()

if(MATHPRESSO_BUILD_TEST)
  cxx_add_executable(mathpresso mpeval     "test/mpeval.cpp"     "${MATHPRESSO_LIBS}" "${MATHPRESSO_CFLAGS}" "" "")
  cxx_add_executable(mathpresso mptest     "test/mptest.cpp"     "${MATHPRESSO_LIBS}" "${MATHPRESSO_CFLAGS}" "" "")
  cxx_add_executable(mathpresso mptutorial "test/mptutorial.cpp" "${MATHPRESSO_LIBS}" "${MATHPRESSO_CFLAGS}" "" "")
endif()
