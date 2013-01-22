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
#include "replay-playback-plugin.h"
#include <replay/replay-window-activatable.h>
#include <replay/replay-window.h>

#define REPLAY_PLAYBACK_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), REPLAY_TYPE_PLAYBACK_PLUGIN, ReplayPlaybackPluginPrivate))

struct _ReplayPlaybackPluginPrivate
{
  ReplayWindow *window;
  /* state info */
  gboolean forward;
  gboolean running;
  gdouble playback_speed;
  int event_timeout_id;

  GtkActionGroup *doc_actions;
  guint           ui_id;

  GtkToolItem *separator;
  GtkToolItem *label;
  GtkToolItem *scale;
};

static void jump_to_start_cb(GtkAction *action,
                             gpointer data);
static void jump_to_end_cb(GtkAction *action,
                           gpointer data);
static void stop_cb(GtkAction *action,
                    gpointer data);
static void forwards_cb(GtkAction *action,
                        gpointer data);
static void backwards_cb(GtkAction *action,
                         gpointer data);
static void step_forward_cb(GtkAction *action,
                            gpointer data);
static void step_back_cb(GtkAction *action,
                         gpointer data);
static void increase_speed_cb(GtkAction *action,
                              gpointer data);
static void decrease_speed_cb(GtkAction *action,
                              gpointer data);

static void replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface);

static GtkActionEntry doc_entries[] =
{
  /* Playback menu */
  { "PlaybackMenuAction", NULL, N_("_Playback") },
  { "JumpToStartAction", GTK_STOCK_GOTO_FIRST, N_("Jump To Start"),
    "<control>Home", N_("Jump to start of event log"),
    G_CALLBACK(jump_to_start_cb) },
  { "JumpToEndAction", GTK_STOCK_GOTO_LAST, N_("Jump To End"), "<control>End",
    N_("Jump to end of event log"), G_CALLBACK(jump_to_end_cb) },
  { "StepBackAction", GTK_STOCK_MEDIA_PREVIOUS, N_("Step Back"), "<alt>Left",
    N_("Step back to previous event"), G_CALLBACK(step_back_cb) },
  /* would have backwards action here but cant since is a toggle action - will
   * add separately later */
  { "StopAction", GTK_STOCK_MEDIA_STOP, N_("Stop Playback"), "space",
    N_("Stop playback of events"), G_CALLBACK(stop_cb) },
  { "StepForwardAction", GTK_STOCK_MEDIA_NEXT, N_("Step Forward"), "<alt>Right",
    N_("Step forward to next event"), G_CALLBACK(step_forward_cb) },
  { "IncreaseSpeedAction", GTK_STOCK_GO_UP, N_("Increase Playback Speed"), "<alt>Up",
    N_("Increase speed of playback"), G_CALLBACK(increase_speed_cb) },
  { "DecreaseSpeedAction", GTK_STOCK_GO_DOWN, N_("Decrease Playback Speed"), "<alt>Down",
    N_("Decrease speed of playback"), G_CALLBACK(decrease_speed_cb) },
};

/* toggle document actions that apply to the currently loaded event log - any
 * defined callback must take a ReplayWindow data parameter (or NULL) */
static GtkToggleActionEntry doc_toggle_entries[] =
{
  { "BackwardsAction", GTK_STOCK_MEDIA_REWIND, N_("Run Backwards"), NULL,
    N_("Play through event log in reverse"), G_CALLBACK(backwards_cb), FALSE },
  { "ForwardsAction", GTK_STOCK_MEDIA_FORWARD, N_("Run Forwards"), NULL,
    N_("Play forwards through event log"), G_CALLBACK(forwards_cb), FALSE },
};

static const gchar * const ui =
  "<ui>"
  "<menubar name='MenuBar'>"
  "<menu name='PlaybackMenu' action='PlaybackMenuAction'>"
  "<separator />"
  "<menuitem action='IncreaseSpeedAction'/>"
  "<menuitem action='DecreaseSpeedAction'/>"
  "<separator />"
  "<menuitem action='JumpToStartAction'/>"
  "<menuitem action='JumpToEndAction'/>"
  "<menuitem action='StepBackAction'/>"
  "<menuitem action='BackwardsAction'/>"
  "<menuitem action='StopAction'/>"
  "<menuitem action='ForwardsAction'/>"
  "<menuitem action='StepForwardAction'/>"
  "</menu>"
  "</menubar>"
  "<toolbar name='ToolBar'>"
  "<separator/>"
  "<toolitem name='StepBackToolItem' action='StepBackAction'/>"
  "<toolitem name='BackwardsToolItem' action='BackwardsAction'/>"
  "<toolitem name='StopToolItem' action='StopAction'/>"
  "<toolitem name='ForwardsToolItem' action='ForwardsAction'/>"
  "<toolitem name='StepForwardToolItem' action='StepForwardAction'/>"
  "</toolbar>"
  "</ui>";

