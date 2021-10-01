#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([nvidia-oobaml], [0.1], [kuzhao@nvidia.com])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AC_LANG([C++])

# Checks for programs.
AC_PROG_CXX
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AX_PTHREAD
AC_CHECK_LIB(rt, shm_open, [],
    [AC_MSG_ERROR([Could not find librt])]
    )

# Checks for header files.
AC_CHECK_HEADER(nlohmann/json.hpp, [],
    AC_MSG_ERROR([Could not find nlohmann/json.hpp])
    )

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_11([noext])

CXXFLAGS="$CXXFLAGS -std=c++11"

# Checks for library functions.
LT_INIT

# Make sure the pkgconfigdata is configured for automake
PKG_INSTALLDIR

# Debug log option
AC_ARG_ENABLE([debug-log],
    AS_HELP_STRING([--enable-debug-log@<:@=LEVEL@:>@],
        [Enable debug log LEVEL [0|1|2|3|4] [default=1]]
        ),
    AS_CASE([$enable_debug_log], [0|1|2|3|4],
        AX_APPEND_FLAG([-DDEF_DBG_LEVEL=$enable_debug_log], [CXXFLAGS]),
        AC_MSG_ERROR([Debug log setting ($enable_debug_log) is out of range!])
        ),
    )


# Source files
AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
                 test/Makefile
                 tools/Makefile])
AC_OUTPUT
