# Process this file with autoconf to produce a configure script, like so:
# aclocal -I ../config && autoconf && autoheader && automake

AC_INIT([GNU Offloading and Multi Processing Runtime Library], 1.0,,[libgomp])
AC_CONFIG_HEADER(config.h)

# -------
# Options
# -------

AC_MSG_CHECKING([for --enable-version-specific-runtime-libs])
LIBGOMP_ENABLE(version-specific-runtime-libs, no, ,
   [Specify that runtime libraries should be installed in a compiler-specific directory],
   permit yes|no)
AC_MSG_RESULT($enable_version_specific_runtime_libs)

# We would like our source tree to be readonly. However when releases or
# pre-releases are generated, the flex/bison generated files as well as the
# various formats of manuals need to be included along with the rest of the
# sources.  Therefore we have --enable-generated-files-in-srcdir to do 
# just that.
AC_MSG_CHECKING([for --enable-generated-files-in-srcdir])
LIBGOMP_ENABLE(generated-files-in-srcdir, no, ,
   [put copies of generated files in source dir intended for creating source 
    tarballs for users without texinfo bison or flex.],
   permit yes|no)
AC_MSG_RESULT($enable_generated_files_in_srcdir)
AM_CONDITIONAL(GENINSRC, test "$enable_generated_files_in_srcdir" = yes)


# -------
# -------

# Gets build, host, target, *_vendor, *_cpu, *_os, etc.
#
# You will slowly go insane if you do not grok the following fact:  when
# building this library, the top-level /target/ becomes the library's /host/.
#
# configure then causes --target to default to --host, exactly like any
# other package using autoconf.  Therefore, 'target' and 'host' will
# always be the same.  This makes sense both for native and cross compilers
# just think about it for a little while.  :-)
#
# Also, if this library is being configured as part of a cross compiler, the
# top-level configure script will pass the "real" host as $with_cross_host.
#
# Do not delete or change the following two lines.  For why, see
# http://gcc.gnu.org/ml/libstdc++/2003-07/msg00451.html
AC_CANONICAL_SYSTEM
target_alias=${target_alias-$host_alias}

# Sets up automake.  Must come after AC_CANONICAL_SYSTEM.  Each of the
# following is magically included in AUTOMAKE_OPTIONS in each Makefile.am.
#  1.9.0:  minimum required version
#  no-define:  PACKAGE and VERSION will not be #define'd in config.h (a bunch
#              of other PACKAGE_* variables will, however, and there's nothing
#              we can do about that; they come from AC_INIT).
#  foreign:  we don't follow the normal rules for GNU packages (no COPYING
#            file in the top srcdir, etc, etc), so stop complaining.
#  no-dist:  we don't want 'dist' and related rules.
#  -Wall:  turns on all automake warnings...
#  -Wno-portability:  ...except this one, since GNU make is required.
#  -Wno-override: ... and this one, since we do want this in testsuite.
AM_INIT_AUTOMAKE([1.9.0 foreign no-dist -Wall -Wno-portability -Wno-override])
AM_ENABLE_MULTILIB(, ..)

GCC_WITH_TOOLEXECLIBDIR

# Calculate toolexeclibdir
# Also toolexecdir, though it's only used in toolexeclibdir
case ${enable_version_specific_runtime_libs} in
  yes)
    # Need the gcc compiler version to know where to install libraries
    # and header files if --enable-version-specific-runtime-libs option
    # is selected.
    toolexecdir='$(libdir)/gcc/$(target_alias)'
    toolexeclibdir='$(toolexecdir)/$(gcc_version)$(MULTISUBDIR)'
    ;;
  no)
    if test -n "$with_cross_host" &&
       test x"$with_cross_host" != x"no"; then
      # Install a library built with a cross compiler in tooldir, not libdir.
      toolexecdir='$(exec_prefix)/$(target_alias)'
      case ${with_toolexeclibdir} in
	no)
	  toolexeclibdir='$(toolexecdir)/lib'
	  ;;
	*)
	  toolexeclibdir=${with_toolexeclibdir}
	  ;;
      esac
    else
      toolexecdir='$(libdir)/gcc-lib/$(target_alias)'
      toolexeclibdir='$(libdir)'
    fi
    multi_os_directory=`$CC -print-multi-os-directory`
    case $multi_os_directory in
      .) ;; # Avoid trailing /.
      *) toolexeclibdir=$toolexeclibdir/$multi_os_directory ;;
    esac
    ;;
esac
AC_SUBST(toolexecdir)
AC_SUBST(toolexeclibdir)

# Check the compiler.
# The same as in boehm-gc and libstdc++. Have to borrow it from there.
# We must force CC to /not/ be precious variables; otherwise
# the wrong, non-multilib-adjusted value will be used in multilibs.
# As a side effect, we have to subst CFLAGS ourselves.

m4_rename([_AC_ARG_VAR_PRECIOUS],[real_PRECIOUS])
m4_define([_AC_ARG_VAR_PRECIOUS],[])
AC_PROG_CC
m4_rename_force([real_PRECIOUS],[_AC_ARG_VAR_PRECIOUS])

AC_SUBST(CFLAGS)

# In order to override CFLAGS_FOR_TARGET, all of our special flags go
# in XCFLAGS.  But we need them in CFLAGS during configury.  So put them
# in both places for now and restore CFLAGS at the end of config.
save_CFLAGS="$CFLAGS"

# Add -Wall -Werror if we are using GCC.
if test "x$GCC" = "xyes"; then
  XCFLAGS="$XCFLAGS -Wall -Werror"
fi

# Find other programs we need.
AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib, ranlib-not-found-in-path-error)
AC_PATH_PROG(PERL, perl, perl-not-found-in-path-error)
AC_PROG_MAKE_SET
AC_PROG_INSTALL

# See if makeinfo has been installed and is modern enough
# that we can use it.
ACX_CHECK_PROG_VER([MAKEINFO], [makeinfo], [--version],
                   [GNU texinfo.* \([0-9][0-9.]*\)],
                   [4.[4-9]*|4.[1-9][0-9]*|[5-9]*|[1-9][0-9]*])
AM_CONDITIONAL(BUILD_INFO, test $gcc_cv_prog_makeinfo_modern = "yes")


# Configure libtool
AM_PROG_LIBTOOL
ACX_LT_HOST_FLAGS
AC_SUBST(enable_shared)
AC_SUBST(enable_static)

AM_MAINTAINER_MODE

# Create a spec file, so that compile/link tests don't fail
test -f libgfortran.spec || touch libgfortran.spec
FCFLAGS="$FCFLAGS -L."

# We need gfortran to compile parts of the library
# We can't use AC_PROG_FC because it expects a fully working gfortran.
#AC_PROG_FC(gfortran)
case `echo $GFORTRAN` in
  -* | no* )
    FC=no ;;
  *)
    set dummy $GFORTRAN; ac_word=$2
    if test -x "$ac_word"; then
      FC="$GFORTRAN"
    else
      FC=no
    fi ;;
esac
AC_PROG_FC(gfortran)
FCFLAGS="$FCFLAGS -Wall -L../libgfortran"

# For libtool versioning info, format is CURRENT:REVISION:AGE
libtool_VERSION=1:0:0
AC_SUBST(libtool_VERSION)

# Check header files.
AC_STDC_HEADERS
AC_HEADER_TIME
ACX_HEADER_STRING
AC_CHECK_HEADERS(pthread.h unistd.h semaphore.h sys/loadavg.h sys/sysctl.h sys/time.h)

XPCFLAGS=""
case "$host" in
  *-*-rtems*)
    # RTEMS supports Pthreads, but the library is not available at GCC build time.
    ;;
  nvptx*-*-* | amdgcn*-*-*)
    # NVPTX does not support Pthreads, has its own code replacement.
    libgomp_use_pthreads=no
    # NVPTX is an accelerator-only target
    libgomp_offloaded_only=yes
    ;;
  *)
    # Check to see if -pthread or -lpthread is needed.  Prefer the former.
    # In case the pthread.h system header is not found, this test will fail.
    CFLAGS="$CFLAGS -pthread"
    AC_LINK_IFELSE(
     [AC_LANG_PROGRAM(
      [#include <pthread.h>
       void *g(void *d) { return NULL; }],
      [pthread_t t; pthread_create(&t,NULL,g,NULL);])],
     [XPCFLAGS=" -Wc,-pthread"],
     [CFLAGS="$save_CFLAGS" LIBS="-lpthread $LIBS"
      AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
        [#include <pthread.h>
         void *g(void *d) { return NULL; }],
        [pthread_t t; pthread_create(&t,NULL,g,NULL);])],
       [],
       [AC_MSG_ERROR([Pthreads are required to build libgomp])])])
