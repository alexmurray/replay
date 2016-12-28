# Process this file with autoconf to produce a configure script.
# ================= initialization =================== #
m4_define([replay_major_version],  0)
m4_define([replay_minor_version],  8)
m4_define([replay_micro_version],  1)
m4_define([replay_version], [replay_major_version].[replay_minor_version].[replay_micro_version])

AC_PREREQ([2.68])
AC_INIT([replay],[replay_version],[https://github.com/alexmurray/replay/issues/new],[replay],[http://alexmurray.github.com/replay])

AC_CONFIG_SRCDIR([replay/replay.c])
AC_CONFIG_HEADERS([replay/config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.10 dist-bzip2 -Wno-portability])
AM_MAINTAINER_MODE([enable])
GNOME_MAINTAINER_MODE_DEFINES

IT_PROG_INTLTOOL([0.40.0])

PKG_PROG_PKG_CONFIG

# = quieten compiler output if have automake >= 1.11 = #
# = and enable by default (yes) ====================== #
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# ============ libtool for libraries ================= #
LT_PREREQ(2.2.6)
LT_INIT

# ============== basic compiler settings ============= #
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_HEADER_STDC

# ========== export compiler / linker options ======== #
AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

# ============ some extra warning flags ============== #
WARNING_CFLAGS="\
    -Wall -Wextra -Werror \
    -Wno-missing-field-initializers \
    -Wno-unused-parameter \
    -Wdeclaration-after-statement \
    -Wshadow -Wwrite-strings -Winline -Wformat-nonliteral \
    -Wformat-security -Wswitch-enum -Wswitch-default \
    -Winit-self -Wmissing-include-dirs -Wundef -Waggregate-return \
    -Wmissing-format-attribute -Wnested-externs"

AC_SUBST(WARNING_CFLAGS)

DEPRECATED_CFLAGS="\
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED"

AC_SUBST(DEPRECATED_CFLAGS)

# ========== extra debugging options ================= #
m4_define([debug_default],[minimum])
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [turn on debugging @<:@default=debug_default@:>@])],
                              [],
                              [enable_debug=debug_default])
AS_CASE([$enable_debug],
        [yes], [ test "$cflags_set" = set || CFLAGS="$CFLAGS -O0 -g3 -ggdb" REPLAY_DEBUG_CFLAGS="-DREPLAY_DEBUG" ],
        [minimum], [ REPLAY_DEBUG_CFLAGS="-DG_DISABLE_CAST_CHECKS" ],
        [no], [ REPLAY_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS" ],
        [AC_MSG_ERROR([Unknown argument to --enable-debug])])
AC_SUBST(REPLAY_DEBUG_CFLAGS)

# ============ gettext stuff ========================= #
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
AC_SUBST([GETTEXT_PACKAGE], [replay])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], "$GETTEXT_PACKAGE", [Define to the gettext package name.])

# ============== look for dependencies =============== #
# check for headers needed for standard interfaces
AC_CHECK_HEADERS(netinet/in.h \
                 math.h \
                 stdio.h \
                 string.h \
                 sys/stat.h \
                 sys/time.h \
                 sys/types.h \
                 unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset])
AC_CHECK_LIB([m],[sqrt])

GLIB_REQUIRED=2.26.0
GTHREAD_REQUIRED=2.6.0
GTK_REQUIRED=3.4.0
LIBPEAS_REQUIRED=0.7.2

PKG_CHECK_MODULES(REPLAY, [glib-2.0 >= $GLIB_REQUIRED
                           gthread-2.0 >= $GTHREAD_REQUIRED
   			   gtk+-3.0 >= $GTK_REQUIRED
                           libpeas-1.0 >= $LIBPEAS_REQUIRED
                           libpeas-gtk-1.0 >= $LIBPEAS_REQUIRED
                           gio-unix-2.0
                           gtkglext-3.0
                           glu
                           freetype2])

AC_SUBST(REPLAY_CFLAGS)
AC_SUBST(REPLAY_LIBS)

# python plugin support
PYTHON_REQUIRED=2.5.2
AM_PATH_PYTHON([$PYTHON_REQUIRED])

# glib utilities
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

# gsettings
GLIB_GSETTINGS

# allow to enable / disable updates to desktop
AC_ARG_ENABLE([database-updates],
              AS_HELP_STRING([--disable-database-updates],
                             [disable mime and desktop database updates (useful for distcheck and distribution packagers)]))
AM_CONDITIONAL(DATABASE_UPDATES, [test "$enable_database_updates" != "no"])

DISTCHECK_CONFIGURE_FLAGS="${DISTCHECK_CONFIGURE_FLAGS} --disable-database-updates"
AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, update-desktop-database)
AC_SUBST(DISTCHECK_CONFIGURE_FLAGS)

GNOME_COMPILE_WARNINGS(maximum)

# ==================== plugins ======================= #
PLUGIN_LIBTOOL_FLAGS="-module -avoid-version"
AC_SUBST(PLUGIN_LIBTOOL_FLAGS)

# ============== graphviz plugin ===================== #
PKG_CHECK_MODULES(GRAPHVIZ, libcgraph, have_graphviz="yes", have_graphviz="no")
AC_SUBST(GRAPHVIZ_CFLAGS)
AC_SUBST(GRAPHVIZ_LIBS)
AM_CONDITIONAL(BUILD_GRAPHVIZ, [test x"$have_graphviz" != x"no"])

# ================== yelp help stuff ================= #
YELP_HELP_INIT

# ================ gtk-doc stuff ===================== #
GTK_DOC_CHECK(1.9)

# =========== gobject-introspection stuff ============ #
GOBJECT_INTROSPECTION_CHECK([1.30.0])
if test "$found_introspection" = "yes"; then
    AC_DEFINE([ENABLE_INTROSPECTION], [1], [Define to enable GObject Introspection])
fi

# ================= print status to user ============= #
AC_MSG_NOTICE([
               ========== Building Replay replay_version ==========
               Enable API Reference:     ${enable_gtk_doc}
               Enable Introspection:     ${found_introspection}
               Enable Debugging:         ${enable_debug}
               Building Graphviz Plugin: ${have_graphviz}
               ])

# ================= generate files =================== #
AC_CONFIG_FILES([Makefile
                 data/replay.pc
                 data/Makefile
                 docs/Makefile
                 docs/help/Makefile
                 docs/reference/Makefile
                 docs/reference/version.xml
                 pixmaps/Makefile
                 plugins/Makefile
                 plugins/export/Makefile
                 plugins/causeway/Makefile
                 plugins/fdr/Makefile
                 plugins/filters/Makefile
                 plugins/graphviz/Makefile
                 plugins/pixmaps/Makefile
                 plugins/playback/Makefile
                 plugins/search/Makefile
                 po/Makefile.in
                 ui/Makefile
                 replay/Makefile])
AC_OUTPUT
