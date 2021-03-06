# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# V8 / ICU
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()

project(V8)
set(V8_VERSION 4.3.61)

# create target directory

set(V8_DIR "${CMAKE_CURRENT_SOURCE_DIR}/V8-${V8_VERSION}")
set(V8_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/v8")

file(MAKE_DIRECTORY ${V8_TARGET_DIR})

# detect endianess, and translate to gyp/v8 proper string

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)

if (${IS_BIGENDIAN})
  set(ENDIANESS 'big')
else()
  set(ENDIANESS 'little')
endif()

# detect 32bit or 64bit

if ("${BITS}" STREQUAL "64")
  set(V8_PROC_ARCH "x64")
else ()
  message(ERROR "#### WILLY FIX ME ####")
  set(V8_PROC_ARCH "ia32")
endif () # TODO: ARM? Win32?

# target architecture

list(APPEND V8_GYP_ARGS
  --generator-output=${V8_TARGET_DIR}
  -Dv8_target_arch=${V8_PROC_ARCH}
  -Dbyteorder=${ENDIANESS}
  -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
)

################################################################################
## ICU EXPORTS
################################################################################

set(ICU_VERSION
  "54.1"
  CACHE INTERNAL
  "ICU: Version"
)

set(ICU_INCLUDE_DIR
  ${V8_DIR}/third_party/icu/source/common;${V8_DIR}/third_party/icu/source/i18n;${V8_DIR}/third_party/icu/source/io
  CACHE INTERNAL
  "ICU: include path"
  FORCE
)

set(V8_VERSION
  ${V8_VERSION}
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
)

set(V8_INCLUDE_DIR
  ${V8_DIR}/include
  CACHE INTERNAL
  "${PROJECT_NAME}: include path"
)

################################################################################
## WINDOWS
################################################################################

