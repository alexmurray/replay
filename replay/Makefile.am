## Process this file with automake to produce Makefile.in
AM_CFLAGS =     $(WARNING_CFLAGS) \
		-DDATADIR=\""$(datadir)"\" \
		-DREPLAY_DATADIR=\""$(datadir)/$(PACKAGE)"\" \
		-DREPLAY_TEXTURES_DIR=\""$(datadir)/$(PACKAGE)/textures"\" \
		-DLIBDIR=\""$(libdir)"\" \
		-DSYSCONFDIR=\""$(sysconfdir)"\" \
		-DPREFIX=\""$(prefix)"\" \
		-I$(top_srcdir) \
		$(DEPRECATED_CFLAGS) \
		$(REPLAY_DEBUG_CFLAGS) \
		$(REPLAY_CFLAGS) \
		$(INTROSPECTION_CFLAGS)

AM_LDFLAGS =	$(REPLAY_LIBS) $(INTROSPECTION_LIBS)

bin_PROGRAMS = 	 replay
replay_SOURCES = replay.c \
		 replay.h

pkginclude_HEADERS =    replay-event.h \
			replay-node-create-event.h \
			replay-node-delete-event.h \
			replay-node-props-event.h \
			replay-edge-create-event.h \
			replay-edge-delete-event.h \
			replay-edge-props-event.h \
			replay-activity-start-event.h \
			replay-activity-end-event.h \
			replay-msg-send-event.h \
			replay-msg-recv-event.h \
			replay-events.h \
			replay-property-store.h \
			replay-event-source.h \
			replay-event-store.h \
			replay-task.h \
			replay-file-loader.h \
			replay-network-server.h \
			replay-log.h \
			replay-application.h \
			replay-window.h \
			replay-node-tree.h \
			replay-graph-view.h \
			replay-message-tree.h \
			replay-message-tree-view.h \
			replay-string-utils.h \
			replay-timestamp.h \
			replay-application-activatable.h \
			replay-window-activatable.h \
			replay-option-activatable.h

# enums
enums_templates = replay-enums.c.template replay-enums.h.template
enums_in_files = $(pkginclude_HEADERS)
enums_built_files = replay-enums.c replay-enums.h

replay-enums.c: $(enums_in_files) replay-enums.c.template $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template replay-enums.c.template $(enums_in_files)) > $@

replay-enums.h: $(enums_in_files) replay-enums.h.template $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template replay-enums.h.template $(enums_in_files)) > $@

# marshallers
marshallers_in_files = replay-marshallers.list
marshallers_built_files = replay-marshallers.c replay-marshallers.h

replay-marshallers.c: $(marshallers_in_files) $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix replay_marshal --body $< > $@

replay-marshallers.h: $(marshallers_in_files) $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix replay_marshal --header $< > $@

BUILT_SOURCES = $(enums_built_files) $(marshallers_built_files)
EXTRA_DIST = $(enums_templates) $(marshallers_in_files)

noinst_LTLIBRARIES =    libreplay.la
libreplay_la_SOURCES =  replay-event.c \
			replay-event.h \
			replay-node-create-event.c \
			replay-node-create-event.h \
			replay-node-delete-event.c \
			replay-node-delete-event.h \
			replay-node-props-event.c \
			replay-node-props-event.h \
			replay-edge-create-event.c \
			replay-edge-create-event.h \
			replay-edge-delete-event.c \
			replay-edge-delete-event.h \
			replay-edge-props-event.c \
			replay-edge-props-event.h \
			replay-activity-start-event.c \
			replay-activity-start-event.h \
			replay-activity-end-event.c \
			replay-activity-end-event.h \
			replay-msg-send-event.c \
			replay-msg-send-event.h \
			replay-msg-recv-event.c \
			replay-msg-recv-event.h \
			replay-property-store.c \
			replay-property-store.h \
			replay-event-source.c \
			replay-event-source.h \
			replay-event-processor.c \
			replay-event-processor.h \
			replay-task.c \
			replay-task.h \
			replay-file-loader.c \
			replay-file-loader.h \
			replay-network-server.c \
			replay-network-server.h \
			replay-application.c \
			replay-application.h \
			replay-window.c \
			replay-window.h \
			replay-log.c \
			replay-log.h \
			replay-log-window.c \
			replay-log-window.h \
			replay-prefs.c \
			replay-prefs.h \
			replay-timeline-view.c \
			replay-timeline-view.h \
			replay-event-text-view.c \
			replay-event-text-view.h \
			replay-event-graph.c \
			replay-event-graph.h \
			replay-node-tree-view.c \
			replay-node-tree-view.h \
			replay-event-store.c \
			replay-event-store.h \
			replay-timeline.c \
			replay-timeline.h \
			replay-message-tree-view.c \
			replay-message-tree-view.h \
			replay-graph-view.c \
			replay-graph-view.h \
			replay-message-tree.c \
			replay-message-tree.h \
			replay-node-tree.c \
			replay-node-tree.h \
			replay-interval-list.c \
			replay-interval-list.h \
			replay-string-utils.c \
			replay-string-utils.h \
			replay-timestamp.c \
			replay-timestamp.h \
			replay-config.h \
			replay-event-string.c \
			replay-event-string.h \
			replay-application-activatable.c \
			replay-application-activatable.h \
			replay-window-activatable.c \
			replay-window-activatable.h \
			replay-option-activatable.c \
			replay-option-activatable.h

nodist_libreplay_la_SOURCES = $(BUILT_SOURCES)

libreplay_la_LDFLAGS = -export-dynamic -no-undefined
libreplay_la_LIBTOOLFLAGS = --tag=disable-static

replay_LDADD = 	libreplay.la \
		$(REPLAY_LIBS) \
		$(INTROSPECTION_LIBS)

replay_LDFLAGS = -export-dynamic -no-undefined

CLEANFILES = *~ $(BUILT_SOURCES)

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Replay-1.0.gir

Replay-1.0.gir: replay
INTROSPECTION_SCANNER_ARGS = -I$(top_srcdir) --warn-all
Replay_1_0_gir_NAMESPACE = Replay
Replay_1_0_gir_VERSION = 1.0
Replay_1_0_gir_PROGRAM = $(builddir)/replay
Replay_1_0_gir_FILES = $(libreplay_la_SOURCES) $(nodist_libreplay_la_SOURCES)
Replay_1_0_gir_INCLUDES = Gtk-3.0

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif
