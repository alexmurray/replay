/* -*- mode: C; mode: C; c-file-style: "ellemtel"; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Replay Visualisation System
 * Copyright (C) 2012 Commonwealth of Australia
 * Developed by Alex Murray <murray.alex@gmail.com>
 *
 * Replay is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 (GPLv3)
 * (with extensions as allowed under clause 7 of the GPLv3) as
 * published by the Free Software Foundation.
 *
 * Replay is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE
 * file distributed with Replay for more details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libpeas/peas.h>
#include <libpeas-gtk/peas-gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <replay/replay.h>
#include <replay/replay-window.h>
#include <replay/replay-window-activatable.h>
#include <replay/replay-task.h>
#include <replay/replay-file-loader.h>
#include <replay/replay-event.h>
#include <replay/replay-event-processor.h>
#include <replay/replay-event-string.h>
#include <replay/replay-prefs.h>
#include <replay/replay-timeline-view.h>
#include <replay/replay-graph-view.h>
#include <replay/replay-message-tree-view.h>
#include <replay/replay-message-tree.h>
#include <replay/replay-event-text-view.h>
#include <replay/replay-log-window.h>
#include <replay/replay-string-utils.h>

/* inherit from GtkWindow */
G_DEFINE_TYPE(ReplayWindow, replay_window, GTK_TYPE_APPLICATION_WINDOW);

#define ZOOM_FACTOR 1.5

#define WINDOW_MAXIMIZED_KEY "window-maximized"
#define WINDOW_WIDTH_KEY "window-width"
#define WINDOW_HEIGHT_KEY "window-height"
#define HPANED_POSITION_KEY "hpaned-position"
#define VPANED_POSITION_KEY "vpaned-position"
#define TIMELINE_VIEW_HPANED_POSITION_KEY "timeline-hpaned-position"
#define TIMELINE_VIEW_VPANED_POSITION_KEY "timeline-vpaned-position"


/* signals we emit */
enum
{
  REPLAY_EVENT = 0,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

enum {
  PROP_EVENT_STORE = 1,
  PROP_NODE_TREE = 2,
  PROP_GRAPH_VIEW = 3,
  PROP_MESSAGE_TREE_VIEW = 4,
  PROP_EVENT_SOURCE = 5,
  PROP_DISPLAY_FLAGS = 6,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _ReplayWindowPrivate
{
  /* data structures */
  ReplayEventProcessor *event_processor;
  ReplayEventStore *event_store;
  ReplayMessageTree *message_tree;
  ReplayNodeTree *node_tree;
  ReplayIntervalList *interval_list;

  /* replay specific widgets */
  GtkWidget *timeline_view;
  GtkWidget *event_text_view;
  GtkWidget *event_text_window;
  GtkWidget *graph_view;
  GtkWidget *message_tree_view;
  GtkWidget *scrolled_window;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *menubar;
  GtkHBox *hbox;
  GtkWidget *toolbar_box; /* to allow to pack / unpack toolbar for fullscreen
                             auto-hide goodness */
  guint slide_id;
  gint slide;
  GtkWidget *fs_toolbar;
  GtkWidget *toolbar;

  GtkWidget *plugins_dialog;
  ReplayPrefs *prefs;
  GtkWidget *info_bar_box;
  GtkWidget *info_bar;
  GtkWidget *info_bar_title;
  GtkWidget *info_bar_message;

  GtkWidget *spinner;
  guint num_activities;

  /* action groups */
  GtkUIManager *ui_manager;
  GtkActionGroup *global_actions;
  GtkActionGroup *doc_actions;
  GtkAction *recent_action;
  guint recent_ui_id;

  /* store the list of log messages */
  ReplayLog *log;
  GtkWidget *log_window;

  /* for printing */
  GtkPageSetup *page_setup;
  GtkPrintSettings *print_settings;

  GtkFileFilter *custom_filter;

  /* The ReplayEventSource which we are currently listening to for events */
  ReplayEventSource *event_source;

  /* to remember last locations of open and save dialogs */
  gchar *last_open_folder;
  gchar *last_save_folder;

  ReplayWindowDisplayFlags display_flags;
  PeasExtensionSet *set;
};

#define REPLAY_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_WINDOW, ReplayWindowPrivate))

/* function prototypes */
static void task_error_cb(ReplayTask *task,
                          const gchar *error,
                          gpointer user_data);
static void task_progress_cb(ReplayTask *task,
                             gdouble fraction,
                             gpointer user_data);
static void event_source_event_cb(ReplayEventSource *event_source,
                                  ReplayEvent *event,
                                  gpointer user_data);
static void event_source_error_cb(ReplayEventSource *event_source,
                                  const gchar *error,
                                  gpointer user_data);
static void event_source_notify_description_cb(ReplayEventSource *event_source,
                                               GParamSpec *pspec,
                                               gpointer user_data);
static void set_widget_data(ReplayWindow *self);
static void replay_window_dispose(GObject *object);
static void replay_window_finalize(GObject *object);
static void replay_window_drag_data_received(GtkWidget *widget,
                                          GdkDragContext *drag_context,
                                          gint x, gint y,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint event_time);
static gboolean replay_window_delete_event(GtkWidget *window,
                                        GdkEventAny *event);

/* callbacks for ui */
static void open_cb(GtkAction *action,
                    gpointer data);
static void open_recent_cb(GtkRecentChooser *chooser,
                           gpointer data);
static void save_cb(GtkAction *action,
                    gpointer data);
static void close_cb(GtkAction *action,
                     gpointer data);
static void plugins_cb(GtkAction *action,
                     gpointer data);
static void print_cb(GtkAction *action,
                     gpointer data);
static void page_setup_cb(GtkAction *action,
                          gpointer data);
static void toggle_fullscreen_cb(GtkAction *action,
                                 gpointer data);
static void toggle_absolute_time_cb(GtkAction *action,
                                    gpointer data);
static void toggle_label_nodes_cb(GtkAction *action,
                                  gpointer data);
static void toggle_label_edges_cb(GtkAction *action,
                                  gpointer data);
static void toggle_label_messages_cb(GtkAction *action,
                                     gpointer data);
static void zoom_in_cb(GtkAction *action,
                       gpointer data);
static void zoom_out_cb(GtkAction *action,
                        gpointer data);
static void event_text_view_cb(GtkAction *action,
                               gpointer data);
static void log_window_cb(GtkAction *action,
                          gpointer data);

/* global actions that apply to the entire window - any defined callback must
 * take a ReplayWindow data parameter (or NULL) */
static GtkActionEntry global_entries[] =
{
  /* name, stock_id, label, accelerator, tooltip, callback */

  /* File Menu */
  { "FileMenuAction", NULL, N_("_File") },
  { "OpenAction", GTK_STOCK_OPEN, N_("_Open…"), "<control>O",
    N_("Open an file"), G_CALLBACK(open_cb) },
  { "CloseAction", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
    N_("Close this window"), G_CALLBACK(close_cb) },

  /* Edit Menu */
  { "EditMenuAction", NULL, N_("_Edit") },
  { "PluginsAction", GTK_STOCK_PREFERENCES, N_("P_lugins"), NULL,
    N_("Enable and disable plugins"), G_CALLBACK(plugins_cb) },

  /* View Menu */
  { "ViewMenuAction", NULL, N_("_View") },
  { "LogWindowAction", GTK_STOCK_INFO, N_("Message Log"), "<control>L",
    N_("Show log window"), G_CALLBACK(log_window_cb) },
};

static GtkToggleActionEntry global_toggle_entries[] =
{
  { "FullscreenAction", GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), "F11",
    N_("Switch to fullscreen view"), G_CALLBACK(toggle_fullscreen_cb) },
};

/* document actions that apply to the currently loaded event log - any defined
 * callback must take a ReplayWindow data parameter (or NULL) */
static GtkActionEntry doc_entries[] =
{
  { "SaveAsAction", GTK_STOCK_SAVE_AS, N_("Save _As…"), "<shift><control>S",
    N_("Save timeline view"), G_CALLBACK(save_cb) },
  { "PageSetupAction", GTK_STOCK_PAGE_SETUP, N_("Page Set_up…"), NULL,
    N_("Configure printer page setup"), G_CALLBACK(page_setup_cb) },
  { "PrintAction", GTK_STOCK_PRINT, N_("_Print…"), NULL,
    N_("Print the timeline view"), G_CALLBACK(print_cb) },

  /* View Menu Items */
  { "ZoomInAction", GTK_STOCK_ZOOM_IN, N_("Zoom _In"), "<control>plus",
    N_("Increase zoom in timeline view"), G_CALLBACK(zoom_in_cb) },
  { "ZoomOutAction", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus",
    N_("Decrease zoom in timeline view"), G_CALLBACK(zoom_out_cb) },
  { "ShowEventTextViewAction", GTK_STOCK_INFO, N_("Event Source"), "<control>U",
    N_("Show current event source"), G_CALLBACK(event_text_view_cb) },

};

static GtkToggleActionEntry doc_toggle_entries[] =
{
  { "AbsoluteTimeAction", NULL, N_("_Absolute time"), "<control><shift>A",
    N_("Use absolute time values"), G_CALLBACK(toggle_absolute_time_cb) },
  { "LabelNodesAction", NULL, N_("_Label nodes"), "<control><shift>n",
    N_("Label nodes in the graph view"), G_CALLBACK(toggle_label_nodes_cb) },
  { "LabelEdgesAction", NULL, N_("_Label edges"), "<control><shift>e",
    N_("Label edges in the graph view"), G_CALLBACK(toggle_label_edges_cb) },
  { "LabelMessagesAction", NULL, N_("_Label messages"), "<control><shift>m",
    N_("Label messages in the timeline view"),
    G_CALLBACK(toggle_label_messages_cb) },
};

static const gchar * const ui = "<ui>"
                                "<menubar name='MenuBar'>"
                                "<menu name='FileMenu' action='FileMenuAction'>"
                                "<menuitem action='OpenAction'/>"
                                "<placeholder name='OpenRecentAction'/>"
                                "<menuitem action='SaveAsAction'/>"
                                "<separator/>"
                                "<placeholder name='FileOps1' />"
                                "<separator/>"
                                "<menuitem action='PageSetupAction'/>"
                                "<menuitem action='PrintAction'/>"
                                "<menuitem action='CloseAction'/>"
                                "</menu>"
                                "<menu name='EditMenu' action='EditMenuAction'>"
                                "<placeholder name='EditOps1' />"
                                "<separator/>"
                                "<menuitem action='PluginsAction'/>"
                                "</menu>"
                                "<menu name='ViewMenu' action='ViewMenuAction'>"
                                "<menuitem action='FullscreenAction'/>"
                                "<menuitem action='ZoomInAction'/>"
                                "<menuitem action='ZoomOutAction'/>"
                                "<separator/>"
                                "<menuitem action='LogWindowAction'/>"
                                "<menuitem action='ShowEventTextViewAction'/>"
                                "<separator/>"
                                "<menuitem action='AbsoluteTimeAction'/>"
                                "<menuitem action='LabelNodesAction'/>"
                                "<menuitem action='LabelEdgesAction'/>"
                                "<menuitem action='LabelMessagesAction'/>"
                                "</menu>"
                                "</menubar>"
                                "<toolbar name='ToolBar'>"
                                "<toolitem name='OpenToolItem' action='OpenAction'/>"
                                "<placeholder name='ToolItemOps1'/>"
                                "</toolbar>"
                                "</ui>";

static void replay_window_display_error(ReplayWindow *self,
                                     const gchar *title,
                                     const gchar *message)
{
  ReplayWindowPrivate *priv;

  priv = self->priv;

  gtk_label_set_markup(GTK_LABEL(priv->info_bar_title), title);
  gtk_label_set_markup(GTK_LABEL(priv->info_bar_message), message);
  gtk_widget_show(priv->info_bar);
}

static void
replay_window_get_property(GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec)
{
  ReplayWindow *self = REPLAY_WINDOW(object);

  switch (prop_id) {
    case PROP_EVENT_STORE:
      g_value_set_object(value, replay_window_get_event_store(self));
      break;

    case PROP_NODE_TREE:
      g_value_set_object(value, replay_window_get_node_tree(self));
      break;

    case PROP_GRAPH_VIEW:
      g_value_set_object(value, replay_window_get_graph_view(self));
      break;

    case PROP_MESSAGE_TREE_VIEW:
      g_value_set_object(value, replay_window_get_message_tree_view(self));
      break;

    case PROP_EVENT_SOURCE:
      g_value_set_object(value, replay_window_get_event_source(self));
      break;

    case PROP_DISPLAY_FLAGS:
      g_value_set_int(value, replay_window_get_display_flags(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_window_set_property(GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
  ReplayWindow *self = REPLAY_WINDOW(object);

  switch (prop_id) {
    case PROP_EVENT_SOURCE:
      replay_window_set_event_source(self, REPLAY_EVENT_SOURCE(g_value_get_object(value)));
      break;

    case PROP_DISPLAY_FLAGS:
      replay_window_set_display_flags(self, g_value_get_int(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
display_help(ReplayWindow *self, const gchar *topic)
{
        ReplayApplication *app = REPLAY_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));
        GError *error = NULL;
        gboolean ret;

        ret = replay_application_display_help(app, "replay-plugins", &error);
        if (!ret) {
                replay_window_display_error(self,
                                            "Error displaying help window",
                                            error->message);
                g_clear_error(&error);
        }
}


static void plugins_dialog_response_cb(GtkDialog *dialog,
                                       gint response_id,
                                       gpointer user_data)
{
        g_return_if_fail(REPLAY_IS_WINDOW(user_data));

        if (response_id == GTK_RESPONSE_HELP) {
                display_help(REPLAY_WINDOW(user_data), "replay-plugins");
        } else {
                gtk_widget_hide(GTK_WIDGET(dialog));
        }
}

static gboolean replay_window_state_event(GtkWidget *widget,
                                       GdkEventWindowState *event)
{
  ReplayWindowPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_WINDOW(widget), FALSE);

  priv = REPLAY_WINDOW(widget)->priv;

  if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
  {
    GSettings *settings;
    gboolean maximized;

    settings = g_settings_new("au.gov.defence.dsto.parallax.replay");
    maximized = ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) ?
                 TRUE : FALSE);

    g_settings_set_boolean(settings, WINDOW_MAXIMIZED_KEY, maximized);
    g_object_unref(settings);
  }
  else if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
  {
    GtkAction *fullscreen_action;
    gboolean fullscreen;

    /* track new value in toggle action - make sure only has values TRUE or
     * FALSE */
    fullscreen = ((event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) ?
                  TRUE : FALSE);
    fullscreen_action = gtk_action_group_get_action(priv->global_actions,
                                                    "FullscreenAction");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(fullscreen_action),
                                 fullscreen);
  }
  return TRUE;
}

static void update_doc_action_sense(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;
  gint n;

  priv = self->priv;

  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->event_store),
                                     NULL);
  gtk_action_group_set_sensitive(priv->doc_actions, n > 0);
}

static void replay_window_replay_event(ReplayWindow *self,
                                 ReplayEvent *event)
{
  ReplayWindowPrivate *priv;
  gboolean ret = FALSE;
  GError *error = NULL;
  GtkTreeIter current;

  g_return_if_fail(REPLAY_IS_WINDOW(self));
  priv = self->priv;

  ret = replay_event_processor_process_event(priv->event_processor, event,
                                          &error);
  if (!ret)
  {
    gchar *title, *string, *esc_string, *esc_error, *message;

    g_assert(error != NULL);

    title = g_strdup_printf(_("Error processing %s event"),
                            G_OBJECT_TYPE_NAME(event));
    string = replay_event_to_string(event,
                                    (replay_prefs_get_absolute_time(priv->prefs) ?
                                     0 :
                                     replay_event_store_get_start_timestamp(priv->event_store)));
    /* the error itself may contain markup-like stuff so let's escape it to make
       sure it doesn't cause problems */
    esc_string = g_markup_escape_text(string, -1);
    g_free(string);

    esc_error = g_markup_escape_text(error->message, -1);
    message = g_strdup_printf("%s\n"
                              "\n"
                              "<span face='monospace'>%s</span>",
                              esc_error,
                              esc_string);

    g_free(esc_error);
    g_free(esc_string);

    replay_log_error(priv->log, "Event Processor", title, message);

    g_free(title);
    g_free(message);

    g_error_free(error);
    goto out;
  }

  /* set current if not already set to first event */
  if (!replay_event_store_get_current(priv->event_store, &current))
  {
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->event_store),
                                  &current);
    replay_event_store_set_current(priv->event_store, &current);
    /* we have a valid event - set actions active so user can interact */
    update_doc_action_sense(self);
  }

