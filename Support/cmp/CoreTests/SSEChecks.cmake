
# In this file we are doing all of our 'configure' checks. Things like checking
# for headers, functions, libraries, types and size of types.
INCLUDE (${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)
INCLUDE (${CMAKE_ROOT}/Modules/CheckTypeSize.cmake)
INCLUDE (${CMAKE_ROOT}/Modules/CheckFunctionExists.cmake)
INCLUDE (${CMAKE_ROOT}/Modules/CheckCXXSourceCompiles.cmake)

CHECK_INCLUDE_FILE("stddef.h" HAVE_STDDEF_H)
CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
CHECK_INCLUDE_FILE("sys/types.h" HAVE_SYS_TYPES_H)


set (SSE_COMPILE_FLAGS "")
option(AIM_USE_SSE "Use SSE2/3 Instructions where possible." ON)
if (AIM_USE_SSE)
  if (NOT MSVC)
    set(CMAKE_REQUIRED_C_FLAGS_SAVE ${CMAKE_REQUIRED_C_FLAGS})
    set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS} "-msse3")
    CHECK_INCLUDE_FILE("pmmintrin.h" HAVE_SSE3_H)
    set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS_SAVE})
    if (HAVE_SSE3_H)
        set(HAVE_SSE2_H 1)
        if (CMAKE_COMPILER_IS_GNUCC)
            set (SSE_COMPILE_FLAGS "-msse3")
            message(STATUS "SSE3 Detected")
        endif()
    else()
        set (HAVE_SSE3_H 0)
    endif()

    if (NOT HAVE_SSE3_H)
        set(CMAKE_REQUIRED_C_FLAGS_SAVE ${CMAKE_REQUIRED_C_FLAGS})
        set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS} "-msse2")
        CHECK_INCLUDE_FILE("emmintrin.h" HAVE_SSE2_H)
        set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS_SAVE})
        if(HAVE_SSE2_H)
            if (CMAKE_COMPILER_IS_GNUCC)
                set (SSE_COMPILE_FLAGS "-msse2" PARENT_SCOPE)
                message(STATUS "SSE2 Detected")
            endif()
        else()
           set (HAVE_SSE2_H 0)
        endif()
    endif()
  else()
    set(CMAKE_REQUIRED_C_FLAGS_SAVE ${CMAKE_REQUIRED_C_FLAGS})
    set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS} "/arch:SSE2")
    CHECK_INCLUDE_FILE("intrin.h" HAVE_INTRIN_H)
    set(CMAKE_REQUIRED_C_FLAGS ${CMAKE_REQUIRED_C_FLAGS_SAVE})
    if (HAVE_INTRIN_H)
        set (SSE_COMPILE_FLAGS " /arch:SSE2 " PARENT_SCOPE)
    else()
        set (HAVE_INTRIN_H 0)
    endif()
  endif()
endif()
