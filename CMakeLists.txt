cmake_minimum_required(VERSION 3.1)

# =============================================================================
# [MathPresso - Configuration]
# =============================================================================

# Embedding mode, mathpresso will not create any targets (default FALSE).
# set(MATHPRESSO_EMBED FALSE)

# Whether to build a static library (default FALSE).
# set(MATHPRESSO_STATIC FALSE)

# Whether to build tests and samples (default FALSE).
# set(MATHPRESSO_BUILD_TEST FALSE)

# =============================================================================
# [MathPresso - Build / Embed]
# =============================================================================

# Do not create a project if this CMakeLists.txt is included from another
# project. This makes it easy to embed or create a static library.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" MATCHES "^mathpresso$")
  project(mathpresso C CXX)
  set(MATHPRESSO_SIGNATURE "Standalone")
else()
  set(MATHPRESSO_SIGNATURE "Included")
endif()

if(MATHPRESSO_EMBED)
  set(MATHPRESSO_SIGNATURE "${MATHPRESSO_SIGNATURE} | Mode=Embed")
  set(MATHPRESSO_STATIC TRUE) # Implies MATHPRESSO_STATIC.
elseif(MATHPRESSO_STATIC)
  set(MATHPRESSO_SIGNATURE "${MATHPRESSO_SIGNATURE} | Mode=Static")
else()
  set(MATHPRESSO_SIGNATURE "${MATHPRESSO_SIGNATURE} | Mode=Shared")
endif()

if(MATHPRESSO_BUILD_TEST)
  set(MATHPRESSO_SIGNATURE "${MATHPRESSO_SIGNATURE} | Test=On")
else()
  set(MATHPRESSO_SIGNATURE "${MATHPRESSO_SIGNATURE} | Test=Off")
endif()

if(NOT MATHPRESSO_DIR)
  set(MATHPRESSO_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

message("-- [mathpresso] ${MATHPRESSO_SIGNATURE}")
message("-- [mathpresso] MATHPRESSO_DIR=${MATHPRESSO_DIR}")

# =============================================================================
# [MathPresso - Flags / Deps]
# =============================================================================

set(MATHPRESSO_SOURCE_DIR "${MATHPRESSO_DIR}/src")    # MathPresso source directory.
set(MATHPRESSO_INCLUDE_DIR "${MATHPRESSO_SOURCE_DIR}")# MathPresso include directory.

set(MATHPRESSO_DEPS)                                  # MathPresso dependencies (list of libraries) for the linker.
set(MATHPRESSO_LIBS)                                  # MathPresso dependencies with mathpresso included, for consumers.

# Internal, never use.
set(MATHPRESSO_D "-D")                                # Used to define a C/C++ preprocessor parameter (-D or /D).
set(MATHPRESSO_PRIVATE_CFLAGS)                        # Compiler flags independent of build type.
set(MATHPRESSO_PRIVATE_CFLAGS_REL)                    # Compiler flags used only by debug build.
set(MATHPRESSO_PRIVATE_CFLAGS_DBG)                    # Compiler flags used only by release build.

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(MATHPRESSO_D "/D")

  list(APPEND MATHPRESSO_PRIVATE_CFLAGS /GF)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_DBG /GS /GR-)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS_REL /Oi /Oy /GS- /GR-)

  if(NOT MSVC60 AND NOT MSVC70 AND NOT MSVC71)
    list(APPEND MATHPRESSO_PRIVATE_CFLAGS /MP)        # Enable multi-process compilation.
  endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)$")
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS -fno-exceptions)
endif()

if(MATHPRESSO_EMBED)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${MATHPRESSO_D}MATHPRESSO_EMBED")
elseif(MATHPRESSO_STATIC)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${MATHPRESSO_D}MATHPRESSO_STATIC")
endif()

# Dependencies.
if(NOT ASMJIT_DIR)
  set(ASMJIT_DIR "${MATHPRESSO_DIR}/../asmjit")
