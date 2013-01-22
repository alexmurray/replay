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

#include "replay-search-window.h"
#include <replay/replay-node-create-event.h>
#include <replay/replay-msg-send-event.h>

G_DEFINE_TYPE(ReplaySearchWindow, replay_search_window, GTK_TYPE_WINDOW);

static void replay_search_window_dispose(GObject *object);
static void replay_search_window_get_property(GObject *object,
                                           guint property_id, GValue *value, GParamSpec *pspec);
static void replay_search_window_set_property(GObject *object,
                                           guint property_id, const GValue *value, GParamSpec *pspec);

/* properties */
enum {
  PROP_TEXT = 1,
  PROP_STATE = 2,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY];

enum {
  SEARCH = 0,
  CANCEL = 1,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

struct _ReplaySearchWindowPrivate
{
  GtkWidget *entry;
  GIcon *error_icon;
  GIcon *active_icon;
  gchar *text;
};

static void
replay_search_window_class_init(ReplaySearchWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass, sizeof(ReplaySearchWindowPrivate));

  gobject_class->get_property = replay_search_window_get_property;
  gobject_class->set_property = replay_search_window_set_property;
  gobject_class->dispose = replay_search_window_dispose;

  properties[PROP_TEXT] = g_param_spec_string("text",
                                              "text",
                                              "text to search for.",
                                              "",
                                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property(gobject_class, PROP_TEXT, properties[PROP_TEXT]);


  properties[PROP_STATE] = g_param_spec_int("state",
                                            "state",
                                            "state",
                                            REPLAY_SEARCH_WINDOW_STATE_IDLE,
                                            REPLAY_SEARCH_WINDOW_STATE_ERROR,
                                            REPLAY_SEARCH_WINDOW_STATE_IDLE,
                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property(gobject_class, PROP_STATE, properties[PROP_STATE]);
  /* install class signals */
  /**
   * ReplaySearchWindow::search:
   * @self: The window emitting this event
   * @up: Searching up (TRUE) or down (FALSE)
   *
   * Emitted by a #ReplaySearchWindow when user clicks either up or down buttons
   */
  signals[SEARCH] = g_signal_new("search",
                                 G_OBJECT_CLASS_TYPE(gobject_class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET(ReplaySearchWindowClass,
                                                 search),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__BOOLEAN,
                                 G_TYPE_NONE,
                                 1,
                                 G_TYPE_BOOLEAN | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ReplaySearchWindow::cancel:
   * @self: The window emitting this event
   *
   * Emitted by a #ReplaySearchWindow when user clicks cancel button or presses ESC
   */
  signals[CANCEL] = g_signal_new("cancel",
                                 G_OBJECT_CLASS_TYPE(gobject_class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET(ReplaySearchWindowClass,
                                                 cancel),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);
}

static void entry_changed_cb(GtkEditable *editable,
                             gpointer data)
{
  ReplaySearchWindow *self;
  ReplaySearchWindowPrivate *priv;
  gchar *text;

  g_return_if_fail(REPLAY_IS_SEARCH_WINDOW(data));

  self = REPLAY_SEARCH_WINDOW(data);
  priv = self->priv;

  text = gtk_editable_get_chars(editable, 0, -1);
  g_free(priv->text);
  priv->text = text;
  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_TEXT]);
}

static gboolean entry_key_press_event_cb(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer data)
{
  ReplaySearchWindow *self;

  g_return_val_if_fail(REPLAY_IS_SEARCH_WINDOW(data), FALSE);

  self = REPLAY_SEARCH_WINDOW(data);

  if (event->keyval == GDK_KEY_Escape)
  {
    g_signal_emit(self, signals[CANCEL], 0);
  }
  else if (event->keyval == GDK_KEY_Up)
  {
    /* search upwards */
    g_signal_emit(self, signals[SEARCH], 0, TRUE);
  }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_Down)
  {
    /* search downwards */
    g_signal_emit(self, signals[SEARCH], 0, FALSE);
  }
  /* make sure the signal keeps propagating so text gets inserted */
  return FALSE;
}

static void
up_button_clicked_cb(GtkButton *button,
                     ReplaySearchWindow *self)
{
  g_signal_emit(self, signals[SEARCH], 0, TRUE);
}

static void
down_button_clicked_cb(GtkButton *button,
                       ReplaySearchWindow *self)
{
  g_signal_emit(self, signals[SEARCH], 0, FALSE);
}

static void
close_button_clicked_cb(GtkButton *button,
                        ReplaySearchWindow *self)
{
  g_signal_emit(self, signals[CANCEL], 0);
}

static void
replay_search_window_init(ReplaySearchWindow *self)
{
  ReplaySearchWindowPrivate *priv;
  GtkWidget *hbox, *button;
  GIcon *icon;

  self->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE(self, REPLAY_TYPE_SEARCH_WINDOW,
                                ReplaySearchWindowPrivate);

  priv->error_icon = g_themed_icon_new_with_default_fallbacks("dialog-error-symbolic");
  priv->active_icon = g_themed_icon_new_with_default_fallbacks("system-run-symbolic");

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, TRUE);

  priv->entry = gtk_entry_new();
  gtk_widget_set_can_focus(priv->entry, TRUE);
  icon = g_themed_icon_new_with_default_fallbacks("edit-find-symbolic");
  gtk_entry_set_icon_from_gicon(GTK_ENTRY(priv->entry),
                                GTK_ENTRY_ICON_PRIMARY,
                                icon);
  g_object_unref(icon);
  /* do selections when user edits entry */
  g_signal_connect(GTK_EDITABLE(priv->entry),
                   "changed",
                   G_CALLBACK(entry_changed_cb),
                   self);

  /* get key press events so we can hide entry when esc is pressed */
  g_signal_connect(priv->entry,
                   "key-press-event",
                   G_CALLBACK(entry_key_press_event_cb),
                   self);
  gtk_box_pack_start(GTK_BOX(hbox), priv->entry, TRUE, TRUE, 0);
  /* up button */
  button = gtk_button_new();
  g_signal_connect(button, "clicked",
                   G_CALLBACK(up_button_clicked_cb), self);
  gtk_widget_set_can_focus(button, FALSE);
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_icon_name("go-up-symbolic",
                                                    GTK_ICON_SIZE_MENU));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
  /* down button */
  button = gtk_button_new();
  g_signal_connect(button, "clicked",
                   G_CALLBACK(down_button_clicked_cb), self);
  gtk_widget_set_can_focus(button, FALSE);
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_icon_name("go-down-symbolic",
                                                    GTK_ICON_SIZE_MENU));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

  /* close button */
  button = gtk_button_new();
  g_signal_connect(button, "clicked",
                   G_CALLBACK(close_button_clicked_cb), self);
  gtk_widget_set_can_focus(button, FALSE);
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_icon_name("edit-delete-symbolic",
                                                    GTK_ICON_SIZE_MENU));
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(self), hbox);
}