G_DEFINE_DYNAMIC_TYPE_EXTENDED(ReplayPlaybackPlugin,
                               replay_playback_plugin,
                               PEAS_TYPE_EXTENSION_BASE,
                               0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                                             replay_window_activatable_iface_init));

enum {
  PROP_WINDOW = 1,
};

static void replay_playback_plugin_dispose(GObject *object);

static void
update_doc_action_sense(ReplayPlaybackPlugin *self)
{
  ReplayPlaybackPluginPrivate *priv;
  ReplayEventStore *store;
  gboolean ret;
  GtkTreeIter current;
  GtkTreePath *path;
  gint n, i;

  /* now get current */
  priv = self->priv;

  store = replay_window_get_event_store(priv->window);
  if (store == NULL) {
    goto out;
  }
  ret = replay_event_store_get_current(store, &current);
  if (!ret) {
    goto out;
  }

  path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &current);
  i = gtk_tree_path_get_indices(path)[0];
  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store),
                                     NULL);
  /* stop should be insensitive when not running */
  gtk_action_set_sensitive(gtk_action_group_get_action(priv->doc_actions,
                                                       "StopAction"),
                           priv->event_timeout_id != 0);

  /* can run backwards only if current is not first */
  gtk_action_set_sensitive(gtk_action_group_get_action(priv->doc_actions,
                                                       "BackwardsAction"),
                           i > 0);
  gtk_action_set_sensitive(gtk_action_group_get_action(priv->doc_actions,
                                                       "StepBackAction"),
                           i > 0);

  /* can run forwards only if current is not last */
  gtk_action_set_sensitive(gtk_action_group_get_action(priv->doc_actions,
                                                       "StepForwardAction"),
                           i < n - 1);
  gtk_action_set_sensitive(gtk_action_group_get_action(priv->doc_actions,
                                                       "ForwardsAction"),
                           i < n - 1);
  gtk_tree_path_free(path);
  out:
  return;
}

static void
event_store_row_inserted_cb(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            ReplayPlaybackPlugin *self)
{
  update_doc_action_sense(self);
}

static void
event_store_current_changed_cb(ReplayEventStore *store,
                               GtkTreeIter *current,
                               ReplayPlaybackPlugin *self)
{
  update_doc_action_sense(self);
}

static void
event_store_notify_cb(ReplayWindow *window,
                      GParamSpec *pspec,
                      ReplayPlaybackPlugin *self)
{
  ReplayEventStore *store;

  /* wait until first event is inserted into event store at which point we
   * update the sensitivity of our doc actions */
  store = replay_window_get_event_store(window);
  if (store)
  {
    g_signal_connect(store, "row-inserted",
                     G_CALLBACK(event_store_row_inserted_cb), self);
    /* watch current position in event store so we set sensitivity of document
     * actions appropriately */
    g_signal_connect(store, "current-changed",
                     G_CALLBACK(event_store_current_changed_cb), self);

  }
}