out:
  return;
}


/*!
 * GObject related stuff
 */
static void replay_window_class_init(ReplayWindowClass *klass)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  obj_class->dispose = replay_window_dispose;
  obj_class->finalize = replay_window_finalize;
  obj_class->get_property = replay_window_get_property;
  obj_class->set_property = replay_window_set_property;
  widget_class->drag_data_received = replay_window_drag_data_received;
  widget_class->window_state_event = replay_window_state_event;
  widget_class->delete_event = replay_window_delete_event;
  klass->replay_event = replay_window_replay_event;

  properties[PROP_EVENT_STORE] = g_param_spec_object("event-store",
                                                     "event-store",
                                                     "The #ReplayEventStore of the window",
                                                     REPLAY_TYPE_EVENT_STORE,
                                                     G_PARAM_READABLE);
  g_object_class_install_property(obj_class, PROP_EVENT_STORE, properties[PROP_EVENT_STORE]);
  properties[PROP_NODE_TREE] = g_param_spec_object("node-tree",
                                                   "node-tree",
                                                   "The #ReplayNodeTree of the window",
                                                   REPLAY_TYPE_NODE_TREE,
                                                   G_PARAM_READABLE);
  g_object_class_install_property(obj_class, PROP_NODE_TREE, properties[PROP_NODE_TREE]);
  properties[PROP_GRAPH_VIEW] = g_param_spec_object("graph-view",
                                                   "graph-view",
                                                   "The #ReplayGraphView of the window",
                                                   REPLAY_TYPE_GRAPH_VIEW,
                                                   G_PARAM_READABLE);
  g_object_class_install_property(obj_class, PROP_GRAPH_VIEW, properties[PROP_GRAPH_VIEW]);
  properties[PROP_MESSAGE_TREE_VIEW] = g_param_spec_object("message-tree-view",
                                                           "message-tree-view",
                                                           "The #ReplayMessageTreeView of the window",
                                                           REPLAY_TYPE_MESSAGE_TREE_VIEW,
                                                           G_PARAM_READABLE);
  g_object_class_install_property(obj_class, PROP_MESSAGE_TREE_VIEW, properties[PROP_MESSAGE_TREE_VIEW]);
  properties[PROP_EVENT_SOURCE] = g_param_spec_object("event-source",
                                                      "event-source",
                                                      "The #ReplayEventSource of the window",
                                                      REPLAY_TYPE_EVENT_SOURCE,
                                                      G_PARAM_READWRITE);
  g_object_class_install_property(obj_class, PROP_EVENT_SOURCE, properties[PROP_EVENT_SOURCE]);
  properties[PROP_DISPLAY_FLAGS] = g_param_spec_int("display-flags",
                                                    "display-flags",
                                                    "The #ReplayDisplayFlags of the window",
                                                    REPLAY_WINDOW_DISPLAY_MESSAGE_TREE,
                                                    REPLAY_WINDOW_DISPLAY_ALL,
                                                    REPLAY_WINDOW_DISPLAY_ALL,
                                                    G_PARAM_READWRITE);
  g_object_class_install_property(obj_class, PROP_DISPLAY_FLAGS, properties[PROP_DISPLAY_FLAGS]);

  /* install class signals */
  /**
   * ReplayWindow::event:
   * @self: The window emitting this event
   * @event: The new event
   *
   * Emitted by a #ReplayWindow when it receives a new event from the currently
   * active #ReplayEventSource
   */
  signals[REPLAY_EVENT] = g_signal_new("replay-event",
                                    G_OBJECT_CLASS_TYPE(obj_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET(ReplayWindowClass,
                                                    replay_event),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__BOXED,
                                    G_TYPE_NONE,
                                    1,
                                    REPLAY_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayWindowPrivate));
}