esac

if test x$libgomp_use_pthreads != xno; then
  AC_DEFINE(LIBGOMP_USE_PTHREADS, 1,
            [Define to 1 if libgomp should use POSIX threads.])
fi

if test x$libgomp_offloaded_only = xyes; then
  AC_DEFINE(LIBGOMP_OFFLOADED_ONLY, 1,
            [Define to 1 if building libgomp for an accelerator-only target.])
fi

if test "x$enable_offload_defaulted" = xyes; then
  AC_DEFINE(OFFLOAD_DEFAULTED, 1,
    [Define to 1 to if -foffload is defaulted])
fi

AC_CHECK_SIZEOF([void *])

m4_include([plugin/configfrag.ac])

# Check for functions needed.
AC_CHECK_FUNCS(getloadavg clock_gettime strtoull)
AC_CHECK_FUNCS(aligned_alloc posix_memalign memalign _aligned_malloc)

# Check for broken semaphore implementation on darwin.
# sem_init returns: sem_init error: Function not implemented.
case "$host" in
  *-darwin*)
    AC_DEFINE(HAVE_BROKEN_POSIX_SEMAPHORES, 1,
	Define if the POSIX Semaphores do not work on your system.)
    ;;
esac

# RTEMS specific checks
case "$host" in
  *-*-rtems*)
    AC_CHECK_TYPES([struct _Mutex_Control],[],[],[#include <sys/lock.h>])
    ;;
esac

GCC_LINUX_FUTEX(:)

# Check for pthread_{,attr_}[sg]etaffinity_np.
AC_LINK_IFELSE(
 [AC_LANG_PROGRAM(
  [#define _GNU_SOURCE
   #include <pthread.h>],
  [cpu_set_t cpuset;
   pthread_attr_t attr;
   pthread_getaffinity_np (pthread_self (), sizeof (cpu_set_t), &cpuset);
   if (CPU_ISSET (0, &cpuset))
     CPU_SET (1, &cpuset);
   else
     CPU_ZERO (&cpuset);
   pthread_setaffinity_np (pthread_self (), sizeof (cpu_set_t), &cpuset);
   pthread_attr_init (&attr);
   pthread_attr_getaffinity_np (&attr, sizeof (cpu_set_t), &cpuset);
   pthread_attr_setaffinity_np (&attr, sizeof (cpu_set_t), &cpuset);])],
  AC_DEFINE(HAVE_PTHREAD_AFFINITY_NP, 1,
[	Define if pthread_{,attr_}{g,s}etaffinity_np is supported.]))

# At least for glibc, clock_gettime is in librt.  But don't pull that
# in if it still doesn't give us the function we want.
if test $ac_cv_func_clock_gettime = no; then
  AC_CHECK_LIB(rt, clock_gettime,
    [LIBS="-lrt $LIBS"
     AC_DEFINE(HAVE_CLOCK_GETTIME, 1,
	       [Define to 1 if you have the `clock_gettime' function.])])
fi

# Check for uname.
AC_COMPILE_IFELSE(
 [AC_LANG_PROGRAM(
  [#include <string.h>
   #include <stdlib.h>
   #include <sys/utsname.h>],
  [struct utsname buf;
   volatile size_t len = 0;
   if (!uname (buf))
     len = strlen (buf.nodename);])],
  AC_DEFINE(HAVE_UNAME, 1,
[	Define if uname is supported and struct utsname has nodename field.]))

# Check for gethostname.
AC_COMPILE_IFELSE(
 [AC_LANG_PROGRAM(
  [#include <unistd.h>],
  [
changequote(,)dnl
   char buf[256];
   if (gethostname (buf, sizeof (buf) - 1) == 0)
     buf[255] = '\0';
changequote([,])dnl
  ])],
  AC_DEFINE(HAVE_GETHOSTNAME, 1,
[	Define if gethostname is supported.]))

# Check for getpid.
AC_COMPILE_IFELSE(
 [AC_LANG_PROGRAM(
  [#include <unistd.h>],
  [int pid = getpid ();])],
  AC_DEFINE(HAVE_GETPID, 1,
[	Define if getpid is supported.]))

# See if we support thread-local storage.
GCC_CHECK_TLS

# See if we have emulated thread-local storage.
GCC_CHECK_EMUTLS

# See what sort of export controls are available.
LIBGOMP_CHECK_ATTRIBUTE_VISIBILITY
LIBGOMP_CHECK_ATTRIBUTE_DLLEXPORT
LIBGOMP_CHECK_ATTRIBUTE_ALIAS
LIBGOMP_ENABLE_SYMVERS

if test $enable_symvers = gnu; then
  AC_DEFINE(LIBGOMP_GNU_SYMBOL_VERSIONING, 1,
	    [Define to 1 if GNU symbol versioning is used for libgomp.])
fi

# Determine cpu count to limit testsuite parallelism.
AX_COUNT_CPUS
AC_SUBST(CPU_COUNT)

# Get target configury.
. ${srcdir}/configure.tgt
CFLAGS="$save_CFLAGS $XCFLAGS"

# Check for __sync_val_compare_and_swap, but only after the target has
# had a chance to set XCFLAGS.
LIBGOMP_CHECK_SYNC_BUILTINS

XCFLAGS="$XCFLAGS$XPCFLAGS"

# Add CET specific flags if CET is enabled
GCC_CET_FLAGS(CET_FLAGS)
XCFLAGS="$XCFLAGS $CET_FLAGS"
FCFLAGS="$FCFLAGS $CET_FLAGS"

AC_SUBST(config_path)
AC_SUBST(XCFLAGS)
AC_SUBST(XLDFLAGS)

# Conditionalize the makefile for this target machine.
tmake_file_=
for f in ${tmake_file}
do
	if test -f ${srcdir}/config/$f
	then
		tmake_file_="${tmake_file_} \$(srcdir)/config/$f"
	fi
done
tmake_file="${tmake_file_}"
AC_SUBST(tmake_file)

# Cleanup and exit.
CFLAGS="$save_CFLAGS"
AC_CACHE_SAVE

if test ${multilib} = yes; then
  multilib_arg="--enable-multilib"
else
  multilib_arg=
fi

# Set up the set of libraries that we need to link against for libgomp.
# Note that the GOMP_SELF_SPEC in gcc.c may force -pthread,
# which will force linkage against -lpthread (or equivalent for the system).
# That's not 100% ideal, but about the best we can do easily.
if test $enable_shared = yes; then
  link_gomp="-lgomp %{static: $LIBS}"
else
  link_gomp="-lgomp $LIBS"
fi
AC_SUBST(link_gomp)

AM_CONDITIONAL([USE_FORTRAN], [test "$ac_cv_fc_compiler_gnu" = yes])

# ??? 2006-01-24: Paulo committed to asking autoconf folk to document
# and export AC_COMPUTE_INT.  If that happens, then we'll need to remove
# the underscore here and update the PREREQ.  If it doesn't, then we'll
# need to copy this macro to our acinclude.m4.
save_CFLAGS="$CFLAGS"
for i in $config_path; do
  if test -f $srcdir/config/$i/omp-lock.h; then
    CFLAGS="$CFLAGS -include confdefs.h -include $srcdir/config/$i/omp-lock.h"
    break
  fi
done

_AC_COMPUTE_INT([sizeof (__INTPTR_TYPE__)], [INTPTR_T_KIND])
_AC_COMPUTE_INT([sizeof (__int128)], [OMP_INT128_SIZE],,[OMP_INT128_SIZE=0])
_AC_COMPUTE_INT([2*sizeof (__INTPTR_TYPE__)], [OMP_DEPEND_KIND],,
		[OMP_DEPEND_KIND=0])
_AC_COMPUTE_INT([sizeof (omp_lock_t)], [OMP_LOCK_SIZE],,
  [AC_MSG_ERROR([unsupported system, cannot find sizeof (omp_lock_t)])])
_AC_COMPUTE_INT([__alignof (omp_lock_t)], [OMP_LOCK_ALIGN])
_AC_COMPUTE_INT([sizeof (omp_nest_lock_t)], [OMP_NEST_LOCK_SIZE])
_AC_COMPUTE_INT([__alignof (omp_nest_lock_t)], [OMP_NEST_LOCK_ALIGN])
_AC_COMPUTE_INT([sizeof (omp_lock_25_t)], [OMP_LOCK_25_SIZE],,
  [AC_MSG_ERROR([unsupported system, cannot find sizeof (omp_lock_25_t)])])
_AC_COMPUTE_INT([__alignof (omp_lock_25_t)], [OMP_LOCK_25_ALIGN])
_AC_COMPUTE_INT([sizeof (omp_nest_lock_25_t)], [OMP_NEST_LOCK_25_SIZE])
_AC_COMPUTE_INT([__alignof (omp_nest_lock_25_t)], [OMP_NEST_LOCK_25_ALIGN])

# If the lock fits in an integer, then arrange for Fortran to use that
# integer.  If it doesn't, then arrange for Fortran to use a pointer.
# Except that we don't have a way at present to multi-lib the installed
# Fortran modules, so we assume 8 bytes for pointers, regardless of the
# actual target.
OMP_LOCK_KIND=$OMP_LOCK_SIZE
OMP_NEST_LOCK_KIND=$OMP_NEST_LOCK_SIZE
if test $OMP_LOCK_SIZE -gt 8 || test $OMP_LOCK_ALIGN -gt $OMP_LOCK_SIZE; then
  OMP_LOCK_KIND=8
fi
if test $OMP_NEST_LOCK_SIZE -gt 8 || test $OMP_NEST_LOCK_ALIGN -gt $OMP_NEST_LOCK_SIZE; then
  OMP_NEST_LOCK_KIND=8
fi
OMP_LOCK_25_KIND=$OMP_LOCK_25_SIZE
OMP_NEST_LOCK_25_KIND=$OMP_NEST_LOCK_25_SIZE
if test $OMP_LOCK_25_SIZE -gt 8 || test $OMP_LOCK_25_ALIGN -gt $OMP_LOCK_25_SIZE; then
  OMP_LOCK_25_KIND=8
fi
if test $OMP_NEST_LOCK_25_SIZE -gt 8 || test $OMP_NEST_LOCK_25_ALIGN -gt $OMP_NEST_LOCK_25_SIZE; then
  OMP_NEST_LOCK_25_KIND=8
fi
if test "$ac_cv_fc_compiler_gnu" = yes; then
  if test $OMP_DEPEND_KIND -eq 16; then
    if test $OMP_INT128_SIZE -ne 16; then
      AC_MSG_ERROR([unsupported system, cannot find Fortran int kind=16, needed for omp_depend_kind])
    fi
  else
    if test $OMP_DEPEND_KIND -ne 8; then
      AC_MSG_ERROR([unsupported system, cannot find Fortran integer kind for omp_depend_kind])
    fi
  fi
fi

AC_SUBST(INTPTR_T_KIND)
AC_SUBST(OMP_LOCK_SIZE)
AC_SUBST(OMP_LOCK_ALIGN)
AC_SUBST(OMP_NEST_LOCK_SIZE)
AC_SUBST(OMP_NEST_LOCK_ALIGN)
AC_SUBST(OMP_LOCK_KIND)
AC_SUBST(OMP_NEST_LOCK_KIND)
AC_SUBST(OMP_LOCK_25_SIZE)
AC_SUBST(OMP_LOCK_25_ALIGN)
AC_SUBST(OMP_NEST_LOCK_25_SIZE)
AC_SUBST(OMP_NEST_LOCK_25_ALIGN)
AC_SUBST(OMP_LOCK_25_KIND)
AC_SUBST(OMP_NEST_LOCK_25_KIND)
AC_SUBST(OMP_DEPEND_KIND)
CFLAGS="$save_CFLAGS"

# Determine what GCC version number to use in filesystem paths.
GCC_BASE_VER

AC_CONFIG_FILES(omp.h omp_lib.h omp_lib.f90 libgomp_f.h)
AC_CONFIG_FILES(Makefile testsuite/Makefile libgomp.spec)
AC_CONFIG_FILES([testsuite/libgomp-test-support.pt.exp:testsuite/libgomp-test-support.exp.in])
AC_CONFIG_FILES([testsuite/libgomp-site-extra.exp])
AC_OUTPUT
