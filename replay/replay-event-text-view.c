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

#include <replay/replay-event.h>
#include <replay/replay-event-text-view.h>
#include <replay/replay-event-string.h>
#include <replay/replay-string-utils.h>

/* inherit from GtkTextView */
G_DEFINE_TYPE(ReplayEventTextView, replay_event_text_view, GTK_TYPE_TEXT_VIEW);

#define REPLAY_EVENT_TEXT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_EVENT_TEXT_VIEW, ReplayEventTextViewPrivate))

struct _ReplayEventTextViewPrivate
{
  ReplayEventStore *event_store;
  gboolean absolute_time;
  ReplayNodeTree *node_tree;
};

static void replay_event_text_view_dispose(GObject *object);

static void replay_event_text_view_class_init(ReplayEventTextViewClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  obj_class->dispose = replay_event_text_view_dispose;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayEventTextViewPrivate));
}


static void update_text_view(ReplayEventTextView *self,
                             GtkTreeIter *iter)
{
  ReplayEventTextViewPrivate *priv;
  GtkTextBuffer *text_buffer;
  gchar *text = NULL;

  priv = self->priv;

  text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self));

  if (priv->event_store && iter)
  {
    GList *events = NULL, *list;

    text = g_strdup("");
    events = replay_event_store_get_events(priv->event_store, iter);
    for (list = events; list != NULL; list = list->next)
    {
      ReplayEvent *event = (ReplayEvent *)list->data;
      gchar *string;

      string = replay_event_to_string(event,
                                      (priv->absolute_time ? 0 :
                                       replay_event_store_get_start_timestamp(priv->event_store)));
      text = replay_strconcat_and_free(text, string);
    }
    g_list_foreach(events, (GFunc)g_object_unref, NULL);
    g_list_free(events);
    gtk_text_buffer_set_text(text_buffer, text, -1);
    g_free(text);
  }
  else
  {
    /* clear the text buffer */
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(text_buffer, &start);
    gtk_text_buffer_get_end_iter(text_buffer, &end);
    gtk_text_buffer_delete(text_buffer, &start, &end);
  }
}

void replay_event_text_view_set_absolute_time(ReplayEventTextView *self,
                                           gboolean absolute_time)
{
  ReplayEventTextViewPrivate *priv;

  priv = self->priv;

  priv->absolute_time = absolute_time;
  if (priv->event_store)
  {
    GtkTreeIter iter;
    gboolean current = replay_event_store_get_current(priv->event_store, &iter);

    if (current)
    {
      update_text_view(self, &iter);
    }
  }
}

static void replay_event_text_view_init(ReplayEventTextView *self)
{
  PangoFontDescription *font_desc;

  g_return_if_fail(REPLAY_IS_EVENT_TEXT_VIEW(self));
  self->priv = REPLAY_EVENT_TEXT_VIEW_GET_PRIVATE(self);

  gtk_text_view_set_editable(GTK_TEXT_VIEW(self), FALSE);

  /* set to use monospace font */
  font_desc = pango_font_description_new();
  pango_font_description_set_family_static(font_desc, "monospace");
  gtk_widget_modify_font(GTK_WIDGET(self), font_desc);
  pango_font_description_free(font_desc);
}

static void replay_event_text_view_dispose(GObject *object)
{
  ReplayEventTextView *self;
  ReplayEventTextViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_TEXT_VIEW(object));

  self = REPLAY_EVENT_TEXT_VIEW(object);
  priv = self->priv;

  if (priv->event_store)
  {
    g_object_unref(priv->event_store);
    priv->event_store = NULL;
  }

  if (priv->node_tree)
  {
    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
  }
  /* call parent dispose */
  G_OBJECT_CLASS(replay_event_text_view_parent_class)->dispose(object);
}

void replay_event_text_view_unset_data(ReplayEventTextView *self)
{
  ReplayEventTextViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_TEXT_VIEW(self));
  priv = self->priv;

  if (priv->event_store)
  {
    g_assert(priv->node_tree);
    g_object_unref(priv->event_store);
    priv->event_store = NULL;

    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
  }
  update_text_view(self, NULL);
}

void replay_event_text_view_set_data(ReplayEventTextView *self,
                                  ReplayEventStore *event_store,
                                  ReplayNodeTree *node_tree)
{
  ReplayEventTextViewPrivate *priv;
  GtkTreeIter iter;
  gboolean current;

  g_return_if_fail(REPLAY_IS_EVENT_TEXT_VIEW(self));
  priv = self->priv;

  replay_event_text_view_unset_data(self);

  priv->node_tree = g_object_ref(node_tree);
  priv->event_store = g_object_ref(event_store);
  g_signal_connect_swapped(priv->event_store,
                           "current-changed",
                           G_CALLBACK(update_text_view),
                           self);
  current = replay_event_store_get_current(priv->event_store, &iter);
  if (current)
  {
    update_text_view(self, &iter);
  }
}

GtkWidget *replay_event_text_view_new(void)
{
  GtkWidget *self;

  self = GTK_WIDGET(g_object_new(REPLAY_TYPE_EVENT_TEXT_VIEW, NULL));

  return self;
}