static void reset_custom_file_filters(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;
  ReplayApplication *app;
  GtkRecentFilter *recent_filter;
  const GList *list;

  priv = self->priv;

  app = REPLAY_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));

  if (!app) {
    /* we may not yet be registered with an application */
    return;
  }

  if (priv->custom_filter)
  {
    g_object_unref(priv->custom_filter);
    priv->custom_filter = NULL;
  }
  priv->custom_filter = gtk_file_filter_new();
  priv->custom_filter = g_object_ref_sink(priv->custom_filter);

  gtk_file_filter_set_name(priv->custom_filter,
                           _("All supported file types"));

  /* also do recent filter for recent files - need to recreate the
   * recent_action itself since just updating the filter on the action doesn't
   * cause it to refilter */
  if (priv->recent_action)
  {
    gtk_action_group_remove_action(priv->global_actions, priv->recent_action);
    g_object_unref(priv->recent_action);
    gtk_ui_manager_remove_ui(priv->ui_manager,
                             priv->recent_ui_id);
    gtk_ui_manager_ensure_update(priv->ui_manager);
  }

  recent_filter = gtk_recent_filter_new();
  gtk_recent_filter_set_name(recent_filter,
                             _("Recently opened files"));

  for (list = replay_application_get_file_loaders(app);
       list != NULL;
       list = list->next)
  {
    ReplayFileLoader *loader = REPLAY_FILE_LOADER(list->data);
    const gchar *mime_type, *pattern;

    mime_type = replay_file_loader_get_mime_type(loader);
    if (mime_type)
    {
      gtk_file_filter_add_mime_type(priv->custom_filter, mime_type);
      gtk_recent_filter_add_mime_type(recent_filter, mime_type);
    }
    pattern = replay_file_loader_get_pattern(loader);
    if (pattern)
    {
      gtk_file_filter_add_pattern(priv->custom_filter, pattern);
      gtk_recent_filter_add_pattern(recent_filter, pattern);
    }
  }

  /* create a recent action to global actions */
  priv->recent_action = gtk_recent_action_new("OpenRecentAction",
                                              _("Open Recent"),
                                              _("Open a recent file"),
                                              GTK_STOCK_OPEN);

  gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(priv->recent_action),
                                recent_filter);
  g_signal_connect(priv->recent_action, "item-activated",
                   G_CALLBACK(open_recent_cb), self);

  gtk_action_group_add_action_with_accel(priv->global_actions,
                                         priv->recent_action,
                                         "");
  priv->recent_ui_id = gtk_ui_manager_new_merge_id(priv->ui_manager);

  gtk_ui_manager_add_ui(priv->ui_manager,
                        priv->recent_ui_id,
                        "/MenuBar/FileMenu/OpenRecentAction",
                        "OpenRecent",
                        "OpenRecentAction",
                        GTK_UI_MANAGER_MENUITEM,
                        TRUE);
}

static void show_activity(ReplayWindow *self, gboolean active)
{
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(self));

  priv = self->priv;

  if (active)
  {
    priv->num_activities++;
    if (priv->num_activities == 1)
    {
      gtk_spinner_start(GTK_SPINNER(priv->spinner));
      gtk_widget_set_visible(priv->spinner, TRUE);
    }
  }
  else
  {
    g_assert(priv->num_activities > 0);
    priv->num_activities--;
    /* spinner could be NULL during dispose cycle */
    if (!priv->num_activities && priv->spinner != NULL)
    {
      gtk_spinner_stop(GTK_SPINNER(priv->spinner));
      gtk_widget_set_visible(priv->spinner, FALSE);
    }
  }
}

static void
info_bar_response_cb(GtkInfoBar *info_bar,
                     gint response_id,
                     gpointer data)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);
  priv = self->priv;

  switch (response_id)
  {
    case GTK_RESPONSE_ACCEPT:
      gtk_window_present(GTK_WINDOW(priv->log_window));
      break;

    default:
      break;
  }
  gtk_widget_hide(GTK_WIDGET(info_bar));
}


static void
on_extension_added(PeasExtensionSet *set,
                   PeasPluginInfo *info,
                   PeasExtension *exten,
                   ReplayWindow *window)
{
  replay_window_activatable_activate(REPLAY_WINDOW_ACTIVATABLE(exten));
}

static void
on_extension_removed(PeasExtensionSet *set,
                     PeasPluginInfo *info,
                     PeasExtension *exten,
                     ReplayWindow *window)
{
  replay_window_activatable_deactivate(REPLAY_WINDOW_ACTIVATABLE(exten));
}

static void
log_appended_cb(ReplayLog *log,
                ReplayLogLevel level,
                const gchar *time_,
                const gchar *source,
                const gchar *brief,
                const gchar *details,
                gpointer user_data)
{
  ReplayWindow *self;

  g_return_if_fail(REPLAY_IS_WINDOW(user_data));

  self = REPLAY_WINDOW(user_data);

  if (level == REPLAY_LOG_LEVEL_ERROR)
  {
    replay_window_display_error(self, brief, details);
  }
}


static void replay_window_restore_layout(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;
  GSettings *settings;
  int position;
  gboolean maximized;
  gboolean info_bar_visible;

  priv = self->priv;

  settings = g_settings_new("au.gov.defence.dsto.parallax.replay");

  /* first hide error bar if visible */
  info_bar_visible = gtk_widget_get_visible(priv->info_bar);
  if (info_bar_visible)
  {
    gtk_widget_hide(priv->info_bar);
  }

  gtk_window_resize(GTK_WINDOW(self),
                    g_settings_get_int(settings, WINDOW_WIDTH_KEY),
                    g_settings_get_int(settings, WINDOW_HEIGHT_KEY));
  maximized = g_settings_get_boolean(settings, WINDOW_MAXIMIZED_KEY);
  if (maximized)
  {
    gtk_window_maximize(GTK_WINDOW(self));
  }
  else
  {
    gtk_window_unmaximize(GTK_WINDOW(self));
  }

  position = g_settings_get_int(settings, HPANED_POSITION_KEY);
  if (position)
  {
    gtk_paned_set_position(GTK_PANED(priv->hpaned), position);
  }

  position = g_settings_get_int(settings, VPANED_POSITION_KEY);
  if (position)
  {
    gtk_paned_set_position(GTK_PANED(priv->vpaned), position);
  }

  position = g_settings_get_int(settings, TIMELINE_VIEW_HPANED_POSITION_KEY);
  if (position)
  {
    replay_timeline_view_set_hpaned_position(REPLAY_TIMELINE_VIEW(priv->timeline_view),
                                          position);
  }
  position = g_settings_get_int(settings, TIMELINE_VIEW_VPANED_POSITION_KEY);
  if (position)
  {
    replay_timeline_view_set_vpaned_position(REPLAY_TIMELINE_VIEW(priv->timeline_view),
                                          position);
  }

  g_object_unref(settings);

  if (info_bar_visible)
  {
    gtk_widget_show_all(priv->info_bar);
  }
}

static gboolean
slide_toolbar(ReplayWindow *self)
{
  ReplayWindowPrivate *priv = self->priv;
  GtkWindow *fs_toolbar = GTK_WINDOW(priv->fs_toolbar);
  gboolean ret = FALSE;
  gint min_y, target_y, x, y;

  /* should be set if this is called */
  g_assert(priv->slide);

  /* should be positioned at a negative index to hide and 0 to show */
  gtk_window_get_size(fs_toolbar, NULL, &min_y);
  min_y = 1 - min_y;

  gtk_window_get_position(fs_toolbar, &x, &y);
  target_y = MAX(min_y, MIN(0, y + priv->slide));
  if (target_y > min_y) {
    gtk_window_move(fs_toolbar, x, target_y);
    ret = TRUE;
  } else {
    g_source_remove(priv->slide_id);
    priv->slide_id = 0;
    priv->slide = 0;
  }
  return ret;
}

static void
set_slide_timeout(ReplayWindow *self)
{
  ReplayWindowPrivate *priv = self->priv;

  if (priv->slide_id) {
    g_source_remove(priv->slide_id);
  }
  priv->slide_id = g_timeout_add(20, (GSourceFunc)slide_toolbar, self);
}

static gboolean
slide_fs_toolbar_in(GtkWidget *widget,
                    GdkEvent *event,
                    ReplayWindow *self)
{
  ReplayWindowPrivate *priv = self->priv;

  priv->slide = 5;

  set_slide_timeout(self);
  return FALSE;
}

static gboolean
slide_fs_toolbar_out(GtkWidget *widget,
                     GdkEvent *event,
                     ReplayWindow *self)
{
  ReplayWindowPrivate *priv = self->priv;

  priv->slide = -5;

  set_slide_timeout(self);
  return FALSE;
}

static void
update_action_from_preference(ReplayWindow *self,
                              const gchar *preference)
{
        ReplayWindowPrivate *priv;
        GtkAction *action;
        guint i;
        gchar **tokens;
        gchar *action_name;

        priv = self->priv;

        /* we name actions as "PropertyNameAction" so find the action matching
           this property and toggle it */
        tokens = g_strsplit(preference, "-", 0);
        /* uppercase all the first letters of tokens */
        for (i = 0; i < g_strv_length(tokens); i++) {
                tokens[i][0] = g_ascii_toupper(tokens[i][0]);
        }
        action_name = replay_strconcat_and_free(g_strjoinv("", tokens),
                                                g_strdup("Action"));
        g_strfreev(tokens);
        action = gtk_action_group_get_action(priv->doc_actions,
                                             action_name);
        if (action) {
                gboolean value;

                g_object_get(priv->prefs, preference, &value, NULL);
                g_assert(GTK_IS_TOGGLE_ACTION(action));
                gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), value);
        } else {
                g_warning("No matching action %s for preference %s",
                          action_name, preference);
        }
}