static void
replay_playback_plugin_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  ReplayPlaybackPlugin *self = REPLAY_PLAYBACK_PLUGIN(object);

  switch (prop_id) {
    case PROP_WINDOW:
      self->priv->window = REPLAY_WINDOW(g_value_dup_object(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_playback_plugin_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  ReplayPlaybackPlugin *plugin = REPLAY_PLAYBACK_PLUGIN(object);

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
replay_playback_plugin_init (ReplayPlaybackPlugin *self)
{
  ReplayPlaybackPluginPrivate *priv;

  priv = self->priv = REPLAY_PLAYBACK_PLUGIN_GET_PRIVATE(self);

  priv->forward = TRUE;
  priv->event_timeout_id = 0;
  priv->playback_speed = 2.0;
}

static void
replay_playback_plugin_dispose (GObject *object)
{
  ReplayPlaybackPlugin *self = REPLAY_PLAYBACK_PLUGIN(object);
  g_clear_object(&self->priv->window);
  G_OBJECT_CLASS (replay_playback_plugin_parent_class)->dispose (object);
}

static void
doc_actions_notify_sensitive_cb(GtkActionGroup *doc_actions,
                                GParamSpec *pspec,
                                ReplayPlaybackPlugin *self) {
  gboolean sensitive = FALSE;
  ReplayPlaybackPluginPrivate *priv = self->priv;

  sensitive = gtk_action_group_get_sensitive(doc_actions);
  gtk_action_group_set_sensitive(priv->doc_actions,
                                 sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(priv->label), sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(priv->scale), sensitive);
  if (sensitive)
  {
    update_doc_action_sense(self);
  }
}

/*
 * Called to process the next event in the ReplayEventStore - we do this by setting
 * current in event list to the next event - we also look up the event in the
 * event tree to see if it has a recv / send event associated with it - if so, we
 * check to see if both the source and dest nodes have the same primary
 * parent - if they do, we skip the event
 */
static gboolean event_timeout(gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;
  ReplayEventStore *store;
  gboolean current;
  GtkTreeIter iter;
  GtkTreePath *path;
  gboolean ret = TRUE;

  g_return_val_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data), ret);

  self = REPLAY_PLAYBACK_PLUGIN(data);
  priv = self->priv;

  store = replay_window_get_event_store(priv->window);
  current = replay_event_store_get_current(store, &iter);
  if (!current)
  {
    /* just process first event */
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
                                  &iter);
    return replay_event_store_set_current(store, &iter);
  }

  path = gtk_tree_model_get_path(GTK_TREE_MODEL(store),
                                 &iter);
  /* limit to first index in path so we only process time steps */
  ret = gtk_tree_path_up(path);
  g_assert(ret);

  /* process another event from the ReplayEventStore - will return FALSE when no
   * more events to process to stop the timeout recurring - so we should unset
   * event_timeout_id */
  if (priv->forward)
  {
    gtk_tree_path_next(path);
  }
  else
  {
    ret = gtk_tree_path_prev(path);
  }

  ret &= gtk_tree_model_get_iter(GTK_TREE_MODEL(store),
                                 &iter,
                                 path);
  /* set current element if it is valid */
  if (ret)
  {
    replay_event_store_set_current(store, &iter);
  }
  else
  {
    /* set to either first or last event depending on direction we are
     * running */
    if (priv->forward)
    {
      gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                      "JumpToEndAction"));
    }
    else
    {
      gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                      "JumpToStartAction"));
    }
  }
  gtk_tree_path_free(path);

  return ret;
}


static void run(ReplayPlaybackPlugin *self)
{
  ReplayPlaybackPluginPrivate *priv;
  guint timeout;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(self));

  priv = self->priv;

  /* if not already running set stop button sensitive, otherwise stop existing
   * run and start with new timeout */
  if (priv->event_timeout_id != 0)
  {
    g_source_remove(priv->event_timeout_id);
  }
  timeout = 1000 / priv->playback_speed;
  /* run with default idle priority so we don't disrupt gtk events */
  priv->event_timeout_id = gdk_threads_add_timeout_full(G_PRIORITY_DEFAULT_IDLE,
                                                        timeout,
                                                        event_timeout,
                                                        self,
                                                        NULL);
  update_doc_action_sense(self);
}


static void step(ReplayPlaybackPlugin *self)
{
  event_timeout(self);
}

static void jump_to_start_cb(GtkAction *action,
                             gpointer data)
{
  ReplayPlaybackPluginPrivate *priv;
  GtkTreeIter iter;
  ReplayEventStore *store;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));

  priv = REPLAY_PLAYBACK_PLUGIN(data)->priv;

  /* stop then jump to start */
  gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                  "StopAction"));
  store = replay_window_get_event_store(priv->window);
  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
                                    &iter))
  {
    replay_event_store_set_current(store, &iter);
  }
}

static void jump_to_end_cb(GtkAction *action,
                           gpointer data)
{
  ReplayPlaybackPluginPrivate *priv;
  GtkTreeIter iter;
  gint n;
  GtkTreePath *path;
  ReplayEventStore *store;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));

  priv = REPLAY_PLAYBACK_PLUGIN(data)->priv;

  store = replay_window_get_event_store(priv->window);
  /* stop then jump to start */
  gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                  "StopAction"));

  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store),
                                     NULL);
  path = gtk_tree_path_new();
  gtk_tree_path_append_index(path, n - 1);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
  gtk_tree_path_free(path);
  replay_event_store_set_current(store, &iter);
}

