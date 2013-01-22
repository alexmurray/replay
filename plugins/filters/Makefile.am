# filters plugin
plugindir = $(libdir)/$(PACKAGE)/plugins/filters

INCLUDES = \
	-I$(top_srcdir) 				\
	-DREPLAY_PLUGINS_DATADIR=\""$(datadir)/$(PACKAGE)/plugins"\" \
	$(WARNING_CFLAGS) \
	$(DEPRECATED_CFLAGS) \
	$(REPLAY_DEBUG_CFLAGS) \
	$(REPLAY_CFLAGS)

plugin_LTLIBRARIES = libfilters.la

libfilters_la_SOURCES = \
	replay-filters-plugin.h	\
	replay-filters-plugin.c	\
	replay-filter-list.h \
	replay-filter-list.c \
	replay-filters-dialog.h \
	replay-filters-dialog.c

libfilters_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libfilters_la_LIBADD  = $(REPLAY_LIBS)

uidir = $(datadir)/$(PACKAGE)/plugins/filters
ui_DATA = replay-filter-props-dialog.ui replay-filters-dialog.ui

plugin_in_files = filters.plugin.desktop.in

%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

EXTRA_DIST = $(ui_DATA) $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