static void
prefs_notify_cb(ReplayPrefs *prefs,
                GParamSpec *pspec,
                ReplayWindow *self)
{
        if (pspec->value_type == G_TYPE_BOOLEAN) {
                update_action_from_preference(self, pspec->name);
        }
}

static void
window_notify_application_cb(ReplayWindow *self)
{
  ReplayApplication *app;
  /* reset custom file filters when available file loaders are changed */
  reset_custom_file_filters(self);
  app = REPLAY_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));
  if (app) {
    g_signal_connect_swapped(app, "file-loader-added",
                             G_CALLBACK(reset_custom_file_filters),
                             self);
    g_signal_connect_swapped(app, "file-loader-removed",
                             G_CALLBACK(reset_custom_file_filters),
                             self);
  }
}

static void replay_window_init(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;
  GtkWidget *scrolled_window;
  GtkAccelGroup *accel_group;
  GtkWidget *vbox;
  GError *error = NULL;
  GtkWidget *menu_item;
  GtkWidget *info_vbox;
  GtkWidget *info_hbox;
  GtkWidget *info_icon;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  PeasEngine *engine;
  GdkScreen *screen;
  GtkRequisition req;
  GtkWidget *plugin_manager;
  GtkWidget *content_area;

  g_return_if_fail(REPLAY_IS_WINDOW(self));

  self->priv = priv = REPLAY_WINDOW_GET_PRIVATE(self);

  priv->event_store = NULL;
  priv->event_processor = NULL;
  priv->message_tree = NULL;
  priv->node_tree = NULL;
  priv->interval_list = NULL;

  priv->page_setup = NULL;
  priv->print_settings = NULL;
  priv->last_open_folder = g_strdup(g_get_current_dir());
  priv->last_save_folder = g_strdup(g_get_current_dir());
  priv->display_flags = REPLAY_WINDOW_DISPLAY_ALL;

  /* set up for drag and drop - can drop uri to load a file */
  gtk_drag_dest_set(GTK_WIDGET(self),
                    GTK_DEST_DEFAULT_ALL,
                    NULL, 0,
                    GDK_ACTION_COPY);

  gtk_drag_dest_add_uri_targets(GTK_WIDGET(self));

  /* setup actions and action groups */
  priv->global_actions = gtk_action_group_new("GlobalActions");
  gtk_action_group_set_translation_domain(priv->global_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->global_actions, global_entries,
                               G_N_ELEMENTS(global_entries),
                               self);
  gtk_action_group_add_toggle_actions(priv->global_actions,
                                      global_toggle_entries,
                                      G_N_ELEMENTS(global_toggle_entries),
                                      self);

  priv->doc_actions = gtk_action_group_new("DocActions");
  gtk_action_group_set_translation_domain(priv->doc_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->doc_actions, doc_entries,
                               G_N_ELEMENTS(doc_entries),
                               self);
  gtk_action_group_add_toggle_actions(priv->doc_actions,
                                      doc_toggle_entries,
                                      G_N_ELEMENTS(doc_toggle_entries),
                                      self);

  /* some actions are more important than others */
  gtk_action_set_is_important(gtk_action_group_get_action(priv->global_actions,
                                                          "OpenAction"),
                              TRUE);

  /* create child contents of main window */
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(self), vbox);

  priv->ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group(priv->ui_manager, priv->global_actions, 0);
  gtk_ui_manager_insert_action_group(priv->ui_manager, priv->doc_actions, 0);
  accel_group = gtk_ui_manager_get_accel_group(priv->ui_manager);
  gtk_window_add_accel_group(GTK_WINDOW(self), accel_group);


  if (!gtk_ui_manager_add_ui_from_string(priv->ui_manager, ui, -1,
                                         &error))
  {
    g_error("error creating window ui from string: '%s': %s", ui,
            error->message);
  }

  /* pack into main vbox */
  priv->menubar = gtk_ui_manager_get_widget(priv->ui_manager, "/MenuBar");

  gtk_box_pack_start(GTK_BOX(vbox), priv->menubar,
                     FALSE, TRUE, 0);

  /* create and pack spinner at end of menu bar */
  priv->spinner = gtk_spinner_new();
  /* manually show and hide */
  gtk_widget_set_no_show_all(priv->spinner, TRUE);
  menu_item = gtk_menu_item_new();
  gtk_widget_set_halign(menu_item, GTK_ALIGN_END);
  gtk_container_add(GTK_CONTAINER(menu_item), priv->spinner);
  gtk_menu_shell_append(GTK_MENU_SHELL(priv->menubar), menu_item);
  /* make sure our pointer to spinner gets NULL'd once it is destroyed so that
   * we don't try to interact with it when is dead */
  g_object_add_weak_pointer(G_OBJECT(priv->spinner),
                            (gpointer *)&priv->spinner);

  priv->toolbar = gtk_ui_manager_get_widget(priv->ui_manager, "/ToolBar");
  priv->toolbar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(priv->toolbar_box), priv->toolbar);
  gtk_box_pack_start(GTK_BOX(vbox), priv->toolbar_box,
                     FALSE, TRUE, 0);

  /* also create window for doing auto-hide toolbar */
  priv->fs_toolbar = gtk_window_new(GTK_WINDOW_POPUP);
  screen = gtk_window_get_screen(GTK_WINDOW(self));
  gtk_widget_get_preferred_size(priv->toolbar, NULL, &req);
  gtk_window_resize(GTK_WINDOW(priv->fs_toolbar),
                    gdk_screen_get_width(screen),
                    req.height);
  gtk_window_set_transient_for(GTK_WINDOW(priv->fs_toolbar), GTK_WINDOW(self));
  g_signal_connect(priv->fs_toolbar, "enter-notify-event",
                   G_CALLBACK(slide_fs_toolbar_in), self);
  g_signal_connect(priv->fs_toolbar, "leave-notify-event",
                   G_CALLBACK(slide_fs_toolbar_out), self);

  priv->info_bar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  /* create info bar */
  priv->info_bar = gtk_info_bar_new_with_buttons(_("View Message Log"),
                                                 GTK_RESPONSE_ACCEPT,
                                                 _("Ignore"),
                                                 GTK_RESPONSE_REJECT,
                                                 NULL);
  /* make sure our pointer to info_bar gets NULL'd once it is destroyed so that
   * we don't try to interact with it when is dead */
  g_object_add_weak_pointer(G_OBJECT(priv->info_bar),
                            (gpointer *)&priv->info_bar);
  /* don't show by default - only show when we have an error or similar to show
   * to user */
  gtk_widget_set_no_show_all(priv->info_bar, TRUE);

  /* Set a large, bold title */
  priv->info_bar_title = gtk_label_new("Error Title");
  gtk_misc_set_alignment(GTK_MISC(priv->info_bar_title), 0.0, 0.0);
  attr_list = pango_attr_list_new();
  attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  pango_attr_list_insert(attr_list, attr);
  gtk_label_set_attributes(GTK_LABEL(priv->info_bar_title), attr_list);
  pango_attr_list_unref(attr_list);
  gtk_widget_show(priv->info_bar_title);
  priv->info_bar_message = gtk_label_new("Error Message");
  gtk_misc_set_alignment(GTK_MISC(priv->info_bar_message), 0.0, 0.0);
  gtk_widget_show(priv->info_bar_message);

  info_icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_ERROR,
                                       GTK_ICON_SIZE_DIALOG);
  gtk_widget_show(info_icon);

  info_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_show(info_vbox);
  gtk_box_pack_start(GTK_BOX(info_vbox), priv->info_bar_title, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(info_vbox), priv->info_bar_message, TRUE, TRUE, 0);

  info_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_show(info_hbox);
  gtk_box_pack_start(GTK_BOX(info_hbox), info_icon, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(info_hbox), info_vbox, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(gtk_info_bar_get_content_area(GTK_INFO_BAR(priv->info_bar))),
                    info_hbox);
  g_signal_connect(priv->info_bar, "response", G_CALLBACK(info_bar_response_cb),
                   self);
  gtk_box_pack_start(GTK_BOX(priv->info_bar_box), priv->info_bar,
                     FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), priv->info_bar_box,
                     FALSE, TRUE, 0);
  gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar),
                                GTK_MESSAGE_ERROR);

  /* now create other widgets */
  priv->hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  priv->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  priv->message_tree_view = replay_message_tree_view_new();
  gtk_container_add(GTK_CONTAINER(priv->scrolled_window), priv->message_tree_view);
  gtk_paned_pack1(GTK_PANED(priv->hpaned), priv->scrolled_window, FALSE, TRUE);
  priv->graph_view = replay_graph_view_new();
  g_signal_connect_swapped(priv->graph_view, "processing",
                           G_CALLBACK(show_activity), self);
  gtk_paned_pack2(GTK_PANED(priv->hpaned), priv->graph_view, TRUE, TRUE);

  priv->vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  priv->timeline_view = replay_timeline_view_new();
  g_signal_connect_swapped(priv->timeline_view, "rendering",
                           G_CALLBACK(show_activity), self);
  gtk_paned_pack1(GTK_PANED(priv->vpaned), priv->timeline_view, FALSE, TRUE);
  gtk_paned_pack2(GTK_PANED(priv->vpaned), priv->hpaned, TRUE, TRUE);

  gtk_box_pack_start(GTK_BOX(vbox), priv->vpaned,
                     TRUE, TRUE, 0);

  /* set all actions as insensitive */
  gtk_action_group_set_sensitive(priv->doc_actions, FALSE);

  gtk_window_set_icon_name(GTK_WINDOW(self), "replay");
  gtk_window_resize(GTK_WINDOW(self), 1600, 1060);

  priv->event_text_view = replay_event_text_view_new();
  scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scrolled_window), priv->event_text_view);

  priv->event_text_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(priv->event_text_window),
                       _("Replay Event Source"));
  /* make sure window is only hidden not destroyed */
  g_signal_connect(priv->event_text_window, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  gtk_window_resize(GTK_WINDOW(priv->event_text_window), 600, 400);
  gtk_container_add(GTK_CONTAINER(priv->event_text_window), scrolled_window);
  gtk_widget_show_all(scrolled_window);

  priv->log = replay_log_new();
  g_signal_connect(priv->log, "appended",
                   G_CALLBACK(log_appended_cb), self);
  priv->log_window = replay_log_window_new(priv->log);
  /* let log window hold the reference to the log */
  g_object_unref(priv->log);
  /* make sure window is only hidden not destroyed */
  g_signal_connect(priv->log_window, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  /* init prefs */
  priv->prefs = replay_prefs_new(REPLAY_TIMELINE_VIEW(priv->timeline_view),
                                 REPLAY_EVENT_TEXT_VIEW(priv->event_text_view),
                                 REPLAY_MESSAGE_TREE_VIEW(priv->message_tree_view),
                                 REPLAY_GRAPH_VIEW(priv->graph_view));

  /* set state of actions based on preference values */
  update_action_from_preference(self, "absolute-time");
  update_action_from_preference(self, "label-nodes");
  update_action_from_preference(self, "label-edges");
  update_action_from_preference(self, "label-messages");

  g_signal_connect(priv->prefs, "notify", G_CALLBACK(prefs_notify_cb), self);

  /* get dialogs and connect response signals */
  priv->plugins_dialog = gtk_dialog_new_with_buttons(_("Replay Plugins"),
                                                     GTK_WINDOW(self),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                     GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                     NULL);
  gtk_window_resize(GTK_WINDOW(priv->plugins_dialog), 400, 500);

  /* plugins with the default engine */
  plugin_manager = peas_gtk_plugin_manager_new(NULL);
  content_area = gtk_dialog_get_content_area(GTK_DIALOG(priv->plugins_dialog));

  gtk_box_pack_start(GTK_BOX(content_area), plugin_manager, TRUE, TRUE, 0);
  gtk_widget_show_all(plugin_manager);
  g_signal_connect(priv->plugins_dialog, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(priv->plugins_dialog, "response",
                   G_CALLBACK(plugins_dialog_response_cb), self);


  /* create custom file filter which combines attributes of all registered file
   * loaders for our application when application is set */
  g_signal_connect(self, "notify::application",
                   G_CALLBACK(window_notify_application_cb), NULL);

  /* give focus to graph view by default */
  gtk_window_set_focus(GTK_WINDOW(self), priv->graph_view);

  /* create extension set of all replay window activatables and set window */
  engine = peas_engine_get_default();
  priv->set = peas_extension_set_new(engine, REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                     "window", self, NULL);

  /* activate all activatable extensions */
  peas_extension_set_foreach(priv->set,
                             (PeasExtensionSetForeachFunc)on_extension_added,
                             self);

  /* and make sure to activate any ones which are found in the future */
  g_signal_connect(priv->set, "extension-added",
                   G_CALLBACK(on_extension_added), self);
  g_signal_connect(priv->set, "extension-removed",
                   G_CALLBACK(on_extension_removed), self);

  replay_window_restore_layout(self);
}

#define REPLAY_FILE_LOADER_TASK_KEY "ReplayFileLoaderTaskKey"

static void replay_window_cleanup(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(self));

  priv = self->priv;

  /* set all doc actions as insensitive */
  gtk_action_group_set_sensitive(priv->doc_actions, FALSE);

  if (priv->info_bar)
  {
    gtk_widget_hide(priv->info_bar);
  }

  replay_window_set_event_source(self, NULL);

  if (priv->event_processor)
  {
    g_signal_handlers_disconnect_by_func(priv->event_processor,
                                         event_source_event_cb,
                                         self);
    g_object_unref(priv->event_processor);
    priv->event_processor = NULL;
  }

  if (priv->event_store)
  {
    g_assert(priv->node_tree && priv->message_tree && priv->interval_list);

    /* unset data from display widgets before we unref them */
    replay_graph_view_unset_data(REPLAY_GRAPH_VIEW(priv->graph_view));
    replay_timeline_view_unset_data(REPLAY_TIMELINE_VIEW(priv->timeline_view));
    replay_event_text_view_unset_data(REPLAY_EVENT_TEXT_VIEW(priv->event_text_view));
    replay_message_tree_view_unset_data(REPLAY_MESSAGE_TREE_VIEW(priv->message_tree_view));

    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_NODE_TREE]);

    g_object_unref(priv->message_tree);
    priv->message_tree = NULL;

    g_object_unref(priv->interval_list);
    priv->interval_list = NULL;

    g_object_unref(priv->event_store);
    priv->event_store = NULL;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_EVENT_STORE]);
  }

  replay_window_set_display_flags(self, REPLAY_WINDOW_DISPLAY_ALL);
}

