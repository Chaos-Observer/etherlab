# Prelude
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([etherlab_rtw], [1.0], [lerich@gmx.net])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
ETHERLAB_DIR=/opt/etherlab
AC_PREFIX_DEFAULT([/opt/etherlab/rtw])
AC_SUBST(ETHERLAB_DIR)

dnl RTAI Path
AC_ARG_WITH([rtai-dir], 
    AS_HELP_STRING(
        [--with-rtai-dir=<rtai-dir>],
        [RTAI directory [[/usr/realtime]]]),
    [ ],
    [
        withval=$(type -p rtai-config)
        withval=${withval%%bin/rtai-config}
        test x"$withval" == x && withval=/usr/realtime
    ])
AC_MSG_CHECKING(RTAI path)
test -r "$withval"/include/rtai.h ||\
    AC_MSG_ERROR([No installed RTAI found $withval/include/rtai.h])
AC_SUBST(RTAI_DIR,[$withval])
AC_MSG_RESULT($RTAI_DIR)
RTAI_FLAGS=$($RTAI_DIR/bin/rtai-config --module-cflags)
AC_SUBST(RTAI_FLAGS)

dnl Make sure rtai-config reports correct kernel
LINUX_DIR=$($RTAI_DIR/bin/rtai-config --linux-dir)
AC_MSG_CHECKING(Linux kernel path)
test -r "$LINUX_DIR/.config" && \
    sh -c "source $LINUX_DIR/.config; test x\"\$CONFIG_IPIPE\" == \"xy\"" || \
    AC_MSG_ERROR([IPIPE not configured in $LINUX_DIR/.config])
AC_MSG_RESULT($LINUX_DIR)
AC_SUBST(LINUX_DIR)

dnl Allow experimental stuff  for development
AC_ARG_ENABLE([experimental],
    AS_HELP_STRING(
        [--enable-experimental],
        [Enable experimental features]),
    [ case "${enableval}" in
        yes) exp=1 ;;
        no)  exp=0 ;;
        *)   AC_MSG_ERROR([bad value for --enable-experimental]) ;;
      esac],
    [ exp=0 ]
)
AM_CONDITIONAL(EXPERIMENTAL, test x$exp = x1)
AC_SUBST(EXPERIMENTAL,${exp})

dnl Allow experimental stuff  for development
AC_ARG_ENABLE([debug],
    AS_HELP_STRING(
        [--enable-debug],
        [Enable to show more kernel messages]),
    [ case "${enableval}" in
        yes) debug=1
                AC_DEFINE([DEBUG], [1],
                [Enable to generate debug messages in the code]) ;;
        no) debug=0 ;;
        *)   AC_MSG_ERROR([bad value for --enable-debug]) ;;
      esac ],
    [ debug=0 ]
)

# Checks for programs
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(RM, rm)
#AC_PATH_PROG(XARGS, xargs)

# Checks for libraries
AC_CHECK_LIB([dl],[dlopen],[:],[
    AC_MSG_ERROR([dlopen() not found])
])

# Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
        arpa/inet.h \
        fcntl.h  \
        limits.h \
        netinet/in.h  \
        stdint.h  \
        stdlib.h  \
        string.h  \
        sys/ioctl.h  \
        sys/socket.h  \
        unistd.h \
])

# Checks for typedefs, structures and compiler characteristics
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions
AC_FUNC_ERROR_AT_LINE
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([memset mkfifo munmap select socket strdup])

# Output files
AC_CONFIG_FILES([
        Makefile 
        etherlab/Makefile 
        etherlab/etherlab_kmod.tmf
        etherlab/setup_etherlab.m 
        etherlab/etherlab_tools.mk 
        blocks/Makefile 
        blocks/setup.m 
        blocks/Kbuild 
        blocks/AddiData/Makefile 
        blocks/AddiData/setup.m 
        blocks/Hilscher/Makefile 
        blocks/Hilscher/setup.m 
        blocks/EtherCAT/Makefile
        blocks/EtherCAT/Kbuild
        blocks/EtherCAT/setup.m 
        src/Makefile
        src/include/Makefile
        src/buddy/Makefile
        src/kernel/Makefile
        tools/Makefile
        tools/buddy/Makefile
        tools/scripts/Makefile
        tools/scripts/etherlab
        tools/rt_kernel/Kbuild
        tools/rt_kernel/Makefile
        tools/include/Makefile
        lib/Makefile
])
AC_OUTPUT
