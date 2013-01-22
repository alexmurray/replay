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

/*
 * A GTK Widget which inherits from GtkTreeView which displays the
 * data in an ReplayMessageTree in a causal view
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <replay/replay-events.h>
#include <replay/replay-message-tree-view.h>
#include <replay/replay-interval-list.h>

/* inherit from GtkTreeView */
G_DEFINE_TYPE(ReplayMessageTreeView, replay_message_tree_view, GTK_TYPE_TREE_VIEW);

struct _ReplayMessageTreeViewPrivate
{
  GtkCellRenderer *node_renderer;
  GtkTreeViewColumn *node_col;
  GtkCellRenderer *details_renderer;
  GtkTreeViewColumn *details_col;

  gboolean absolute_time;

  ReplayNodeTree *node_tree;
  ReplayIntervalList *interval_list;
  ReplayMessageTree *message_tree;
};

#define REPLAY_MESSAGE_TREE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_MESSAGE_TREE_VIEW, ReplayMessageTreeViewPrivate))

static gchar *replay_message_tree_view_get_event_markup(ReplayMessageTreeView *self,
                                                     GtkTreeModel *model,
                                                     GtkTreeIter *iter)
{
  ReplayMsgSendEvent *event = NULL;
  gchar *markup = NULL;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(model), markup);

  gtk_tree_model_get(model,
                     iter,
                     REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT, &event,
                     -1);

  if (event)
  {
    g_variant_lookup(replay_msg_send_event_get_props(event), "description", "s", &markup);
    g_object_unref(event);
  }

  return markup;
}

static void replay_message_tree_view_event_markup_data(GtkTreeViewColumn *tree_column,
                                                    GtkCellRenderer *cell,
                                                    GtkTreeModel *model,
                                                    GtkTreeIter *iter,
                                                    gpointer data)
{
  ReplayMessageTreeView *self;
  gchar *markup = NULL;
  gboolean current = FALSE;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(data));

  self = REPLAY_MESSAGE_TREE_VIEW(data);

  markup = replay_message_tree_view_get_event_markup(self, model, iter);
  if (markup)
  {
    /* strip whitespace from markup */
    markup = g_strstrip(markup);
  }

  g_object_set(cell, "markup", markup, NULL);
  g_free(markup);

  /* see if this event is current one - change markup in that case */
  gtk_tree_model_get(model, iter,
                     REPLAY_MESSAGE_TREE_COL_IS_CURRENT, &current,
                     -1);
  if (current)
  {
    g_object_set(cell, "weight-set", TRUE, NULL);
    g_object_set(cell, "weight", PANGO_WEIGHT_BOLD, NULL);
  }
  else
  {
    g_object_set(cell, "weight-set", FALSE, NULL);
  }
}

static void replay_message_tree_view_row_activated(GtkTreeView *tree_view,
                                                GtkTreePath *path,
                                                GtkTreeViewColumn *column)
{
  ReplayMessageTree *message_tree;
  GtkTreeIter iter;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(tree_view));

  message_tree = REPLAY_MESSAGE_TREE(gtk_tree_view_get_model(tree_view));

  gtk_tree_model_get_iter(GTK_TREE_MODEL(message_tree),
                          &iter,
                          path);

  replay_message_tree_set_current(message_tree, &iter);

  if (GTK_TREE_VIEW_CLASS(replay_message_tree_view_parent_class)->row_activated)
  {
    GTK_TREE_VIEW_CLASS(replay_message_tree_view_parent_class)->row_activated(tree_view,
                                                                           path,
                                                                           column);
  }
}

/*!
 * GObject related stuff
 */
static void replay_message_tree_view_class_init(ReplayMessageTreeViewClass *klass)
{
  GObjectClass *obj_class;
  GtkTreeViewClass *tree_view_class;

  obj_class = G_OBJECT_CLASS(klass);
  tree_view_class = GTK_TREE_VIEW_CLASS(klass);

  /* override tree-view methods */
  tree_view_class->row_activated = replay_message_tree_view_row_activated;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayMessageTreeViewPrivate));

}

static gboolean replay_message_tree_view_search_equal(GtkTreeModel *model,
                                                   gint column,
                                                   const gchar *key,
                                                   GtkTreeIter *iter,
                                                   gpointer search_data)
{
  gboolean result = TRUE; /* no match */
  gchar *markup = NULL;
  gchar *stripped_markup = NULL;

  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE(model), result);
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(search_data), result);

  markup = replay_message_tree_view_get_event_markup(REPLAY_MESSAGE_TREE_VIEW(search_data),
                                                  model, iter);
  if (markup != NULL)
  {
    /* strip markup text */
    if (pango_parse_markup(markup, -1, 0, NULL, &stripped_markup, NULL, NULL))
    {
      if (g_strrstr(stripped_markup, key) != NULL)
      {
        result = FALSE;
      }
      g_free(stripped_markup);
    }
  }
  return result;
}

static void replay_message_tree_view_node_data(GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer *cell,
                                            GtkTreeModel *model,
                                            GtkTreeIter *iter,
                                            gpointer data)
{
  ReplayMessageTreeView *self;
  ReplayMessageTreeViewPrivate *priv;
  ReplayEvent *event = NULL;
  gchar *markup;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(data));
  g_return_if_fail(REPLAY_IS_MESSAGE_TREE(model));

  self = REPLAY_MESSAGE_TREE_VIEW(data);
  priv = self->priv;

  g_return_if_fail(tree_column == priv->node_col);

  gtk_tree_model_get(model,
                     iter,
                     REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT, &event,
                     -1);

  if (event)
  {
    const gchar *id = replay_event_get_node_id(event);
    GtkTreeIter node_iter;
    gchar *label;

    if (id != NULL) {
      gboolean ret = replay_node_tree_get_iter_for_id(priv->node_tree, &node_iter, id);
      if (ret) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->node_tree),
                           &node_iter,
                           REPLAY_NODE_TREE_COL_LABEL, &label,
                           -1);
        g_object_set(cell, "markup", label, NULL);

        g_object_unref(event);
        goto out;

      }
    }
  }
  /* no send event - or couldn't get sender is unknown */
  markup = g_strdup("unknown");
  g_object_set(cell, "markup", markup, NULL);
  g_object_set(cell, "background-set", FALSE, NULL);
  g_free(markup);