static void replay_window_save_layout(ReplayWindow *self)
{
  ReplayWindowPrivate *priv;
  GSettings *settings;
  int position;
  GtkAction *fullscreen_action;
  gboolean fullscreen;
  gboolean info_bar_visible;

  priv = self->priv;

  /* first hide error bar if visible */
  info_bar_visible = gtk_widget_get_visible(priv->info_bar);
  if (info_bar_visible)
  {
    gtk_widget_hide(priv->info_bar);
  }

  settings = g_settings_new("au.gov.defence.dsto.parallax.replay");

  /* save width and height if not fullscreen as otherwise we get quite
   * distorted values */
  fullscreen_action = gtk_action_group_get_action(priv->global_actions,
                                                  "FullscreenAction");
  fullscreen = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(fullscreen_action));
  if (!fullscreen)
  {
    int width, height;
    gtk_window_get_size(GTK_WINDOW(self), &width, &height);

    g_settings_set_int(settings, WINDOW_WIDTH_KEY, width);
    g_settings_set_int(settings, WINDOW_HEIGHT_KEY, height);
  }

  position = gtk_paned_get_position(GTK_PANED(priv->hpaned));
  g_settings_set_int(settings,
                     HPANED_POSITION_KEY,
                     position);
  position = gtk_paned_get_position(GTK_PANED(priv->vpaned));
  g_settings_set_int(settings,
                     VPANED_POSITION_KEY,
                     position);

  /* save properties for timeline view as well */
  position = replay_timeline_view_get_hpaned_position(REPLAY_TIMELINE_VIEW(priv->timeline_view));
  g_settings_set_int(settings,
                     TIMELINE_VIEW_HPANED_POSITION_KEY,
                     position);
  position = replay_timeline_view_get_vpaned_position(REPLAY_TIMELINE_VIEW(priv->timeline_view));
  g_settings_set_int(settings,
                     TIMELINE_VIEW_VPANED_POSITION_KEY,
                     position);
  g_object_unref(settings);
  if (info_bar_visible)
  {
    gtk_widget_show_all(priv->info_bar);
  }
}

static gboolean replay_window_delete_event(GtkWidget *widget,
                                        GdkEventAny *event)
{
  ReplayWindow *self;

  g_return_val_if_fail(REPLAY_IS_WINDOW(widget), FALSE);

  self = REPLAY_WINDOW(widget);

  replay_window_save_layout(self);

  replay_window_cleanup(self);

  /* destroy ourself */
  gtk_widget_destroy(widget);
  /* would chain up and call parent delete_event but it is NULL for GtkWindow */
  return TRUE;
}

static void replay_window_finalize(GObject *object)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(object));

  self = REPLAY_WINDOW(object);
  priv = self->priv;

  g_object_unref(priv->ui_manager);

  g_object_unref(priv->prefs);

  g_free(priv->last_open_folder);
  priv->last_open_folder = NULL;
  g_free(priv->last_save_folder);
  priv->last_save_folder = NULL;

  gtk_widget_destroy(priv->log_window);
  gtk_widget_destroy(priv->event_text_window);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_window_parent_class)->finalize(object);
}

static void replay_window_dispose(GObject *object)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(object));

  self = REPLAY_WINDOW(object);
  priv = self->priv;

  /* unload all our plugins */
  g_clear_object(&priv->set);
  replay_window_cleanup(self);

  /* call parent dispose */
  G_OBJECT_CLASS(replay_window_parent_class)->dispose(object);
}

typedef struct
{
  ReplayTask *task;
  ReplayWindow *self;
  GtkWidget *info_bar;
  GtkWidget *progress_bar;
} TaskData;

static void task_error_cb(ReplayTask *task,
                          const gchar *error,
                          gpointer user_data)
{
  TaskData *data;
  const gchar *desc;
  gchar *title;

  g_return_if_fail(user_data != NULL);
  data = (TaskData *)user_data;

  show_activity(data->self, FALSE);

  g_signal_handlers_disconnect_by_func(task, task_error_cb, data);
  g_signal_handlers_disconnect_by_func(task, task_progress_cb, data);

  gtk_widget_destroy(data->info_bar);

  desc = replay_task_get_description(task);
  title = g_strdup_printf(_("<b>ERROR:</b> %s"), desc);
  replay_window_display_error(data->self, title, error);
  g_free(title);

  /* make sure task is cancelled */
  replay_task_cancel(task);

  g_object_unref(data->task);
  g_free(data);
}

static void task_progress_cb(ReplayTask *task,
                             gdouble fraction,
                             gpointer user_data)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;
  TaskData *data;
  GtkWidget *progress_bar;
  gchar *text;

  g_return_if_fail(user_data != NULL);
  data = (TaskData *)user_data;
  progress_bar = data->progress_bar;

  self = data->self;
  priv = self->priv;

  text = g_strdup_printf("%2.0f%%", 100.0 * fraction);
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar),
                            text);
  g_free(text);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),
                                fraction);

  if (fraction >= 1.0)
  {
    ReplayFileLoader *loader;

    show_activity(self, FALSE);

    g_signal_handlers_disconnect_by_func(task, task_error_cb, data);
    g_signal_handlers_disconnect_by_func(task, task_progress_cb, data);

    gtk_widget_destroy(data->info_bar);

    loader = (ReplayFileLoader *)g_object_get_data(G_OBJECT(task),
                                                REPLAY_FILE_LOADER_TASK_KEY);
    /* this task was for a file load */
    if (loader != NULL && REPLAY_EVENT_SOURCE(loader) == priv->event_source)
    {
      /* done with processor now */
      g_signal_handlers_disconnect_by_func(priv->event_processor,
                                           event_source_event_cb,
                                           self);
      g_object_unref(priv->event_processor);
      priv->event_processor = NULL;

      replay_window_set_event_source(self, NULL);

      set_widget_data(self);
    }

    g_object_unref(data->task);
    g_free(data);
  }
}

