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
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "replay-search-plugin.h"
#include "replay-search-window.h"
#include <replay/replay-window-activatable.h>
#include <replay/replay-option-activatable.h>
#include <replay/replay-window.h>
#include <replay/replay-msg-send-event.h>

#define REPLAY_SEARCH_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), REPLAY_TYPE_SEARCH_PLUGIN, ReplaySearchPluginPrivate))

struct _ReplaySearchPluginPrivate
{
  ReplayWindow *window;
  ReplaySearchWindow *search_window;
  GtkActionGroup *doc_actions;
  guint           ui_id;
  GOptionContext *context;

  GRegex *pattern;

  GThread *search_thread;
  GMainContext *search_context;
  GMainLoop *search_loop;

  GSource *search_source;
  /* protect variables from concurrent access between main loop and search
   * thread */
  GMutex lock;
  gboolean forward;
  gboolean run_search;
  GtkTreeIter iter;
};

static void search_action_cb(GtkAction *action,
                              gpointer data);
static void search_next_action_cb(GtkAction *action,
                                  gpointer data);
static void search_prev_action_cb(GtkAction *action,
                                  gpointer data);
static void replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface);

static GtkActionEntry doc_entries[] =
{
  { "SearchAction", GTK_STOCK_FIND, N_("Find..."), NULL,
    N_("Find a specific event"), G_CALLBACK(search_action_cb) },
  { "SearchNextAction", GTK_STOCK_FIND, N_("Find next"), "<Control><Shift>F",
    N_("Find next matching event"), G_CALLBACK(search_next_action_cb) },
  { "SearchPrevAction", GTK_STOCK_FIND, N_("Find previous"), "<Control><Shift>G",
    N_("Find previous matching event"), G_CALLBACK(search_prev_action_cb) },
};

#define MENU_PATH "/MenuBar/EditMenu/EditOps1"
#define TOOLBAR_PATH "/ToolBar/ToolItemOps1"

G_DEFINE_DYNAMIC_TYPE_EXTENDED(ReplaySearchPlugin,
                               replay_search_plugin,
                               PEAS_TYPE_EXTENSION_BASE,
                               0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                                             replay_window_activatable_iface_init));

enum {
  PROP_WINDOW = 1,
};

static void replay_search_plugin_dispose(GObject *object);

