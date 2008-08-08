#-----------------------------------------------------------------------------
#
# $Id$
#
# Prelude
#                                               -*- Autoconf -*-
#
# Process this file with autoconf to produce a configure script.
#
#-----------------------------------------------------------------------------

AC_PREREQ(2.59)
AC_INIT([etherlab], [trunk], [lerich@gmx.net])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT(/opt/etherlab)
AC_SUBST(RTW_DIR,[rtw])

#-----------------------------------------------------------------------------
# Simulink
#-----------------------------------------------------------------------------

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

#-----------------------------------------------------------------------------
# AppCore
#-----------------------------------------------------------------------------

dnl Compile and install appcore related components (default: yes)
AC_ARG_ENABLE([appcore],
    AS_HELP_STRING(
        [--disable-appcore],
        [Do not install AppCore components]),
    [ case "${enableval}" in
        yes) appcore=1 ;;
        no)  appcore=0 ;;
        *)   AC_MSG_ERROR([bad value for --enable-appcore]) ;;
      esac],
    [ appcore=1 ]
)

AM_CONDITIONAL(APPCORE, test x$appcore = x1)
AC_SUBST(APPCORE,${appcore})

#-----------------------------------------------------------------------------
# Staging directory
#-----------------------------------------------------------------------------

dnl Staging directory for host compile
dnl Sometimes you want to create an environment where the real time models
dnl are compiled on a different host than the target. In this case you can
dnl compile the target host's packages normally, install them there as well
dnl as in a staging directory (using DESTDIR=/staging/dir). To tell
dnl etherlab about this, use --with-staging-dir
dnl Note that nothing will be installed in $staging_dir

AC_ARG_WITH([staging-dir], 
    AS_HELP_STRING(
        [--with-staging-dir=<staging-dir>],
        [Host staging directory for cross compile]),
    [ staging_dir=${withval} ],
    [ staging_dir= ]
    )

AC_SUBST(STAGING_DIR,[$staging_dir])

#-----------------------------------------------------------------------------
# RTAI directory
#-----------------------------------------------------------------------------

dnl Path to RTAI installation
AC_ARG_WITH([rtai-dir], 
    AS_HELP_STRING(
        [--with-rtai-dir=<rtai-dir>],
        [RTAI directory ($staging_dir/usr/realtime)]),
    [ rtai_dir=${withval} ],
    [
        if test -x "$staging_dir/usr/realtime/bin/rtai-config"; then
            rtai_dir="/usr/realtime"
        else
            rtai_dir=$(type -p rtai-config)
            rtai_dir=${rtai_dir%%bin/rtai-config}
        fi
    ])

if test "x$appcore" = "x1"; then

AC_SUBST(RTAI_DIR,[$rtai_dir])

AC_MSG_CHECKING(RTAI path)
dir="$staging_dir/$rtai_dir"
header="$dir/include/rtai.h"
test -r $header || AC_MSG_ERROR([No installed RTAI found in $dir])
AC_MSG_RESULT($rtai_dir)

fi # appcore

#-----------------------------------------------------------------------------
# Linux directory
#-----------------------------------------------------------------------------

dnl Make sure rtai-config reports correct kernel
dnl and that IPIPE is configured for the kernel

AC_ARG_WITH([linux-dir], 
    AS_HELP_STRING(
        [--with-linux-dir=<linux-dir>],
        [Kernel directory (rtai-config --linux-dir)]),
    [ linux_dir=${withval} ],
    [
        linux_dir=$($staging_dir/${RTAI_DIR}/bin/rtai-config --linux-dir)
    ])

if test "x$appcore" = "x1"; then

AC_SUBST(LINUX_DIR,$linux_dir)

AC_MSG_CHECKING(Linux kernel path)
test -r "${linux_dir}/.config" || \
        AC_MSG_ERROR([.config not found in ${linux_dir}])
AC_MSG_RESULT(${linux_dir})

AC_MSG_CHECKING(IPIPE configured in kernel)
ipipe_conf=$(make -s -f - -I${linux_dir} <<'EOF'
include .config
all:
	@echo -n $(CONFIG_IPIPE)
EOF)
test "x${ipipe_conf}" == "xy" || \
    AC_MSG_ERROR(not configured)
AC_MSG_RESULT(yes)

fi # appcore

#-----------------------------------------------------------------------------
# EtherCAT
#-----------------------------------------------------------------------------

AC_ARG_ENABLE([ethercat],
    AS_HELP_STRING(
        [--disable-ethercat],
        [Do not compile in EtherCAT components]),
    [ case "${enableval}" in
        yes) ethercat=1 ;;
        no)  ethercat=0 ;;
        *)   AC_MSG_ERROR([bad value for --disable-ethercat]) ;;
      esac],
    [ ethercat=1 ]
)

AM_CONDITIONAL(ETHERCAT, test x$ethercat = x1)
AC_SUBST(ETHERCAT,${ethercat})

dnl Path to EtherCAT installation
AC_ARG_WITH([ethercat-dir], 
    AS_HELP_STRING(
        [--with-ethercat-dir=<ethercat-dir>],
        [EtherCAT directory (/opt/etherlab)]),
    [ ethercat_dir=${withval} ],
    [ ethercat_dir=/opt/etherlab ])

if test "x$appcore" = "x1" -a "x$ethercat" = "x1"; then

AC_MSG_CHECKING(EtherCAT path)
dir="${staging_dir}/${ethercat_dir}"
header="${dir}/include/ecrt.h"
test -r $header ||\
    AC_MSG_ERROR([No installed EtherCAT found in $dir])
AC_MSG_RESULT($ethercat_dir)

AC_SUBST(ETHERCAT_DIR,[$ethercat_dir])

fi # appcore and ethercat

#-----------------------------------------------------------------------------
# Experimental
#-----------------------------------------------------------------------------

dnl Allow experimental stuff for development

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

#-----------------------------------------------------------------------------
# Buddy NG
#-----------------------------------------------------------------------------

AC_ARG_ENABLE([buddy-ng],
    AS_HELP_STRING(
        [--enable-buddy-ng],
        [Enable new next generation buddy]),
    [ case "${enableval}" in
        yes) : ;;
        *)   enableval=
      esac],
    [ enableval= ]
)
AM_CONDITIONAL(BUDDY_NG, test x$enableval != x)

#-----------------------------------------------------------------------------
# Debug
#-----------------------------------------------------------------------------

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

#-----------------------------------------------------------------------------

# Checks for programs
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(UNZIP, unzip)
#AC_PATH_PROG(SED, sed)
#AC_PATH_PROG(ED, ed)
AC_PATH_PROG(GREP, grep)
#AC_PATH_PROG(PYTHON, python)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(XSLTPROC, xsltproc)
AC_PATH_PROG(INDENT, indent)
AC_PATH_PROG(CP, cp)

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
        rtw/Makefile 
        rtw/blocks/Makefile 
        rtw/blocks/setup.m 
        rtw/blocks/Hilscher/Makefile 
        rtw/blocks/AddiData/Makefile 
        rtw/blocks/EtherCAT/Makefile
        rtw/blocks/EtherCAT/xml/Makefile
        rtw/blocks/EtherCAT/@EtherCATInfo/Makefile
        rtw/blocks/EtherCAT/@EtherCATInfo/private/Makefile
        rtw/etherlab/Makefile 
        rtw/etherlab/etherlab_kmod.tmf
        rtw/lib/Makefile
        rtw/src/Makefile
        capi/Kbuild.makefile
        capi/Makefile
        tools/Makefile
        tools/buddy-ng/Makefile
        tools/buddy/Makefile
        tools/include/Makefile
        tools/rt_appcore/Makefile
        tools/scripts/Makefile
        tools/scripts/etherlab
])
AC_OUTPUT

#-----------------------------------------------------------------------------