static void event_source_event_cb(ReplayEventSource *event_source,
                                  ReplayEvent *event,
                                  gpointer user_data)
{
  ReplayWindow *self;

  g_return_if_fail(REPLAY_IS_WINDOW(user_data));
  self = REPLAY_WINDOW(user_data);

  /* hold a ref to event whilst we are processing */
  g_object_ref(event);
  g_signal_emit(self,
                signals[REPLAY_EVENT],
                0,
                event);
  g_object_unref(event);
}

static void event_source_error_cb(ReplayEventSource *event_source,
                                  const gchar *error,
                                  gpointer user_data)
{
  g_return_if_fail(REPLAY_IS_WINDOW(user_data));

  replay_window_display_error(REPLAY_WINDOW(user_data),
                           error, NULL);
}

static void event_source_notify_description_cb(ReplayEventSource *event_source,
                                               GParamSpec *pspec,
                                               gpointer user_data)
{
  const gchar *description;

  g_return_if_fail(REPLAY_IS_WINDOW(user_data));

  description = replay_event_source_get_description(event_source);
  if (description != NULL) {
    gtk_window_set_title(GTK_WINDOW(user_data), description);
  } else {
    gtk_window_set_title(GTK_WINDOW(user_data), "Replay");
  }
}

static void
file_loader_error_cb(ReplayFileLoader *loader,
                     gchar *error,
                     gpointer data)
{
  ReplayTask *task;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(loader));
  g_return_if_fail(REPLAY_IS_WINDOW(data));

  task = REPLAY_TASK(g_object_get_data(G_OBJECT(loader), REPLAY_FILE_LOADER_TASK_KEY));
  replay_task_emit_error(task, error);
}

static void
file_loader_progress_cb(ReplayFileLoader *loader,
                        gdouble fraction,
                        gpointer data)
{
  ReplayTask *task;

  g_return_if_fail(REPLAY_IS_FILE_LOADER(loader));
  g_return_if_fail(REPLAY_IS_WINDOW(data));

  task = REPLAY_TASK(g_object_get_data(G_OBJECT(loader), REPLAY_FILE_LOADER_TASK_KEY));
  replay_task_emit_progress(task, fraction);
}

void replay_window_load_file(ReplayWindow *self, GFile *file)
{
  ReplayWindowPrivate *priv;
  const GList *list;
  GFileInfo *file_info;
  gboolean match = FALSE;
  GError *error = NULL;

  g_return_if_fail(REPLAY_IS_WINDOW(self));
  g_return_if_fail(file != NULL);

  priv = self->priv;

  file_info = g_file_query_info(file,
                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                G_FILE_QUERY_INFO_NONE,
                                NULL, &error);

  if (file_info != NULL)
  {
    /* find the correct file loader for this file type */
    ReplayApplication *app =
      REPLAY_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));
    for (list = replay_application_get_file_loaders(app);
         list != NULL;
         list = list->next)
    {
      ReplayFileLoader *loader = REPLAY_FILE_LOADER(list->data);
      GtkFileFilter *filter;
      GtkFileFilterInfo info;
      const gchar *mime_type, *pattern;

      filter = gtk_file_filter_new();
      g_object_ref_sink(filter);

      gtk_file_filter_set_name(filter,
                               replay_event_source_get_name(REPLAY_EVENT_SOURCE(loader)));
      mime_type = replay_file_loader_get_mime_type(loader);
      if (mime_type)
      {
        gtk_file_filter_add_mime_type(filter, mime_type);
      }
      pattern = replay_file_loader_get_pattern(loader);
      if (pattern)
      {
        gtk_file_filter_add_pattern(filter, pattern);
      }

      info.contains = gtk_file_filter_get_needed(filter);

      if (info.contains & GTK_FILE_FILTER_FILENAME)
      {
        info.filename = g_file_info_get_name(file_info);
        if (!info.filename)
        {
          info.contains &= ~GTK_FILE_FILTER_FILENAME;
        }
      }
      else
      {
        info.filename = NULL;
      }

      if (info.contains & GTK_FILE_FILTER_DISPLAY_NAME)
      {
        info.display_name = g_file_info_get_display_name(file_info);
        if (!info.display_name)
        {
          info.contains &= ~GTK_FILE_FILTER_DISPLAY_NAME;
        }
      }
      else
      {
        info.display_name = NULL;
      }

      if (info.contains & GTK_FILE_FILTER_URI)
      {
        info.uri = g_file_get_uri(file);
        if (!info.uri)
        {
          info.contains &= ~GTK_FILE_FILTER_URI;
        }
      }
      else
      {
        info.uri = NULL;
      }

      if (info.contains & GTK_FILE_FILTER_MIME_TYPE)
      {
        info.mime_type = g_file_info_get_content_type(file_info);
        if (!info.mime_type)
        {
          info.contains &= ~GTK_FILE_FILTER_MIME_TYPE;
        }
      }
      else
      {
        info.mime_type = NULL;
      }

      match = gtk_file_filter_filter(filter, &info);
      g_object_unref(filter);

      if (match)
      {
        GtkRecentManager *manager;
        ReplayTask *task;
        gchar *desc;
        gchar *uri = g_file_get_uri(file);
        gchar *filename = g_file_get_parse_name(file);
        gchar *basename = g_path_get_basename(filename);
        gchar *dirname = g_path_get_dirname(filename);

        /* add to recently opened files */
        manager = gtk_recent_manager_get_default();
        gtk_recent_manager_add_item(manager, uri);

        /* listen to this new event source */
        replay_window_set_event_source(self, REPLAY_EVENT_SOURCE(loader));

        /* show file loading task progress to user */
        desc = g_strdup_printf(_("Loading events from <b>%s</b> in <b>%s</b>"),
                               basename, dirname);
        task = replay_task_new(GTK_STOCK_OPEN, desc);
        g_object_set_data(G_OBJECT(loader), REPLAY_FILE_LOADER_TASK_KEY, task);
        g_object_set_data(G_OBJECT(task), REPLAY_FILE_LOADER_TASK_KEY, loader);
        g_signal_connect(loader, "progress",
                         G_CALLBACK(file_loader_progress_cb), self);
        g_signal_connect(loader, "error",
                         G_CALLBACK(file_loader_error_cb), self);

        replay_window_register_task(self, task);

        replay_log_info(priv->log, "replay", desc, NULL);
        replay_file_loader_load_file(loader, file);
        g_free(desc);
        g_free(dirname);
        g_free(basename);
        g_free(filename);
        g_free(uri);
        break;
      }
    }

    if (!match)
    {
      gchar *filename = g_file_get_parse_name(file);
      gchar *title = g_strdup_printf(_("Error loading events from file '%s'"),
                                     filename);
      replay_window_display_error(self, title,
                _("No plugin exists to handle loading events from file"));

      g_free(title);
      g_free(filename);
    }
    g_object_unref(file_info);
  }
  else
  {
    gchar *filename = g_file_get_parse_name(file);
    gchar *title = g_strdup_printf(_("Error loading events from file '%s'"),
                                   filename);
    replay_window_display_error(self, title, error->message);

    g_clear_error(&error);
    g_free(title);
    g_free(filename);
  }
}

static void replay_window_drag_data_received(GtkWidget *widget,
                                          GdkDragContext *drag_context,
                                          gint x, gint y,
                                          GtkSelectionData *data,
                                          guint info,
                                          guint event_time)
{
  gchar **uris;
  ReplayWindow *self;
  gboolean ret = FALSE;

  g_return_if_fail(REPLAY_IS_WINDOW(widget));

  self = REPLAY_WINDOW(widget);

  /* copy string uri data remove any trailing or leading whitespace */
  uris = gtk_selection_data_get_uris(data);

  if (uris != NULL)
  {
    GFile *file;

    /* only use first uri - ignore the rest - and strip whitespace first */
    g_strstrip(uris[0]);

    file = g_file_new_for_uri(uris[0]);
    replay_window_load_file(self, file);
    g_object_unref(file);
    ret = TRUE;
    g_strfreev(uris);
  }

  gtk_drag_finish(drag_context, ret, FALSE, event_time);
}

/* signal handlers in ui file */
static void plugins_cb(GtkAction *action,
                       gpointer data)
{
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  gtk_window_present(GTK_WINDOW(priv->plugins_dialog));
}

static void event_text_view_cb(GtkAction *action,
                               gpointer data)
{
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  gtk_window_present(GTK_WINDOW(priv->event_text_window));
}

static void log_window_cb(GtkAction *action,
                          gpointer data)
{
  ReplayWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  gtk_window_present(GTK_WINDOW(priv->log_window));
}

static void open_cb(GtkAction *action,
                    gpointer data)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;
  ReplayApplication *app;
  GtkWidget *dialog;
  const GList *list;

  g_return_if_fail(REPLAY_IS_WINDOW(data));
  self = REPLAY_WINDOW(data);
  priv = self->priv;
  dialog = gtk_file_chooser_dialog_new("Open event log file",
                                       GTK_WINDOW(self),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                       NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                      priv->last_open_folder);

  /* add custom filter first */
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), priv->custom_filter);

  app = REPLAY_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));
  /* filter to allow only those files which we have file loaders for */
  for (list = replay_application_get_file_loaders(app);
       list != NULL;
       list = list->next)
  {
    ReplayFileLoader *loader = REPLAY_FILE_LOADER(list->data);
    GtkFileFilter *filter = gtk_file_filter_new();
    const gchar *mime_type, *pattern;

    gtk_file_filter_set_name(filter,
                             replay_event_source_get_name(REPLAY_EVENT_SOURCE(loader)));
    mime_type = replay_file_loader_get_mime_type(loader);
    if (mime_type)
    {
      gtk_file_filter_add_mime_type(filter, mime_type);
    }
    pattern = replay_file_loader_get_pattern(loader);
    if (pattern)
    {
      gtk_file_filter_add_pattern(filter, pattern);
    }

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
  }

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    GFile *file;
    gchar *open_folder;

    file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));

    /* update open location */
    open_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
    if (open_folder)
    {
      g_free(priv->last_open_folder);
      priv->last_open_folder = open_folder;
    }

    replay_window_load_file(self, file);
    g_object_unref(file);
  }

  gtk_widget_destroy(dialog);
}

