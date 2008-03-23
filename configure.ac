# Prelude
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([etherlab_rtw], [trunk], [lerich@gmx.net])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
ETHERLAB_DIR=/opt/etherlab
AC_PREFIX_DEFAULT([/opt/etherlab/rtw])
AC_SUBST(ETHERLAB_DIR)

dnl Compile and install Simulink related components (default: yes)
AC_ARG_ENABLE([simulink],
    AS_HELP_STRING(
        [--disable-simulink],
        [Do not install Simulink components]),
    [ case "${enableval}" in
        yes) simulink=1 ;;
        no)  simulink=0 ;;
        *)   AC_MSG_ERROR([bad value for --enable-simulink]) ;;
      esac],
    [ simulink=1 ]
)
AM_CONDITIONAL(SIMULINK, test x$simulink = x1)
AC_SUBST(SIMULINK,${simulink})

dnl Compile and install rt_kernel related components (default: yes)
AC_ARG_ENABLE([kernel],
    AS_HELP_STRING(
        [--disable-kernel],
        [Do not install RT-Kernel components]),
    [ case "${enableval}" in
        yes) kernel=1 ;;
        no)  kernel=0 ;;
        *)   AC_MSG_ERROR([bad value for --enable-kernel]) ;;
      esac],
    [ kernel=1 ]
)
AM_CONDITIONAL(KERNEL, test x$kernel = x1)
AC_SUBST(KERNEL,${kernel})

dnl Staging directory for host compile
dnl Sometimes you want to create an environment where the real time models
dnl are compiled on a different host than the target. In this case you can
dnl compile the target host's packages normally, install them there as well
dnl as in a staging directory (using DESTDIR=/staging/dir). To tell
dnl etherlab about this, use --with-staging-dir
dnl Note that nothing will be installed in $staging_dir
dnl 
AC_ARG_WITH([staging-dir], 
    AS_HELP_STRING(
        [--with-staging-dir=<staging-dir>],
        [Host staging directory for cross compile]),
    [ staging_dir=${withval} ],
    [ staging_dir= ]
    )
if test "x$staging_dir" != x; then
AC_MSG_CHECKING(for $ETHERLAB_DIR in $staging_dir)
test -d "$staging_dir/$ETHERLAB_DIR" ||\
    AC_MSG_ERROR([Could not find $staging_dir/$ETHERLAB_DIR])
AC_MSG_RESULT(yes)
fi
AC_SUBST(STAGING_DIR,[$staging_dir])

AC_ARG_WITH([matlab-dir], 
    AS_HELP_STRING(
        [--with-matlab-dir=<matlab-dir>],
        [Optionally enter Matlab(tm) directory to pre-build matlab library]),
    [ matlab_dir=${withval} ],
    [ matlab_dir= ]
    )
if test "x$matlab_dir" != x; then
matlab_rtw_src=$matlab_dir/rtw/c/libsrc
AC_MSG_CHECKING([for $matlab_rtw_src])
test -d "$matlab_rtw_src" ||\
    AC_MSG_ERROR([Could not locate library sources in $matlab_rtw_src])
AC_MSG_RESULT(yes)
AC_SUBST(MATLAB_ROOT,[$matlab_dir])
AM_CONDITIONAL(MATLAB_ROOT, test "x$matlab_dir" != x)
fi

dnl Path to RTAI installation
if test "x$kernel" = "x1"; then
AC_ARG_WITH([rtai-dir], 
    AS_HELP_STRING(
        [--with-rtai-dir=<rtai-dir>],
        [RTAI directory ($staging_dir/usr/realtime)]),
    [ rtai_dir=${withval} ],
    [
        if test -x "$staging_dir/usr/realtime/bin/rtai-config"; then
            rtai_dir="$staging_dir/usr/realtime"
        else
            rtai_dir=$(type -p rtai-config)
            rtai_dir=${rtai_dir%%bin/rtai-config}
        fi
    ])
AC_MSG_CHECKING(RTAI path)
test -r "$staging_dir/$rtai_dir"/include/rtai.h ||\
    AC_MSG_ERROR([No installed RTAI found $staging_dir/$rtai_dir/include/rtai.h])