static void stop_cb(GtkAction *action,
                    gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;
  GtkAction *forwards_action, *backwards_action;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));

  self = REPLAY_PLAYBACK_PLUGIN(data);
  priv = self->priv;

  /* make sure we're not running forwards or backwards */
  forwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                "ForwardsAction");
  backwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                 "BackwardsAction");
  gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(forwards_action), FALSE);
  gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(backwards_action), FALSE);
  if (priv->event_timeout_id != 0)
  {
    g_source_remove(priv->event_timeout_id);
    priv->event_timeout_id = 0;
  }
  update_doc_action_sense(self);
}

static void forwards_cb(GtkAction *action,
                        gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;
  GtkAction *forwards_action, *backwards_action;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  self = REPLAY_PLAYBACK_PLUGIN(data);

  priv = self->priv;
  /* run forwards */
  forwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                "ForwardsAction");
  backwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                 "BackwardsAction");
  if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(forwards_action)))
  {
    /* stop any backwards run */
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(backwards_action),
                                 FALSE);

    priv->forward = TRUE;
    run(self);
  }
  else
  {
    gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                    "StopAction"));
  }
}

static void backwards_cb(GtkAction *action,
                         gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;
  GtkAction *forwards_action, *backwards_action;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  self = REPLAY_PLAYBACK_PLUGIN(data);

  priv = self->priv;

  forwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                "ForwardsAction");
  backwards_action = gtk_action_group_get_action(priv->doc_actions,
                                                 "BackwardsAction");
  if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(backwards_action)))
  {
    /* stop any forwards run */
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(forwards_action),
                                 FALSE);

    /* run backwards */
    priv->forward = FALSE;
    run(self);
  }
  else
  {
    gtk_action_activate(gtk_action_group_get_action(priv->doc_actions,
                                                    "StopAction"));
  }
}

static void step_forward_cb(GtkAction *action,
                            gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  self = REPLAY_PLAYBACK_PLUGIN(data);

  priv = self->priv;

  priv->forward = TRUE;
  step(self);
}

static void step_back_cb(GtkAction *action,
                         gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  self = REPLAY_PLAYBACK_PLUGIN(data);
  priv = self->priv;

  priv->forward = FALSE;
  step(self);
}

static void change_speed(ReplayPlaybackPlugin *self,
                         gboolean increase)
{
  ReplayPlaybackPluginPrivate *priv;
  GtkWidget *scale;
  gdouble value;

  priv = self->priv;

  scale = gtk_bin_get_child(GTK_BIN(priv->scale));
  value = gtk_range_get_value(GTK_RANGE(scale));
  value += (increase ? 1.0 : -1.0) * 10.0;
  gtk_range_set_value(GTK_RANGE(scale), value);
}

static void increase_speed_cb(GtkAction *action,
                              gpointer data)
{
  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  change_speed(REPLAY_PLAYBACK_PLUGIN(data), TRUE);
}

static void decrease_speed_cb(GtkAction *action,
                              gpointer data)
{
  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  change_speed(REPLAY_PLAYBACK_PLUGIN(data), FALSE);
}

static void scale_value_changed(GtkRange *scale,
                                gpointer data)
{
  ReplayPlaybackPlugin *self;
  ReplayPlaybackPluginPrivate *priv;

  g_return_if_fail(REPLAY_IS_PLAYBACK_PLUGIN(data));
  self = REPLAY_PLAYBACK_PLUGIN(data);

  priv = self->priv;
  priv->playback_speed = gtk_range_get_value(scale);
  /* restart run if already running */
  if (priv->event_timeout_id != 0)
  {
    run(self);
  }
}