out:
  return;
}

static void replay_message_tree_view_init(ReplayMessageTreeView *self)
{
  ReplayMessageTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(self));

  self->priv = priv = REPLAY_MESSAGE_TREE_VIEW_GET_PRIVATE(self);

  priv->absolute_time = REPLAY_DEFAULT_ABSOLUTE_TIME;

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);

  priv->node_renderer = gtk_cell_renderer_text_new();
  /* node renderer is always right aligned */
  g_object_set(priv->node_renderer, "xalign", 1.0, NULL);

  priv->node_col = gtk_tree_view_column_new();
  gtk_tree_view_column_pack_start(priv->node_col, priv->node_renderer,
                                  TRUE);
  gtk_tree_view_column_set_cell_data_func(priv->node_col,
                                          priv->node_renderer,
                                          replay_message_tree_view_node_data,
                                          self,
                                          NULL);
  gtk_tree_view_column_set_title(priv->node_col, "Sender");
  gtk_tree_view_append_column(GTK_TREE_VIEW(self), priv->node_col);

  priv->details_renderer = gtk_cell_renderer_text_new();
  priv->details_col = gtk_tree_view_column_new();
  gtk_tree_view_column_pack_start(priv->details_col, priv->details_renderer,
                                  TRUE);
  gtk_tree_view_column_set_cell_data_func(priv->details_col,
                                          priv->details_renderer,
                                          replay_message_tree_view_event_markup_data,
                                          self,
                                          NULL);
  gtk_tree_view_column_set_title(priv->details_col, "Message Details");
  gtk_tree_view_append_column(GTK_TREE_VIEW(self), priv->details_col);


  /* use details column as expander column */
  gtk_tree_view_set_expander_column(GTK_TREE_VIEW(self),
                                    priv->details_col);

  /* use details as search column */
  gtk_tree_view_set_search_column(GTK_TREE_VIEW(self),
                                  REPLAY_MESSAGE_TREE_COL_MSG_SEND_EVENT);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(self),
                                      TRUE);

  /* but we need to specifically do our own search compare since will try and
     compare strings against pointers to events... */
  gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(self),
                                      replay_message_tree_view_search_equal,
                                      self,
                                      NULL);

}


GtkWidget *replay_message_tree_view_new(void)
{
  GtkWidget *self;

  self = GTK_WIDGET(g_object_new(REPLAY_TYPE_MESSAGE_TREE_VIEW, NULL));
  return self;
}

static void scroll_to_current(ReplayMessageTree *message_tree,
                              GtkTreeIter *iter,
                              gpointer data)
{
  ReplayMessageTreeView *self;
  GtkTreePath *path;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(data));

  self = REPLAY_MESSAGE_TREE_VIEW(data);

  path = gtk_tree_model_get_path(GTK_TREE_MODEL(message_tree),
                                 iter);

  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(self),
                               path);

  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(self),
                               path,
                               NULL,
                               TRUE,
                               0.0,
                               0.0);

  gtk_tree_view_set_cursor(GTK_TREE_VIEW(self), path, NULL, FALSE);

  gtk_tree_path_free(path);

  /* redraw self since current has changed */
  gtk_widget_queue_draw(GTK_WIDGET(self));
}

void replay_message_tree_view_unset_data(ReplayMessageTreeView *self)
{
  ReplayMessageTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(self));

  priv = self->priv;

  if (priv->node_tree)
  {
    g_assert(priv->message_tree);
    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
    g_object_unref(priv->message_tree);
    priv->message_tree = NULL;
    gtk_tree_view_set_model(GTK_TREE_VIEW(self), NULL);
  }
}

void replay_message_tree_view_set_data(ReplayMessageTreeView *self,
                                    ReplayMessageTree *message_tree,
                                    ReplayNodeTree *node_tree)
{
  ReplayMessageTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(self));

  priv = self->priv;

  priv->node_tree = g_object_ref(node_tree);
  priv->message_tree = g_object_ref(message_tree);
  /* watch replay-event-list in ReplayMessageTree for changes in current so we can set
     cursor etc */
  g_signal_connect(priv->message_tree,
                   "current-changed",
                   G_CALLBACK(scroll_to_current),
                   self);
  /* redraw when node tree changes */
  g_signal_connect_swapped(priv->node_tree,
                           "row-changed",
                           G_CALLBACK(gtk_widget_queue_draw),
                           self);
  gtk_tree_view_set_model(GTK_TREE_VIEW(self),
                          GTK_TREE_MODEL(message_tree));
}


void replay_message_tree_view_set_absolute_time(ReplayMessageTreeView *self, gboolean absolute_time)
{
  ReplayMessageTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(self));

  priv = self->priv;
  if (priv->absolute_time != absolute_time)
  {
    priv->absolute_time = absolute_time;
    gtk_widget_queue_draw(GTK_WIDGET(self));
  }
}

gboolean replay_message_tree_view_get_absolute_time(const ReplayMessageTreeView *self)
{
  ReplayMessageTreeViewPrivate *priv;
  g_return_val_if_fail(REPLAY_IS_MESSAGE_TREE_VIEW(self), FALSE);

  priv = self->priv;
  return priv->absolute_time;
}
