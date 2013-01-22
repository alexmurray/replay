# graphviz plugin
plugindir = $(libdir)/$(PACKAGE)/plugins/graphviz

INCLUDES = \
	-I$(top_srcdir) 				\
	$(WARNING_CFLAGS) \
	$(DEPRECATED_CFLAGS) \
	$(REPLAY_DEBUG_CFLAGS) \
	$(REPLAY_CFLAGS) \
	$(GRAPHVIZ_CFLAGS)

plugin_LTLIBRARIES = libgraphviz.la

libgraphviz_la_SOURCES = \
	replay-graphviz-plugin.h	\
	replay-graphviz-plugin.c \
	replay-graphviz-file-loader.h \
	replay-graphviz-file-loader.c

libgraphviz_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libgraphviz_la_LIBADD  = $(REPLAY_LIBS) $(GRAPHVIZ_LIBS)

plugin_in_files = graphviz.plugin.desktop.in

%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