endif()
set(ASMJIT_EMBED TRUE)
include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND MATHPRESSO_DEPS ${ASMJIT_LIBS})
list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${MATHPRESSO_D}ASMJIT_EMBED")

# Use Unicode by default on Windows target.
if(WIN32)
  list(APPEND MATHPRESSO_PRIVATE_CFLAGS "${MATHPRESSO_D}_UNICODE")
endif()

set(MATHPRESSO_LIBS ${MATHPRESSO_DEPS})
if(NOT MATHPRESSO_EMBED)
  list(INSERT MATHPRESSO_LIBS 0 mathpresso)
endif()

message("-- [mathpresso] MATHPRESSO_DEPS=${MATHPRESSO_DEPS}")
message("-- [mathpresso] MATHPRESSO_LIBS=${MATHPRESSO_LIBS}")

# =============================================================================
# [MathPresso - Macros]
# =============================================================================

macro(mathpresso_add_source _out_dst _src_dir)
  set(_src_path "${MATHPRESSO_SOURCE_DIR}/${_src_dir}")
  set(_src_list)

  foreach(_arg ${ARGN})
    set(_src_file "${_src_path}/${_arg}")
    list(APPEND _src_list ${_src_file})
  endforeach()

  list(APPEND "${_out_dst}" ${_src_list})
  source_group(${_src_dir} FILES ${_src_list})
endmacro()

macro(mathpresso_add_library _target _src _deps _cflags _cflags_dbg _cflags_rel)
  if(NOT MATHPRESSO_STATIC)
    add_library(${_target} SHARED ${_src})
  else()
    add_library(${_target} STATIC ${_src})
  endif()
  target_link_libraries(${_target} ${_deps})

  if(CMAKE_BUILD_TYPE)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      target_compile_options(${_target} PRIVATE ${_cflags} ${_cflags_dbg})
    else()
      target_compile_options(${_target} PRIVATE ${_cflags} ${_cflags_rel})
    endif()
  else()
    target_compile_options(${_target} PRIVATE ${_cflags}
      $<$<CONFIG:Debug>:${_cflags_dbg}>
      $<$<NOT:$<CONFIG:Debug>>:${_cflags_rel}>)
  endif()

  if(NOT MATHPRESSO_STATIC)
    install(TARGETS ${_target} DESTINATION "lib${LIB_SUFFIX}")
  endif()
endmacro()

# =============================================================================
# [MathPresso - Sources]
# =============================================================================

set(MATHPRESSO_SRC "")

mathpresso_add_source(MATHPRESSO_SRC mathpresso
  mathpresso.cpp
  mathpresso.h
  mathpresso_ast.cpp
  mathpresso_ast_p.h
  mathpresso_context.cpp
  mathpresso_context_p.h
  mathpresso_jit.cpp
  mathpresso_jit_p.h
  mathpresso_optimizer.cpp
  mathpresso_optimizer_p.h
  mathpresso_parser.cpp
  mathpresso_parser_p.h
  mathpresso_tokenizer.cpp
  mathpresso_tokenizer_p.h
  mathpresso_util.cpp
  mathpresso_util_p.h
)

# =============================================================================
# [MathPresso - Targets]
# =============================================================================

if(NOT MATHPRESSO_EMBED)
  mathpresso_add_library(mathpresso
    "${MATHPRESSO_SRC};${ASMJIT_SRC}"
    "${MATHPRESSO_DEPS}"
    "${MATHPRESSO_PRIVATE_CFLAGS}"
    "${MATHPRESSO_PRIVATE_CFLAGS_REL}"
    "${MATHPRESSO_PRIVATE_CFLAGS_DBG}"
  )
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

# =============================================================================
# [MathPresso - Test]
# =============================================================================

if(MATHPRESSO_BUILD_TEST)
  add_executable(mpeval src/test/mpeval.cpp)
  target_link_libraries(mpeval ${MATHPRESSO_LIBS})

  add_executable(mptest src/test/mptest.cpp)
  target_link_libraries(mptest ${MATHPRESSO_LIBS})
endif()