static void
replay_search_window_get_property(GObject *object,
                               guint property_id, GValue *value, GParamSpec *pspec)
{
  ReplaySearchWindow *self = REPLAY_SEARCH_WINDOW(object);

  switch (property_id) {
    case PROP_TEXT:
      g_value_set_string(value, replay_search_window_get_text(self));
      break;
    case PROP_STATE:
      g_value_set_int(value, replay_search_window_get_state(self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
replay_search_window_set_property(GObject *object,
                               guint property_id, const GValue *value, GParamSpec *pspec)
{
  ReplaySearchWindow *self = REPLAY_SEARCH_WINDOW(object);

  switch (property_id) {
    case PROP_TEXT:
      replay_search_window_set_text(self, g_value_get_string(value));
      break;
    case PROP_STATE:
      replay_search_window_set_state(self, g_value_get_int(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}


static void
replay_search_window_dispose(GObject *object)
{
  ReplaySearchWindow *self = (ReplaySearchWindow *)object;
  ReplaySearchWindowPrivate *priv = self->priv;

  g_free(priv->text);
  priv->text = NULL;
  g_clear_object(&priv->error_icon);
  g_clear_object(&priv->active_icon);

  G_OBJECT_CLASS(replay_search_window_parent_class)->dispose(object);
}

GtkWindow *
replay_search_window_new(void)
{
  return g_object_new(REPLAY_TYPE_SEARCH_WINDOW,
                      "decorated", FALSE,
                      "focus-on-map", TRUE,
                      NULL);
}

const gchar *
replay_search_window_get_text(ReplaySearchWindow *self)
{
  g_return_val_if_fail(REPLAY_IS_SEARCH_WINDOW(self), NULL);
  return self->priv->text;
}

void
replay_search_window_set_text(ReplaySearchWindow *self,
                           const gchar *text)
{
  ReplaySearchWindowPrivate *priv;

  g_return_if_fail(REPLAY_IS_SEARCH_WINDOW(self));

  priv = self->priv;

  g_free(priv->text);
  priv->text = g_strdup(text);
  gtk_editable_delete_text(GTK_EDITABLE(priv->entry), 0, -1);
  gtk_editable_insert_text(GTK_EDITABLE(priv->entry), text, -1, NULL);
}

ReplaySearchWindowState
replay_search_window_get_state(ReplaySearchWindow *self)
{
  ReplaySearchWindowPrivate *priv;
  GIcon *icon;
  ReplaySearchWindowState state = REPLAY_SEARCH_WINDOW_STATE_IDLE;

  g_return_val_if_fail(REPLAY_IS_SEARCH_WINDOW(self), state);

  priv = self->priv;

  icon = gtk_entry_get_icon_gicon(GTK_ENTRY(priv->entry),
                                  GTK_ENTRY_ICON_SECONDARY);
  if (icon == priv->error_icon)
  {
    state = REPLAY_SEARCH_WINDOW_STATE_ERROR;
  }
  else if (icon == priv->active_icon)
  {
    state = REPLAY_SEARCH_WINDOW_STATE_ACTIVE;
  }

  return state;
}

void
replay_search_window_set_state(ReplaySearchWindow *self,
                            ReplaySearchWindowState state)
{
  ReplaySearchWindowPrivate *priv;
  GIcon *icon = NULL;

  g_return_if_fail(REPLAY_IS_SEARCH_WINDOW(self));

  priv = self->priv;

  if (state == REPLAY_SEARCH_WINDOW_STATE_ERROR)
  {
    icon = priv->error_icon;
  }
  else if (state == REPLAY_SEARCH_WINDOW_STATE_ACTIVE)
  {
    icon = priv->active_icon;
  }

  gtk_entry_set_icon_from_gicon(GTK_ENTRY(priv->entry),
                                GTK_ENTRY_ICON_SECONDARY,
                                icon);
}