static void open_recent_cb(GtkRecentChooser *chooser,
                           gpointer data)
{
  ReplayWindow *self;
  GFile *file;
  gchar *uri;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);

  uri = gtk_recent_chooser_get_current_uri(chooser);
  file = g_file_new_for_uri(uri);

  replay_window_load_file(self, file);
  g_object_unref(file);
  g_free(uri);
}


static void save_cb(GtkAction *action,
                    gpointer data)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;
  GtkWidget *dialog;
  GtkFileFilter *filter;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);
  priv = self->priv;
  dialog = gtk_file_chooser_dialog_new("Save Timeline View",
                                       GTK_WINDOW(self),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                       NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                      priv->last_save_folder);

  /* ask user if they try to overwrite an existing file */
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                 TRUE);

  /* filter to allow only PDF files to be selected and created */
  filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "PDF");
  gtk_file_filter_add_mime_type(filter, "application/pdf");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
  {
    gchar *filename;
    gchar *save_folder;

    /* update save location */
    save_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
    if (save_folder)
    {
      g_free(priv->last_save_folder);
      priv->last_save_folder = save_folder;
    }

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    replay_timeline_view_export_to_file(REPLAY_TIMELINE_VIEW(self->priv->timeline_view),
                                     filename);

    g_free(filename);
  }

  gtk_widget_destroy(dialog);
}

static void close_cb(GtkAction *action,
                     gpointer data)
{
  g_return_if_fail(REPLAY_IS_WINDOW(data));

  replay_window_save_layout(REPLAY_WINDOW(data));
  replay_window_cleanup(REPLAY_WINDOW(data));
  gtk_widget_destroy(GTK_WIDGET(data));
}

static void toggle_fullscreen_cb(GtkAction *action,
                                 gpointer data)
{
  ReplayWindowPrivate *priv;
  GtkAction *fullscreen_action;
  gboolean fullscreen;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  fullscreen_action = gtk_action_group_get_action(priv->global_actions,
                                                  "FullscreenAction");
  fullscreen = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(fullscreen_action));
  gtk_widget_set_visible(priv->menubar, !fullscreen);

  if (fullscreen)
  {
    /* reparent toolbar into fs_toolbar window so we can auto-show / hide it */
    g_object_ref(priv->toolbar);
    gtk_container_remove(GTK_CONTAINER(priv->toolbar_box), priv->toolbar);
    gtk_container_add(GTK_CONTAINER(priv->fs_toolbar), priv->toolbar);
    g_object_unref(priv->toolbar);
    gtk_widget_show(priv->fs_toolbar);
    /* slide the toolbar out but wait 750ms first */
    g_timeout_add(750, (GSourceFunc)slide_fs_toolbar_out, data);
    gtk_window_fullscreen(GTK_WINDOW(data));
  }
  else
  {
    g_object_ref(priv->toolbar);
    gtk_container_remove(GTK_CONTAINER(priv->fs_toolbar), priv->toolbar);
    gtk_container_add(GTK_CONTAINER(priv->toolbar_box), priv->toolbar);
    g_object_unref(priv->toolbar);
    gtk_widget_hide(priv->fs_toolbar);
    gtk_window_unfullscreen(GTK_WINDOW(data));
  }
}

static void toggle_absolute_time_cb(GtkAction *action,
                                    gpointer data)
{
  ReplayWindowPrivate *priv;
  gboolean absolute_time;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  absolute_time = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
  replay_prefs_set_absolute_time(priv->prefs, absolute_time);
}

static void toggle_label_nodes_cb(GtkAction *action,
                                  gpointer data)
{
  ReplayWindowPrivate *priv;
  gboolean label_nodes;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  label_nodes = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
  replay_prefs_set_label_nodes(priv->prefs, label_nodes);
}

static void toggle_label_edges_cb(GtkAction *action,
                                  gpointer data)
{
  ReplayWindowPrivate *priv;
  gboolean label_edges;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  label_edges = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
  replay_prefs_set_label_edges(priv->prefs, label_edges);
}

static void toggle_label_messages_cb(GtkAction *action,
                                     gpointer data)
{
  ReplayWindowPrivate *priv;
  gboolean label_messages;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  label_messages = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
  replay_prefs_set_label_messages(priv->prefs, label_messages);
}

static void zoom_in_cb(GtkAction *action,
                       gpointer data)
{
  ReplayTimelineView *timeline_view;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  timeline_view = REPLAY_TIMELINE_VIEW(REPLAY_WINDOW(data)->priv->timeline_view);
  replay_timeline_view_set_x_resolution(timeline_view,
                                     (replay_timeline_view_get_x_resolution(timeline_view) *
                                      ZOOM_FACTOR));
}

static void zoom_out_cb(GtkAction *action,
                        gpointer data)
{
  ReplayTimelineView *timeline_view;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  timeline_view = REPLAY_TIMELINE_VIEW(REPLAY_WINDOW(data)->priv->timeline_view);
  replay_timeline_view_set_x_resolution(timeline_view,
                                     (replay_timeline_view_get_x_resolution(timeline_view) /
                                      ZOOM_FACTOR));
}

static void begin_print(GtkPrintOperation *op,
                        GtkPrintContext *context,
                        gpointer data)
{
  ReplayWindow *self;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);
  replay_timeline_view_begin_print(REPLAY_TIMELINE_VIEW(self->priv->timeline_view), op, context);
}

static void draw_page(GtkPrintOperation *op,
                      GtkPrintContext *context,
                      int page_nr,
                      gpointer data)
{
  ReplayWindow *self;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);
  replay_timeline_view_draw_page(REPLAY_TIMELINE_VIEW(self->priv->timeline_view), op, context,
                              page_nr);
}

static void end_print(GtkPrintOperation *op,
                      GtkPrintContext *context,
                      gpointer data)
{
  ReplayWindow *self;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  self = REPLAY_WINDOW(data);
  replay_timeline_view_end_print(REPLAY_TIMELINE_VIEW(self->priv->timeline_view), op, context);
}

static void print_cb(GtkAction *action,
                     gpointer data)
{
  ReplayWindow *self;
  ReplayWindowPrivate *priv;
  GtkPrintOperation *op;
  GtkPrintOperationResult res;

  g_return_if_fail(REPLAY_IS_WINDOW(data));
  self = REPLAY_WINDOW(data);

  priv = self->priv;

  if (priv->page_setup == NULL)
  {
    priv->page_setup = gtk_page_setup_new();
  }
  g_assert(priv->page_setup != NULL);

  op = gtk_print_operation_new();
  if (priv->print_settings == NULL)
  {
    const gchar *dir, *ext;
    gchar *uri;

    priv->print_settings = gtk_print_settings_new();

    /* set a sensible default filename for printing to a file - place in user's
     * Documents dir or home dir if not found */
    dir = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
    if (dir == NULL)
    {
      dir = g_get_home_dir();
    }
    /* set extension based on current output file format - is either ps or
     * pdf */
    if (g_strcmp0(gtk_print_settings_get(priv->print_settings,
                                         GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT),
                  "ps") == 0)
    {
      ext = ".ps";
    }
    else
    {
      ext = ".pdf";
    }

    uri = g_strconcat("file://", dir, "/", "replay-timeline", ext, NULL);
    gtk_print_settings_set(priv->print_settings,
                           GTK_PRINT_SETTINGS_OUTPUT_URI, uri);
    g_free(uri);
  }

  gtk_print_operation_set_default_page_setup(op, priv->page_setup);
  gtk_print_operation_set_print_settings(op, priv->print_settings);

  gtk_print_operation_set_embed_page_setup(op, TRUE);

  g_signal_connect(op, "begin_print", G_CALLBACK(begin_print), self);
  g_signal_connect(op, "draw_page", G_CALLBACK(draw_page), self);
  g_signal_connect(op, "end_print", G_CALLBACK(end_print), self);
  res = gtk_print_operation_run(op,
                                GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                GTK_WINDOW(self), NULL);
  if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
  {
    priv->print_settings = gtk_print_operation_get_print_settings(op);
  }
}

static void page_setup_cb(GtkAction *action,
                          gpointer data)
{
  ReplayWindowPrivate *priv;
  GtkPageSetup *new_page_setup;

  g_return_if_fail(REPLAY_IS_WINDOW(data));

  priv = REPLAY_WINDOW(data)->priv;

  if (priv->print_settings == NULL)
  {
    priv->print_settings = gtk_print_settings_new();
  }

  if (priv->page_setup == NULL)
  {
    priv->page_setup = gtk_page_setup_new();
  }

  new_page_setup = gtk_print_run_page_setup_dialog(GTK_WINDOW(data),
                                                   priv->page_setup,
                                                   priv->print_settings);
  if (new_page_setup != priv->page_setup)
  {
    if (priv->page_setup)
    {
      g_object_unref(priv->page_setup);
    }
    priv->page_setup = new_page_setup;
  }
}

/**
 * replay_window_new:
 * @application: The #ReplayApplication which is controlling this window
 *
 * Returns: (transfer full): A new #ReplayWindow
 */
GtkWidget *replay_window_new(ReplayApplication *application)
{
  return g_object_new(REPLAY_TYPE_WINDOW,
                      "application", application,
                      "title", "Replay",
                      NULL);
}

/**
 * replay_window_get_ui_manager:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #GtkUIManager for this #ReplayWindow
 */
GtkUIManager *replay_window_get_ui_manager(ReplayWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);

  return self->priv->ui_manager;
}

