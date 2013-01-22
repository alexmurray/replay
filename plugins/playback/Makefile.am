# playback plugin
plugindir = $(libdir)/$(PACKAGE)/plugins/playback

INCLUDES = \
	-I$(top_srcdir) 				\
	-DREPLAY_PLUGINS_DATADIR=\""$(datadir)/$(PACKAGE)/plugins"\" \
	$(WARNING_CFLAGS) \
	$(DEPRECATED_CFLAGS) \
	$(REPLAY_DEBUG_CFLAGS) \
	$(REPLAY_CFLAGS)

plugin_LTLIBRARIES = libplayback.la

libplayback_la_SOURCES = \
	replay-playback-plugin.h	\
	replay-playback-plugin.c

libplayback_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
libplayback_la_LIBADD  = $(REPLAY_LIBS)

plugin_in_files = playback.plugin.desktop.in

%.plugin: %.plugin.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.plugin.desktop.in=.plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