static void
replay_search_plugin_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  ReplaySearchPlugin *plugin = REPLAY_SEARCH_PLUGIN(object);

  switch (prop_id) {
    case PROP_WINDOW:
      plugin->priv->window = REPLAY_WINDOW(g_value_dup_object(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_search_plugin_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  ReplaySearchPlugin *plugin = REPLAY_SEARCH_PLUGIN(object);

  switch (prop_id) {
    case PROP_WINDOW:
      g_value_set_object(value, plugin->priv->window);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}
static void
replay_search_plugin_init (ReplaySearchPlugin *plugin)
{
  plugin->priv = REPLAY_SEARCH_PLUGIN_GET_PRIVATE(plugin);
}

static void
replay_search_plugin_dispose (GObject *object)
{
  ReplaySearchPlugin *self = REPLAY_SEARCH_PLUGIN(object);
  g_clear_object(&self->priv->window);
  G_OBJECT_CLASS (replay_search_plugin_parent_class)->dispose (object);
}

static void present_search_window(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv;
  GtkWidget *message_tree_view;
  GdkWindow *window;
  gint parent_x, parent_y, parent_width;
  gint width, height;

  priv = self->priv;

  gtk_widget_show_all(GTK_WIDGET(priv->search_window));
  /* move to show just over top right corner of message tree view */
  message_tree_view = GTK_WIDGET(replay_window_get_message_tree_view(priv->window));
  window = gtk_widget_get_window(message_tree_view);
  gdk_window_get_origin(window, &parent_x, &parent_y);
  parent_width = gdk_window_get_width(window);
  gtk_window_get_size(GTK_WINDOW(priv->search_window), &width, &height);
  gtk_window_move(GTK_WINDOW(priv->search_window),
                  parent_x + parent_width - width,
                  parent_y);
  gtk_window_present(GTK_WINDOW(priv->search_window));
}

static void search_action_cb(GtkAction *action,
                             gpointer data)
{
  g_return_if_fail(REPLAY_IS_SEARCH_PLUGIN(data));

  present_search_window(REPLAY_SEARCH_PLUGIN(data));
}

static gboolean
compare_msg_send_event(ReplaySearchPlugin *self,
                       ReplayMsgSendEvent *event)
{
  /* compare on message contents using pattern as a regex */
  ReplaySearchPluginPrivate *priv = self->priv;
  gboolean match = FALSE;
  GVariant *props = replay_msg_send_event_get_props(event);
  gchar *description = NULL;
  gboolean ret;

  ret = g_variant_lookup(props, "description", "s", &description);

  if (ret)
  {
    gchar *plain;
    GError *error = NULL;
    if (pango_parse_markup(description, -1, 0, NULL, &plain, NULL, &error))
    {
      match = g_regex_match(priv->pattern, plain, 0, NULL);
    }
    else
    {
      replay_log_error(replay_window_get_log(priv->window), "search-plugin",
                       _("Error stripping message description of markup to match against"),
                       error->message);
      g_clear_error(&error);
    }
    g_free(description);
  }

  return match;
}

static gboolean
compare_event(ReplaySearchPlugin *self,
              ReplayEvent *event)
{
  GType type;
  gboolean match = FALSE;

  type = G_OBJECT_TYPE(event);
  if (type == REPLAY_TYPE_MSG_SEND_EVENT)
  {
    match = compare_msg_send_event(self, REPLAY_MSG_SEND_EVENT(event));
  }
  return match;
}

static gboolean
set_search_idle(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv = self->priv;
  replay_search_window_set_state(priv->search_window,
                              REPLAY_SEARCH_WINDOW_STATE_IDLE);
  return FALSE;
}

static gboolean
set_current_to_iter(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv = self->priv;
  ReplayEventStore *store = replay_window_get_event_store(priv->window);
  replay_event_store_set_current(store, &priv->iter);
  replay_search_window_set_state(priv->search_window,
                              REPLAY_SEARCH_WINDOW_STATE_IDLE);
  return FALSE;
}

/* this is a source func - returns FALSE to remove the source and TRUE to keep
 * going */
static gboolean replay_search_plugin_run_search(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv;
  ReplayEventStore *store;
  gboolean match = FALSE;

  priv = self->priv;

  store = replay_window_get_event_store(priv->window);
  g_mutex_lock(&priv->lock);
  if (priv->run_search)
  {
    GList *events = replay_event_store_get_events(store, &priv->iter);
    while (events != NULL)
    {
      ReplayEvent *event = REPLAY_EVENT(events->data);
      match = compare_event(self, event);
      g_object_unref(event);
      events = g_list_delete_link(events, events);
    }
    if (match)
    {
      priv->run_search = FALSE;
      /* set current in main loop rather than from a thread... */
      gdk_threads_add_idle((GSourceFunc)set_current_to_iter, self);
    }
    else if (priv->forward)
    {
      priv->run_search = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &priv->iter);
    }
    else
    {
      priv->run_search = gtk_tree_model_iter_previous(GTK_TREE_MODEL(store), &priv->iter);
    }
  }
  if (!priv->run_search)
  {
    gdk_threads_add_idle((GSourceFunc)set_search_idle, self);
  }
  g_mutex_unlock(&priv->lock);
  return priv->run_search;
}

static void
search_thread_run(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_SEARCH_PLUGIN(self));

  priv = self->priv;

  priv->search_context = g_main_context_new();
  priv->search_loop = g_main_loop_new(priv->search_context, FALSE);
  /* make all async operations started from this thread run on this
     search_context rather than in the main loop */
  g_main_context_push_thread_default(priv->search_context);
  g_main_loop_run(priv->search_loop);
}

static void cancel_search(ReplaySearchPlugin *self)
{
  ReplaySearchPluginPrivate *priv;

  priv = self->priv;

  g_mutex_lock(&priv->lock);
  priv->run_search = FALSE;
  if (priv->search_source)
  {
    g_source_destroy(priv->search_source);
    g_source_unref(priv->search_source);
    priv->search_source = NULL;
  }
  replay_search_window_set_state(priv->search_window,
                              REPLAY_SEARCH_WINDOW_STATE_IDLE);
  g_mutex_unlock(&priv->lock);
}

static void search(ReplaySearchPlugin *self,
                   gboolean forward)
{
  ReplayEventStore *store;
  GtkTreePath *path;
  ReplaySearchPluginPrivate *priv;
  gboolean ret;

  priv = self->priv;

  cancel_search(self);

  /* no point searching if we don't have a pattern yet to search on */
  if (!priv->pattern)
    return;

  store = replay_window_get_event_store(priv->window);
  /* this returns the iter pointing to an event within a time - go up to get
     all events at this time */
  ret = replay_event_store_get_current(store, &priv->iter);
  if (!ret)
    /* iter is not valid - bail */
    return;

  path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &priv->iter);
  gtk_tree_path_up(path);

  /* and automatically advance so we skip over the current event time */
  if (priv->forward)
  {
    gtk_tree_path_next(path);
  }
  else
  {
    gtk_tree_path_prev(path);
  }
  g_mutex_lock(&priv->lock);
  priv->run_search = gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &priv->iter, path);
  if (priv->run_search)
  {
    priv->forward = forward;
    g_assert(priv->search_source == NULL);
    priv->search_source = g_idle_source_new();
    g_source_set_callback(priv->search_source,
                          (GSourceFunc)replay_search_plugin_run_search,
                          self,
                          NULL);
    g_source_attach(priv->search_source, priv->search_context);
    /* show search window and show searching state within it */
    present_search_window(self);
    replay_search_window_set_state(priv->search_window,
                                REPLAY_SEARCH_WINDOW_STATE_ACTIVE);
  }
  g_mutex_unlock(&priv->lock);
}