if (WIN32)
  set(GYP_MAIN ${V8_DIR}/build/gyp_v8)

  # try to find 2013 in 'Visual Studio 12 2013 Win64'
  if (${CMAKE_GENERATOR} MATCHES "2013") 
    list(APPEND V8_GYP_ARGS "-G msvs_version=2013")
  endif()

  # try to find 2015 in 'Visual Studio 14 2015 Win64'
  if (${CMAKE_GENERATOR} MATCHES "2013") 
    list(APPEND V8_GYP_ARGS "-G msvs_version=2015")
  endif()

  if ("${BITS}" STREQUAL "64")
    set(MS_TARGET_PLATFORM x64)
  else ()
    set(MS_TARGET_PLATFORM Win32)
  endif ()

  list(APPEND V8_GYP_ARGS -Dtarget_arch=${V8_PROC_ARCH} -Dhost_arch=${V8_PROC_ARCH})
  list(APPEND V8_GYP_ARGS ${V8_DIR}/build/all.gyp)

  set(V8_TARGET_ARCHS "Debug;Release" CACHE string "V8 targets to build")

  list(APPEND V8_GYP_ARGS
    -Dcomponent=static_library
    -Dlibrary=static_library
    #--depth=2
    #-Dv8_use_external_startup_data=1
    #-Dicu_gyp_path=../third_party/icu/icu.gyp
    #-Dv8_optimized_debug=0
    #-Dos_posix=0
    #-Dclang=0
    #-Dwant_separate_host_toolset=0
  )

  add_custom_target(v8_build)

  foreach (MS_TARGET_CONF ${V8_TARGET_ARCHS})
    string(TOLOWER ${MS_TARGET_CONF} MS_TARGET_CONF_LOWER)

    ExternalProject_Add(v8_build_${MS_TARGET_CONF_LOWER}
      BUILD_IN_SOURCE
        TRUE
      WORKING_DIRECTORY
        ${V8_DIR}/build
      SOURCE_DIR
        "${V8_DIR}"
      CONFIGURE_COMMAND
        ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS} -Dmode=${MS_TARGET_CONF_LOWER}
      BUILD_COMMAND
        ""
      INSTALL_COMMAND
        ""
    )

    add_dependencies(v8_build v8_build_${MS_TARGET_CONF_LOWER})

    # and now ICU...
    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} icu_build_icudata
      WORKING_DIRECTORY
        ${V8_DIR}/third_party/icu
      COMMAND
        msbuild icu.sln /t:icudata /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} icu_build_icui18n
      WORKING_DIRECTORY
        ${V8_DIR}/third_party/icu
      COMMAND
        msbuild icu.sln /t:icui18n /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} icu_build_icuuc
      WORKING_DIRECTORY
        ${V8_DIR}/third_party/icu
      COMMAND
        msbuild icu.sln /t:icuuc /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    # cmake eats semicolons or quotes them or, or, or so we can't use /t:a;b;c
    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8
      WORKING_DIRECTORY
        ${V8_DIR}/build
      COMMAND
        msbuild all.sln /t:v8 /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8_libbase
      WORKING_DIRECTORY
        ${V8_DIR}/build
      COMMAND
        msbuild all.sln /t:v8_libbase /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )

    ExternalProject_Add_Step(v8_build_${MS_TARGET_CONF_LOWER} v8_build_v8_libplatform
      WORKING_DIRECTORY
        ${V8_DIR}/build
      COMMAND
        msbuild all.sln /t:v8_libplatform /p:Configuration=${MS_TARGET_CONF} /p:Platform=${MS_TARGET_PLATFORM}
      DEPENDEES
        configure
      DEPENDERS
        build
    )
  endforeach ()

  set(ICU_LIBRARY_DEBUG "${V8_DIR}/build/Debug")
  set(ICU_LIBRARY_RELEASE "${V8_DIR}/build/Release")

  set(ICU_LIBS
    debug "${ICU_LIBRARY_DEBUG}/lib/icui18n.lib;${ICU_LIBRARY_DEBUG}/lib/icuuc.lib"
    optimized "${ICU_LIBRARY_RELEASE}/lib/icui18n.lib;${ICU_LIBRARY_RELEASE}/lib/icuuc.lib"
    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
    )

  set(V8_LIBRARY_DEBUG "${V8_DIR}/build/Debug")
  set(V8_LIBRARY_RELEASE "${V8_DIR}/build/Release")

  set(V8_LIBS
    debug "${V8_LIBRARY_DEBUG}/lib/v8_base.lib;${V8_LIBRARY_DEBUG}/lib/v8_libbase.lib;${V8_LIBRARY_DEBUG}/lib/v8_libplatform.lib;${V8_LIBRARY_DEBUG}/lib/v8_nosnapshot.lib"
    optimized "${V8_LIBRARY_RELEASE}/lib/v8_base.lib;${V8_LIBRARY_RELEASE}/lib/v8_libbase.lib;${V8_LIBRARY_RELEASE}/lib/v8_libplatform.lib;${V8_LIBRARY_RELEASE}/lib/v8_nosnapshot.lib"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

else ()

################################################################################
## UNIX
################################################################################

  option(USE_DEBUG_V8 "compile V8 in DEBUG mode" OFF)

  if (USE_DEBUG_V8)
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.debug")
  else ()
    set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")
  endif ()

  if (USE_DEBUG_V8)
    list(APPEND V8_GYP_ARGS -Dv8_enable_backtrace=1)
  endif ()

  set(V8_LIBRARY_DIR "${V8_TARGET_DIR}/${V8_TARGET_ARCH}")
  set(ICU_LIBRARY_DIR "${V8_LIBRARY_DIR}")

  set(GYP_MAIN ${V8_DIR}/build/gyp/gyp_main.py)

  list(APPEND V8_GYP_ARGS build/all.gyp)
  list(APPEND V8_GYP_ARGS --format make)
  list(APPEND V8_GYP_ARGS -S.${V8_TARGET_ARCH})

  if (LINUX OR SOLARIS)
    set(V8_LIBRARY_DIR "${V8_LIBRARY_DIR}/obj.target/tools/gyp")
    set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}/obj.target/third_party/icu")
  endif ()

  list(APPEND V8_GYP_ARGS
    -Ibuild/standalone.gypi
    --depth=.
    -Dstandalone_static_library=1
    -Dlinux_use_bundled_gold=0
    -Dlinux_use_gold_flags=0
    -Dv8_no_strict_aliasing=1
    -Dwerror=
  )

  set(V8_COMPILE_ARGS
    -C ${V8_TARGET_DIR}
    -f Makefile.${V8_TARGET_ARCH}
    BUILDTYPE=Release
    builddir=${V8_TARGET_DIR}/${V8_TARGET_ARCH}
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
  )

  if (APPLE AND CMAKE_COMPILER_IS_CLANG)
    list(APPEND V8_COMPILE_ARGS CXXFLAGS=-stdlib=libc++ LDFLAGS=-stdlib=libc++)
  endif ()

  list(APPEND V8_COMPILE_ARGS icui18n icuuc icudata)
  list(APPEND V8_COMPILE_ARGS v8)

  ExternalProject_Add(v8_build
    BUILD_IN_SOURCE
      TRUE
    SOURCE_DIR
      "${V8_DIR}"
    CONFIGURE_COMMAND
      ${PYTHON_EXECUTABLE} ${GYP_MAIN} ${V8_GYP_ARGS}
    BUILD_COMMAND
      $(MAKE) ${V8_COMPILE_ARGS}
    STEP_TARGETS
      v8_base v8_libbase v8_libplatform v8_nosnapshot icui18n icuuc icudata
    INSTALL_COMMAND
      "")

  list(APPEND LINK_DIRECTORIES "${V8_LIBRARY_DIR}")

  set(V8_LIBS
    "v8_base;v8_libbase;v8_libplatform;v8_nosnapshot"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
  )

  list(APPEND LINK_DIRECTORIES "${ICU_LIBRARY_DIR}")

  set(ICU_LIBS
    "icui18n;icuuc;icudata"
    CACHE INTERNAL
    "ICU: Libraries"
  )

  set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)
endif()