AC_SUBST(RTAI_DIR,[$rtai_dir])
AC_MSG_RESULT($RTAI_DIR)
RTAI_FLAGS=$($staging_dir/$rtai_dir/bin/rtai-config --module-cflags)
RTAI_FLAGS=${RTAI_FLAGS//-I./}
RTAI_FLAGS=${RTAI_FLAGS//-I/-I$staging_dir}
AC_SUBST(RTAI_FLAGS)
AC_MSG_RESULT(RTAI_FLAGS = $RTAI_FLAGS)
fi

dnl Make sure rtai-config reports correct kernel
dnl and that IPIPE is configured for the kernel
if test "x$kernel" = "x1"; then
AC_ARG_WITH([linux-dir], 
    AS_HELP_STRING(
        [--with-linux-dir=<linux-dir>],
        [Kernel directory (rtai-config --linux-dir)]),
    [ linux_dir=${withval} ],
    [
        linux_dir=$($staging_dir/${RTAI_DIR}/bin/rtai-config --linux-dir)
    ])
AC_MSG_CHECKING(Linux kernel path)
test -r "${linux_dir}/.config" || \
        AC_MSG_ERROR([.config not found in ${linux_dir}])
AC_MSG_RESULT(${linux_dir})
AC_MSG_CHECKING(Checking IPIPE configured in kernel)
ipipe_conf=$(make -s -f - -I${linux_dir} <<'EOF'
include .config
all:
	@echo -n $(CONFIG_IPIPE)
EOF)
test "x${ipipe_conf}" == "xy" || \
    AC_MSG_ERROR(not configured)
AC_MSG_RESULT(yes)
AC_SUBST(LINUX_DIR,$linux_dir)
fi

dnl When kernel is selected, EtherCat can be switched off
if test "x$kernel" = "x1"; then
AC_ARG_ENABLE([ethercat],
    AS_HELP_STRING(
        [--disable-ethercat],
        [Do not compile in EtherCat components]),
    [ case "${enableval}" in
        yes) ethercat=1 ;;
        no)  ethercat=0 ;;
        *)   AC_MSG_ERROR([bad value for --disable-ethercat]) ;;
      esac],
    [ ethercat=1 ]
)
if test "x$ethercat" = "x1"; then
AC_MSG_CHECKING(EtherCat header file)
test -r "$staging_dir/${ETHERLAB_DIR}/include/ecrt.h" || \
        AC_MSG_ERROR([ecrt.h not found in $staging_dir/${ETHERLAB_DIR}/include])
AC_MSG_RESULT($staging_dir/${ETHERLAB_DIR}/include/ecrt.h)
fi
fi
AM_CONDITIONAL(ETHERCAT, test x$ethercat = x1)
AC_SUBST(ETHERCAT,${ethercat})

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
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(SED, sed)
AC_PATH_PROG(ED, ed)
AC_PATH_PROG(GREP, grep)
AC_PATH_PROG(PYTHON, python)
AC_PATH_PROG(AR, ar)

# Checks for libraries
AC_CHECK_LIB([dl],[dlopen],[:],[
    AC_MSG_ERROR([dlopen() not found])
])
AC_CHECK_LIB([z],[deflate],[:],[
    AC_MSG_ERROR([deflate() not found])
])

# Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
        arpa/inet.h \
        fcntl.h  \
        limits.h \
        netinet/in.h  \
        inttypes.h  \
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
        etherlab/etherlab_config.m
        blocks/Makefile 
        blocks/setup.m 
        blocks/AddiData/Makefile 
        blocks/AddiData/setup.m 
        blocks/Hilscher/Makefile 
        blocks/Hilscher/setup.m 
        blocks/EtherCAT/Makefile
        blocks/EtherCAT/setup.m 
        src/Makefile
        tools/Makefile
        tools/buddy/Makefile
        tools/buddy-ng/Makefile
        tools/scripts/Makefile
        tools/scripts/etherlab
        tools/rt_kernel/Kbuild
        tools/rt_kernel/Makefile
        tools/include/Makefile
        lib/Kbuild
        lib/Makefile
        lib/Makefile.rtw
])
AC_OUTPUT