static void search_next_action_cb(GtkAction *action,
                                  gpointer data)
{
  /* ensure search window is visible */
  search(REPLAY_SEARCH_PLUGIN(data), TRUE);
}

static void search_prev_action_cb(GtkAction *action,
                                  gpointer data)
{
  g_return_if_fail(REPLAY_IS_SEARCH_PLUGIN(data));
  search(REPLAY_SEARCH_PLUGIN(data), FALSE);
}

static void search_cb(ReplaySearchWindow *window,
                      gboolean up,
                      gpointer user_data)
{
  ReplaySearchPlugin *self;
  ReplaySearchPluginPrivate *priv;
  GtkAction *action = NULL;
  GError *error = NULL;
  const gchar *pattern;

  g_return_if_fail(REPLAY_IS_SEARCH_PLUGIN(user_data));

  self = REPLAY_SEARCH_PLUGIN(user_data);
  priv = self->priv;

  g_mutex_lock(&priv->lock);
  if (priv->pattern)
  {
    g_regex_unref(priv->pattern);
    priv->pattern = NULL;
  }
  pattern = replay_search_window_get_text(priv->search_window);
  if (pattern && pattern[0] != '\0')
  {
    priv->pattern = g_regex_new(pattern, 0, 0, &error);
    if (!priv->pattern)
    {
      /* show error in search window */
      replay_search_window_set_state(priv->search_window,
                                  REPLAY_SEARCH_WINDOW_STATE_ERROR);
    }
    else
    {
      replay_search_window_set_state(priv->search_window,
                                  REPLAY_SEARCH_WINDOW_STATE_IDLE);
      action = gtk_action_group_get_action(priv->doc_actions,
                                           up ? "SearchPrevAction" : "SearchNextAction");
    }
  }
  g_mutex_unlock(&priv->lock);
  if (action)
  {
    gtk_action_activate(action);
  }
  if (!priv->pattern)
  {
    cancel_search(self);
  }
}

static void
doc_actions_notify_sensitive_cb(GtkActionGroup *doc_actions,
                                GParamSpec *pspec,
                                ReplaySearchPlugin *self) {
  gtk_action_group_set_sensitive(self->priv->doc_actions,
                                 gtk_action_group_get_sensitive(doc_actions));
}

static void
cancel_cb(ReplaySearchWindow *window,
          ReplaySearchPlugin *self)
{
  gtk_widget_hide(GTK_WIDGET(window));
  cancel_search(self);
}

