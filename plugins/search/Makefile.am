# search plugin
plugindir = $(libdir)/$(PACKAGE)/plugins/search

INCLUDES = \
	-I$(top_srcdir) 				\
	-DREPLAY_PLUGINS_DATADIR=\""$(datadir)/$(PACKAGE)/plugins"\" \
	$(WARNING_CFLAGS) \
	$(DEPRECATED_CFLAGS) \
	$(REPLAY_DEBUG_CFLAGS) \
	$(REPLAY_CFLAGS)

plugin_LTLIBRARIES = libsearch.la

libsearch_la_SOURCES = \
	replay-search-plugin.h	\
	replay-search-plugin.c	\
	replay-search-window.h \
	replay-search-window.c

libsearch_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libsearch_la_LIBADD  = $(REPLAY_LIBS)

plugin_in_files = search.plugin.desktop.in

%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