static void
replay_playback_plugin_activate(ReplayWindowActivatable *plugin)
{
  ReplayPlaybackPlugin *self = REPLAY_PLAYBACK_PLUGIN(plugin);
  ReplayPlaybackPluginPrivate *priv = self->priv;
  GtkActionGroup *doc_actions;
  GtkUIManager *manager;
  GtkWidget *scale;
  GtkWidget *label;
  GtkWidget *toolbar;
  gboolean sensitive;
  GError *error = NULL;

  manager = replay_window_get_ui_manager(priv->window);
  priv->doc_actions = gtk_action_group_new("ReplayPlaybackPluginDocActions");
  gtk_action_group_set_translation_domain(priv->doc_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->doc_actions, doc_entries,
                               G_N_ELEMENTS(doc_entries),
                               self);
  gtk_action_group_add_toggle_actions(priv->doc_actions, doc_toggle_entries,
                                      G_N_ELEMENTS(doc_toggle_entries),
                                      self);

  gtk_ui_manager_insert_action_group(manager, priv->doc_actions, -1);

  /* when sensitivity of the global doc actions changes, make sure we respect that */
  doc_actions = replay_window_get_doc_action_group(priv->window);
  sensitive = gtk_action_group_get_sensitive(doc_actions);
  gtk_action_group_set_sensitive(priv->doc_actions,
                                 sensitive);
  g_signal_connect(doc_actions, "notify::sensitive",
                   G_CALLBACK(doc_actions_notify_sensitive_cb), self);

  priv->ui_id = gtk_ui_manager_add_ui_from_string(manager, ui, -1, &error);
  if (!priv->ui_id)
  {
    replay_log_error(replay_window_get_log(priv->window), "playback",
                  "Error adding UI", error->message);
    g_clear_error(&error);
  }

  gtk_ui_manager_ensure_update(manager);

  /* append a separator to toolbar */
  toolbar = gtk_ui_manager_get_widget(manager, "/ToolBar");
  priv->separator = gtk_separator_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), priv->separator, -1);

  label = gtk_label_new(_("Playback speed"));
  priv->label = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(priv->label), label);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), priv->label, -1);
  gtk_widget_set_sensitive(GTK_WIDGET(priv->label), sensitive);

  /* add scale button to set speed to toolbar */
  scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 100.0, 10.0);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  gtk_widget_set_size_request(scale, 150, -1);
  gtk_range_set_value(GTK_RANGE(scale), priv->playback_speed);
  g_signal_connect(scale, "value-changed",
                   G_CALLBACK(scale_value_changed),
                   self);
  priv->scale = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(priv->scale), scale);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), priv->scale, -1);
  gtk_widget_set_sensitive(GTK_WIDGET(priv->scale), sensitive);
  /* watch for changes to the current event store of window */
  g_signal_connect(priv->window, "notify::event-store",
                   G_CALLBACK(event_store_notify_cb), self);
  event_store_notify_cb(priv->window, NULL, self);
}

static void
replay_playback_plugin_deactivate(ReplayWindowActivatable *plugin)
{
  ReplayPlaybackPlugin *self = REPLAY_PLAYBACK_PLUGIN(plugin);
  ReplayPlaybackPluginPrivate *priv = self->priv;
  GtkUIManager *manager;
  GtkActionGroup *doc_actions;
  GtkWidget *toolbar;
  ReplayEventStore *store;

  if (priv->event_timeout_id != 0)
  {
    g_source_remove(priv->event_timeout_id);
    priv->event_timeout_id = 0;
  }

  g_signal_handlers_disconnect_by_func(priv->window,
                                       G_CALLBACK(event_store_notify_cb), self);
  doc_actions = replay_window_get_doc_action_group(priv->window);
  g_signal_handlers_disconnect_by_func(doc_actions,
                                       G_CALLBACK(doc_actions_notify_sensitive_cb),
                                       self);
  manager = replay_window_get_ui_manager(priv->window);
  toolbar = gtk_ui_manager_get_widget(manager, "/ToolBar");
  gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(priv->separator));
  gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(priv->label));
  gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(priv->scale));

  gtk_ui_manager_remove_ui(manager, priv->ui_id);
  gtk_ui_manager_remove_action_group(manager, priv->doc_actions);
  g_clear_object(&priv->doc_actions);

  store = replay_window_get_event_store(priv->window);
  if (store)
  {
    g_signal_handlers_disconnect_by_func(store, event_store_current_changed_cb, self);
    g_signal_handlers_disconnect_by_func(store, event_store_row_inserted_cb, self);
  }
}

static void
replay_playback_plugin_class_init (ReplayPlaybackPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (ReplayPlaybackPluginPrivate));

  object_class->get_property = replay_playback_plugin_get_property;
  object_class->set_property = replay_playback_plugin_set_property;
  object_class->dispose = replay_playback_plugin_dispose;

  g_object_class_override_property(object_class, PROP_WINDOW, "window");
}

static void
replay_window_activatable_iface_init(ReplayWindowActivatableInterface *iface)
{
  iface->activate = replay_playback_plugin_activate;
  iface->deactivate = replay_playback_plugin_deactivate;
}

static void
replay_playback_plugin_class_finalize(ReplayPlaybackPluginClass *klass)
{
  /* nothing to do */
}

G_MODULE_EXPORT void
peas_register_types(PeasObjectModule *module)
{
  replay_playback_plugin_register_type(G_TYPE_MODULE(module));

  peas_object_module_register_extension_type(module,
                                             REPLAY_TYPE_WINDOW_ACTIVATABLE,
                                             REPLAY_TYPE_PLAYBACK_PLUGIN);
}