static void
replay_search_plugin_activate(ReplayWindowActivatable *plugin)
{
  ReplaySearchPlugin *self = REPLAY_SEARCH_PLUGIN(plugin);
  ReplaySearchPluginPrivate *priv = self->priv;
  GtkActionGroup *doc_actions;
  GtkUIManager *manager;

  priv->search_window = REPLAY_SEARCH_WINDOW(replay_search_window_new());
  gtk_window_set_transient_for(GTK_WINDOW(priv->search_window),
                               GTK_WINDOW(priv->window));
  gtk_window_set_attached_to(GTK_WINDOW(priv->search_window),
                             GTK_WIDGET(priv->window));
  g_signal_connect(priv->search_window, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(priv->search_window, "search",
                   G_CALLBACK(search_cb), self);
  g_signal_connect(priv->search_window, "cancel",
                   G_CALLBACK(cancel_cb), self);

  /* set up to do filtering - watch window for change in the node-tree property
   * and add menu options for our ui */
  manager = replay_window_get_ui_manager(priv->window);

  priv->doc_actions = gtk_action_group_new("ReplaySearchPluginDocActions");
  gtk_action_group_set_translation_domain(priv->doc_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->doc_actions,
                               doc_entries,
                               G_N_ELEMENTS(doc_entries),
                               self);
  gtk_ui_manager_insert_action_group(manager, priv->doc_actions, -1);

  priv->ui_id = gtk_ui_manager_new_merge_id(manager);

  /* add actions to menu and toolbar */
  /* add a separator */
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "SearchBeginSeparator",
                        NULL,
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "SearchAction",
                        "SearchAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "SearchNextAction",
                        "SearchNextAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "SearchPrevAction",
                        "SearchPrevAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        MENU_PATH,
                        "SearchEndSeparator",
                        NULL,
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_ui_manager_add_ui(manager,
                        priv->ui_id,
                        TOOLBAR_PATH,
                        "SearchAction",
                        "SearchAction",
                        GTK_UI_MANAGER_AUTO,
                        FALSE);
  gtk_action_set_is_important(gtk_action_group_get_action(priv->doc_actions,
                                                          "SearchAction"),
                              TRUE);

  /* when sensitivity of the global doc actions changes, make sure we respect that */
  doc_actions = replay_window_get_doc_action_group(priv->window);
  gtk_action_group_set_sensitive(priv->doc_actions,
                                 gtk_action_group_get_sensitive(doc_actions));
  g_signal_connect(doc_actions, "notify::sensitive",
                   G_CALLBACK(doc_actions_notify_sensitive_cb), self);
  gtk_ui_manager_ensure_update(manager);

  /* create a separate thread which we use to do searching within to not block
     main loop */
  g_mutex_init(&priv->lock);
  priv->search_thread = g_thread_new("search-thread",
                                     (GThreadFunc)search_thread_run,
                                     self);
}

static void
replay_search_plugin_deactivate(ReplayWindowActivatable *plugin)
{
  ReplaySearchPlugin *self = REPLAY_SEARCH_PLUGIN(plugin);
  ReplaySearchPluginPrivate *priv = self->priv;
  GtkUIManager *manager;
  GtkActionGroup *doc_actions;

  cancel_search(self);
  g_assert(priv->search_source == NULL);
  g_main_loop_quit(priv->search_loop);
  g_thread_join(priv->search_thread);
  g_mutex_clear(&priv->lock);

  gtk_widget_destroy(GTK_WIDGET(priv->search_window));
  doc_actions = replay_window_get_doc_action_group(priv->window);
  g_signal_handlers_disconnect_by_func(doc_actions,
                                       G_CALLBACK(doc_actions_notify_sensitive_cb), self);
  manager = replay_window_get_ui_manager(priv->window);

  gtk_ui_manager_remove_ui(manager, priv->ui_id);
  gtk_ui_manager_remove_action_group(manager, priv->doc_actions);
  g_clear_object(&priv->doc_actions);
}

static void
replay_search_plugin_class_init (ReplaySearchPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (ReplaySearchPluginPrivate));

  object_class->get_property = replay_search_plugin_get_property;
  object_class->set_property = replay_search_plugin_set_property;
  object_class->dispose = replay_search_plugin_dispose;

  g_object_class_override_property(object_class, PROP_WINDOW, "window");
}

static void
replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface)
{
  iface->activate = replay_search_plugin_activate;
  iface->deactivate = replay_search_plugin_deactivate;
}

static void
replay_search_plugin_class_finalize(ReplaySearchPluginClass *klass)
{
  /* nothing to do */
}

G_MODULE_EXPORT void
peas_register_types(PeasObjectModule *module)
{
  replay_search_plugin_register_type(G_TYPE_MODULE(module));

  peas_object_module_register_extension_type(module,
                                             REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                             REPLAY_TYPE_SEARCH_PLUGIN);
}