static void set_widget_data(ReplayWindow *self)
{
  ReplayWindowPrivate *priv = self->priv;

  replay_graph_view_set_data(REPLAY_GRAPH_VIEW(priv->graph_view), priv->event_store,
                          priv->node_tree);
  replay_timeline_view_set_data(REPLAY_TIMELINE_VIEW(priv->timeline_view),
                             priv->node_tree, priv->interval_list);
  replay_event_text_view_set_data(REPLAY_EVENT_TEXT_VIEW(priv->event_text_view),
                               priv->event_store, priv->node_tree);
  replay_message_tree_view_set_data(REPLAY_MESSAGE_TREE_VIEW(priv->message_tree_view),
                                 priv->message_tree, priv->node_tree);
}

static void task_response_cb(GtkWidget *widget,
                             gint response_id,
                             gpointer user_data)
{
  TaskData *data;
  ReplayTask *task;
  ReplayWindow *self;

  data = (TaskData *)user_data;
  task = data->task;
  self = data->self;

  /* stop listening to this task - emit progress of 1.0 - and then actually
   * cancel it */
  replay_task_emit_progress(task, 1.0);
  replay_task_cancel(task);
  replay_window_cleanup(self);
}

void replay_window_register_task(ReplayWindow *self,
                              ReplayTask *task)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *icon;
  GtkWidget *label;
  TaskData *data;

  g_return_if_fail(REPLAY_IS_WINDOW(self));
  g_return_if_fail(REPLAY_IS_TASK(task));

  data = g_malloc(sizeof(*data));

  data->self = self;
  data->task = g_object_ref(task);

  icon = gtk_image_new_from_stock(replay_task_get_icon(task),
                                  GTK_ICON_SIZE_DIALOG);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), replay_task_get_description(task));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

  data->progress_bar = gtk_progress_bar_new();
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), data->progress_bar, TRUE, TRUE, 0);
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  data->info_bar = gtk_info_bar_new_with_buttons(GTK_STOCK_CANCEL,
                                                 GTK_RESPONSE_CANCEL,
                                                 NULL);
  g_signal_connect(data->info_bar, "response",
                   G_CALLBACK(task_response_cb), data);
  gtk_box_pack_start(GTK_BOX(gtk_info_bar_get_content_area(GTK_INFO_BAR(data->info_bar))),
                     hbox, TRUE, TRUE, 0);
  /* place this into the main info_bar_box which holds info_bar's in self */
  gtk_box_pack_end(GTK_BOX(self->priv->info_bar_box), data->info_bar,
                   FALSE, TRUE, 0);
  gtk_widget_show_all(data->info_bar);

  show_activity(self, TRUE);
  g_signal_connect(task, "progress", G_CALLBACK(task_progress_cb), data);
  g_signal_connect(task, "error", G_CALLBACK(task_error_cb), data);
}

void replay_window_set_event_source(ReplayWindow *self,
                                 ReplayEventSource *event_source)
{
  ReplayWindowPrivate *priv;
  gchar *message;

  g_return_if_fail(REPLAY_IS_WINDOW(self));
  g_return_if_fail(!event_source || REPLAY_IS_EVENT_SOURCE(event_source));

  priv = self->priv;

  if (priv->event_source)
  {
    message = g_strdup_printf("Stopping listening to event source '%s'",
                              replay_event_source_get_name(priv->event_source));
    replay_log_debug(priv->log, "replay", message, NULL);
    g_free(message);

    if (REPLAY_IS_FILE_LOADER(priv->event_source))
    {
      ReplayTask *task;

      task = REPLAY_TASK(g_object_get_data(G_OBJECT(priv->event_source),
                                           REPLAY_FILE_LOADER_TASK_KEY));
      if (task)
      {
        replay_task_emit_progress(task, 1.0);
        replay_file_loader_cancel(REPLAY_FILE_LOADER(priv->event_source));
      }
    }

    g_signal_handlers_disconnect_by_func(priv->event_source,
                                         event_source_event_cb,
                                         self);
    g_signal_handlers_disconnect_by_func(priv->event_source,
                                         event_source_error_cb,
                                         self);
    g_signal_handlers_disconnect_by_func(priv->event_source,
                                         event_source_notify_description_cb,
                                         self);
    g_object_unref(priv->event_source);
    priv->event_source = NULL;
  }

  if (event_source)
  {
    const gchar *description;

    /* cleanup existing display */
    replay_window_cleanup(self);

    /* create new data structures for this event source */
    priv->event_store = replay_event_store_new();
    priv->node_tree = replay_node_tree_new();
    priv->interval_list = replay_interval_list_new(priv->event_store);
    priv->message_tree = replay_message_tree_new(priv->event_store);
    priv->event_processor = replay_event_processor_new(priv->event_store,
                                                    priv->node_tree,
                                                    priv->interval_list,
                                                    priv->message_tree);

    /* listen to event processor which generates ancillary events to help
     * maintain consistent state when running backwards */
    g_signal_connect(priv->event_processor, "event",
                     G_CALLBACK(event_source_event_cb), self);

    /* handle ReplayFileLoader's specially - don't set any widget's data
     * structures here, will be done separately - this is so we can do most of
     * the work
     * once the 'done' signal is emitted rather than as events are being loaded
     * from the file */
    if (!REPLAY_IS_FILE_LOADER(event_source))
    {
      set_widget_data(self);
    }

    /* watch this event source for events */
    priv->event_source = g_object_ref(event_source);
    g_signal_connect(priv->event_source, "event",
                     G_CALLBACK(event_source_event_cb),
                     self);
    g_signal_connect(priv->event_source, "error",
                     G_CALLBACK(event_source_error_cb),
                     self);
    g_signal_connect(priv->event_source, "notify::description",
                     G_CALLBACK(event_source_notify_description_cb),
                     self);
    description = replay_event_source_get_description(priv->event_source);
    if (description != NULL) {
      gtk_window_set_title(GTK_WINDOW(self), description);
    } else {
      gtk_window_set_title(GTK_WINDOW(self), "Replay");
    }
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_EVENT_SOURCE]);
    message = g_strdup_printf("Using event source '%s'",
                              replay_event_source_get_name(event_source));
    replay_log_debug(priv->log, "replay", message, NULL);
    g_free(message);

    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_EVENT_STORE]);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_NODE_TREE]);
  }
}

/**
 * replay_window_get_event_source:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #ReplayEventSource for this #ReplayWindow
 */
ReplayEventSource *
replay_window_get_event_source(ReplayWindow *self) {
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);
  return self->priv->event_source;
}

void replay_window_set_display_flags(ReplayWindow *self,
                                  ReplayWindowDisplayFlags flags)
{
  ReplayWindowPrivate *priv;
  ReplayWindowDisplayFlags changed;

  g_return_if_fail(REPLAY_IS_WINDOW(self));

  priv = self->priv;

  /* see what flags have changed and set view appropriately */
  changed = priv->display_flags ^ flags;

  priv->display_flags = flags;

  if (changed & REPLAY_WINDOW_DISPLAY_MESSAGE_TREE)
  {
    gtk_widget_set_visible(priv->scrolled_window,
                           flags & REPLAY_WINDOW_DISPLAY_MESSAGE_TREE);
  }
  if (changed & REPLAY_WINDOW_DISPLAY_TIMELINE)
  {
    gtk_widget_set_visible(priv->timeline_view,
                           flags & REPLAY_WINDOW_DISPLAY_TIMELINE);
  }
  if (changed & REPLAY_WINDOW_DISPLAY_GRAPH)
  {
    gtk_widget_set_visible(priv->graph_view,
                           flags & REPLAY_WINDOW_DISPLAY_GRAPH);
  }
  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DISPLAY_FLAGS]);
}

ReplayWindowDisplayFlags replay_window_get_display_flags(ReplayWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), 0);

  return self->priv->display_flags;
}

/**
 * replay_window_get_global_action_group:
 * @self: the #ReplayWindow to get the global action group for
 *
 * Returns: (transfer none): The #GtkActionGroup containing the actions which
 * can be performed on a #ReplayWindow at any time. The caller will need to
 * g_object_ref() this if they want to maintain a reference.
 */
GtkActionGroup *
replay_window_get_global_action_group(ReplayWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);

  return self->priv->global_actions;
}

/**
 * replay_window_get_doc_action_group:
 * @self: the #ReplayWindow to get the document action group for
 *
 * Returns: (transfer none): The #GtkActionGroup containing the actions which
 * can be performed on a #ReplayWindow when a document is loaded (i.e. when there
 * are events loaded). The caller will need to g_object_ref() this if they want
 * to maintain a reference.
 */
GtkActionGroup *
replay_window_get_doc_action_group(ReplayWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);

  return self->priv->doc_actions;
}

/**
 * replay_window_get_event_store:
 * @self: the #ReplayWindow to get the #ReplayEventStore from
 *
 * Returns: (transfer none): The current list of events being displayed by this
 * window. The caller will need to g_object_ref() this if the want to maintain
 * a reference.
 */
ReplayEventStore *
replay_window_get_event_store(ReplayWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);

  return self->priv->event_store;
}

/**
 * replay_window_get_node_tree:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #ReplayNodeTree for this #ReplayWindow
 */
ReplayNodeTree *
replay_window_get_node_tree(ReplayWindow *self) {
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);
  return self->priv->node_tree;
}

/**
 * replay_window_get_graph_view:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #ReplayGraphView for this #ReplayWindow
 */
ReplayGraphView *
replay_window_get_graph_view(ReplayWindow *self) {
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);
  return REPLAY_GRAPH_VIEW(self->priv->graph_view);
}

/**
 * replay_window_get_message_tree_view:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #ReplayMessageTreeView for this #ReplayWindow
 */
ReplayMessageTreeView *
replay_window_get_message_tree_view(ReplayWindow *self) {
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);
  return REPLAY_MESSAGE_TREE_VIEW(self->priv->message_tree_view);
}

/**
 * replay_window_get_log:
 * @self: A #ReplayWindow
 *
 * Returns: (transfer none): The #ReplayLog for this #ReplayWindow
 */
ReplayLog *
replay_window_get_log(ReplayWindow *self) {
  g_return_val_if_fail(REPLAY_IS_WINDOW(self), NULL);
  return self->priv->log;
}
