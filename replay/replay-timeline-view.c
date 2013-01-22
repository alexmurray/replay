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

#include <gtk/gtk.h>
#include <math.h>
#include <cairo/cairo-ps.h>
#include <cairo/cairo-pdf.h>
#include <replay/replay.h>
#include <replay/replay-timeline-view.h>
#include <replay/replay-event-graph.h>
#include <replay/replay-node-tree-view.h>
#include <replay/replay-timeline.h>
#include <replay/replay-message-tree.h>
#include <replay/replay-node-tree.h>

/* inherit from GtkGrid */
G_DEFINE_TYPE(ReplayTimelineView, replay_timeline_view, GTK_TYPE_GRID);

#define REPLAY_TIMELINE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_TIMELINE_VIEW, ReplayTimelineViewPrivate))

/* signals we emit */
enum
{
  RENDERING,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

struct _ReplayTimelineViewPrivate
{
  /* the actual display widgets */
  GtkWidget *event_graph;
  GtkWidget *timeline;
  GtkWidget *node_tree_view;

  /* To scroll the different views together (node_tree and event_graph scroll
     together vertically, and timeline and event_graph scroll together
     horizontally) we have two primary scrollbars */
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;

  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *vbox;

  ReplayEventStore *event_store;
  gulong times_changed_id;
};

static void replay_timeline_view_class_init(ReplayTimelineViewClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* install class signals */
  signals[RENDERING] = g_signal_new("rendering",
                                    G_OBJECT_CLASS_TYPE(obj_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET(ReplayTimelineViewClass,
                                                    rendering),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__BOOLEAN,
                                    G_TYPE_NONE,
                                    1,
                                    G_TYPE_BOOLEAN);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayTimelineViewPrivate));

}

void replay_timeline_view_set_x_resolution(ReplayTimelineView *self,
                                        gdouble x_resolution)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  replay_event_graph_set_x_resolution(REPLAY_EVENT_GRAPH(priv->event_graph),
                                   x_resolution);
}

gdouble replay_timeline_view_get_x_resolution(const ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_TIMELINE_VIEW(self), 0.0);

  priv = self->priv;

  return replay_event_graph_get_x_resolution(REPLAY_EVENT_GRAPH(priv->event_graph));
}

void replay_timeline_view_set_label_messages(ReplayTimelineView *self,
                                          gboolean label_messages)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  replay_event_graph_set_label_messages(REPLAY_EVENT_GRAPH(priv->event_graph),
                                   label_messages);
}

static void emit_rendering(ReplayTimelineView *self,
                           gboolean rendering)
{
  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  g_signal_emit(self, signals[RENDERING], 0, rendering);
}

static void replay_timeline_view_init(ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;
  GtkAdjustment *hadj, *vadj;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  self->priv = priv = REPLAY_TIMELINE_VIEW_GET_PRIVATE(self);

  /* setup the widgets which we contain
     create scrollbars */
  priv->hscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, NULL);
  priv->vscrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, NULL);

  hadj = gtk_range_get_adjustment(GTK_RANGE(priv->hscrollbar));
  vadj = gtk_range_get_adjustment(GTK_RANGE(priv->vscrollbar));

  /* timeline */
  priv->timeline = replay_timeline_new(hadj);

  /* do node tree view */
  priv->node_tree_view = replay_node_tree_view_new(vadj);

  priv->event_graph = replay_event_graph_new(hadj, vadj);

  /* propagate rendering signal from event graph */
  g_signal_connect_swapped(priv->event_graph,
                           "rendering",
                           G_CALLBACK(emit_rendering),
                           self);

  /* propagate changes in event_graph to timeline */
  g_signal_connect_swapped(priv->event_graph,
                           "x-resolution-changed",
                           G_CALLBACK(replay_timeline_set_x_resolution),
                           priv->timeline);

  /* propagate changes in event_graph to node_tree_view */
  g_signal_connect_swapped(priv->event_graph,
                           "y-resolution-changed",
                           G_CALLBACK(replay_node_tree_view_set_y_resolution),
                           priv->node_tree_view);

  priv->vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  /* have event graph resize and be able to shrink smaller than request, but
   * keep timeline fixed and allow to be made smaller */
  gtk_paned_pack1(GTK_PANED(priv->vpaned), priv->event_graph, TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(priv->vpaned), priv->timeline, FALSE, TRUE);

  priv->hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_vexpand(priv->hpaned, TRUE);
  gtk_paned_pack1(GTK_PANED(priv->hpaned), priv->node_tree_view, FALSE, TRUE);
  gtk_paned_pack2(GTK_PANED(priv->hpaned), priv->vpaned, TRUE, TRUE);

  gtk_grid_attach(GTK_GRID(self), priv->hpaned, 0, 0, 1, 1);
  gtk_widget_set_hexpand(priv->hscrollbar, TRUE);
  gtk_widget_set_vexpand(priv->hscrollbar, FALSE);
  gtk_grid_attach(GTK_GRID(self), priv->hscrollbar, 0, 1, 1, 1);

  gtk_widget_set_hexpand(priv->vscrollbar, FALSE);
  gtk_widget_set_vexpand(priv->vscrollbar, TRUE);
  gtk_grid_attach(GTK_GRID(self), priv->vscrollbar, 1, 0, 1, 2);
}

void replay_timeline_view_set_absolute_time(ReplayTimelineView *self,
                                         gboolean absolute_time)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  replay_timeline_set_absolute_time(REPLAY_TIMELINE(priv->timeline), absolute_time);
}

GtkWidget *replay_timeline_view_new(void)
{
  GtkWidget *self;

  self = GTK_WIDGET(g_object_new(REPLAY_TYPE_TIMELINE_VIEW, NULL));

  return self;
}

void replay_timeline_view_unset_data(ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  if (priv->event_store)
  {
    g_signal_handler_disconnect(priv->event_store, priv->times_changed_id);
    g_object_unref(priv->event_store);
    priv->event_store = NULL;
  }

  /* unset child widgets */
  replay_timeline_set_bounds(REPLAY_TIMELINE(priv->timeline), 0, 0);
  replay_node_tree_view_unset_data(REPLAY_NODE_TREE_VIEW(priv->node_tree_view));
  replay_event_graph_unset_data(REPLAY_EVENT_GRAPH(priv->event_graph));

  /* reset scrollbars */
  gtk_adjustment_configure(gtk_range_get_adjustment(GTK_RANGE(priv->hscrollbar)),
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  gtk_adjustment_configure(gtk_range_get_adjustment(GTK_RANGE(priv->vscrollbar)),
                           0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
}

void replay_timeline_view_set_data(ReplayTimelineView *self,
                                ReplayNodeTree *node_tree,
                                ReplayIntervalList *interval_list)
{
  ReplayTimelineViewPrivate *priv;
  GtkTreeModel *f_node_tree;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  replay_timeline_view_unset_data(self);

  priv->event_store = g_object_ref(replay_interval_list_get_event_store(interval_list));

  /* setup timeline times and connect to event list signal to keep in sync */
  replay_timeline_set_bounds(REPLAY_TIMELINE(priv->timeline),
                         replay_event_store_get_start_timestamp(priv->event_store),
                         replay_event_store_get_end_timestamp(priv->event_store));

  priv->times_changed_id = g_signal_connect_swapped(priv->event_store,
                                                    "times-changed",
                                                    G_CALLBACK(replay_timeline_set_bounds),
                                                    priv->timeline);

  /* set on object graph first so when we get change on resolution from event
     graph object graph knows to respond */
  f_node_tree = gtk_tree_model_filter_new(GTK_TREE_MODEL(node_tree),
                                          NULL);
  gtk_tree_model_filter_set_visible_column(GTK_TREE_MODEL_FILTER(f_node_tree),
                                           REPLAY_NODE_TREE_COL_REPLAYIBLE);

  replay_node_tree_view_set_data(REPLAY_NODE_TREE_VIEW(priv->node_tree_view),
                              GTK_TREE_MODEL_FILTER(f_node_tree));
  replay_event_graph_set_data(REPLAY_EVENT_GRAPH(priv->event_graph),
                           interval_list,
                           GTK_TREE_MODEL_FILTER(f_node_tree));

  g_object_unref(f_node_tree);
}

static void draw_graphs(ReplayTimelineView *self,
                        cairo_t *cr,
                        PangoLayout *layout,
                        int page_nr,
                        gdouble page_ratio)
{
  ReplayTimelineViewPrivate *priv;
  guint event_graph_width, event_graph_height;
  guint node_tree_view_width;
  guint timeline_width, timeline_height;
  gdouble fill_width, fill_height;

  priv = self->priv;

  event_graph_width = replay_event_graph_drawable_width(REPLAY_EVENT_GRAPH(priv->event_graph));
  event_graph_height = replay_event_graph_drawable_height(REPLAY_EVENT_GRAPH(priv->event_graph));
  node_tree_view_width = replay_node_tree_view_drawable_width(REPLAY_NODE_TREE_VIEW(priv->node_tree_view));
  timeline_width = replay_timeline_drawable_width(REPLAY_TIMELINE(priv->timeline));
  timeline_height = replay_timeline_drawable_height(REPLAY_TIMELINE(priv->timeline));

  /* set fill region so we don't draw into bounds of event_graph or timeline */
  fill_width = node_tree_view_width;
  fill_height = event_graph_height;
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, fill_width, fill_height);
  cairo_clip(cr);
  replay_node_tree_view_draw(REPLAY_NODE_TREE_VIEW(priv->node_tree_view), cr, layout);
  cairo_restore(cr);

  /* shift in by node_tree_view width (need to take into account scale
     though) */
  cairo_translate(cr, node_tree_view_width, 0);

  /* set clip region so we don't draw into bounds of event_graph or timeline -
     we need to respect page_ratio since this sets the end of the clip region
     properly */
  fill_width = MIN(event_graph_width*(1 - (page_nr*page_ratio)),
                   event_graph_width*page_ratio);
  fill_height = event_graph_height;
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, fill_width, fill_height);
  cairo_clip(cr);
  cairo_translate(cr, -page_ratio*page_nr*event_graph_width, 0);
  replay_event_graph_draw(REPLAY_EVENT_GRAPH(priv->event_graph), cr);
  cairo_restore(cr);

  cairo_translate(cr, 0, event_graph_height);
  /* set fill region so we don't draw into bounds of event_graph or
     node_tree_view if at last page, only supply remainder */
  fill_width = MIN(timeline_width*(1 - (page_nr*page_ratio)),
                   timeline_width*page_ratio);
  fill_height = timeline_height;
  cairo_save(cr);
  cairo_rectangle(cr, 0, 0, fill_width, fill_height);
  cairo_clip(cr);
  cairo_translate(cr, -page_ratio*page_nr*timeline_width, 0);
  replay_timeline_draw(REPLAY_TIMELINE(priv->timeline), cr, layout);
  cairo_restore(cr);
}

void replay_timeline_view_export_to_file(ReplayTimelineView *self,
                                      const char *filename)
{
  ReplayTimelineViewPrivate *priv;
  cairo_t *cr;
  cairo_surface_t *surface = NULL;
  PangoLayout *layout;
  guint event_graph_width, event_graph_height;
  guint node_tree_view_width;
  guint timeline_height;

  priv = self->priv;

  event_graph_width = replay_event_graph_drawable_width(REPLAY_EVENT_GRAPH(priv->event_graph));
  event_graph_height = replay_event_graph_drawable_height(REPLAY_EVENT_GRAPH(priv->event_graph));
  node_tree_view_width = replay_node_tree_view_drawable_width(REPLAY_NODE_TREE_VIEW(priv->node_tree_view));
  timeline_height = replay_timeline_drawable_height(REPLAY_TIMELINE(priv->timeline));

  if (g_str_has_suffix(filename, ".pdf"))
  {
    surface = cairo_pdf_surface_create(filename,
                                       (double)(node_tree_view_width + event_graph_width),
                                       (double)(event_graph_height + timeline_height));
  }
  else if (g_str_has_suffix(filename, ".ps"))
  {
    surface = cairo_ps_surface_create(filename,
                                      (double)(node_tree_view_width + event_graph_width),
                                      (double)(event_graph_height + timeline_height));
  }
  else
  {
    g_error("Invalid export file type - extensions does not match %s or %s", ".pdf", ".ps");
  }

  cr = cairo_create(surface);

  /* cr now references the surface, so we can destroy since is not needed */
  cairo_surface_destroy(surface);
  layout = pango_cairo_create_layout(cr);

  /* draw the entire thing in one go */
  draw_graphs(self, cr, layout, 0, 1);
  cairo_show_page(cr);
  g_assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

  g_object_unref(layout);
  cairo_destroy(cr);
}


void replay_timeline_view_begin_print(ReplayTimelineView *self,
                                   GtkPrintOperation *operation,
                                   GtkPrintContext *context)
{
  ReplayTimelineViewPrivate *priv;
  GtkPrintSettings *settings;
  double page_ratio;
  int num_pages;
  gdouble page_width, page_height;
  guint event_graph_height;
  guint node_tree_view_width;
  guint timeline_width, timeline_height;
  gdouble scale;

  priv = self->priv;

  /* figure out how many pages */
  settings = gtk_print_operation_get_print_settings(operation);

  page_width = gtk_print_context_get_width(context);
  page_height = gtk_print_context_get_height(context);

  event_graph_height = replay_event_graph_drawable_height(REPLAY_EVENT_GRAPH(priv->event_graph));
  node_tree_view_width = replay_node_tree_view_drawable_width(REPLAY_NODE_TREE_VIEW(priv->node_tree_view));
  timeline_width = replay_timeline_drawable_width(REPLAY_TIMELINE(priv->timeline));
  timeline_height = replay_timeline_drawable_height(REPLAY_TIMELINE(priv->timeline));

  /* always fit entire height of graph on page, then go over multiple pages if
     needed - limit scale to a max of 1 */
  scale = MIN(1.0,
              page_height / (gdouble)(event_graph_height + timeline_height));

  gtk_print_settings_set_double(settings, "event-view-scale", scale);

  /* we may run over the edge now so see how many pages we need - for each page
     re-print the object list though, and just print a new section of the
     event_graph and timeline */

  /* thus each page has node_tree_view_width*scale taken up already, so need
     to divide timeline width by (page_width - (node_tree_view_width*scale))
     to get number of pages - page_ratio is the amount of 'event_graph' or
     'timeline' which is drawn on each page */
  page_ratio = (((double)page_width / scale) - (node_tree_view_width)) / timeline_width;
  num_pages = (int)ceil( 1.0 / page_ratio);
  gtk_print_settings_set_double(settings, "event-view-page-ratio", page_ratio);

  gtk_print_operation_set_n_pages(operation, num_pages);
}


void replay_timeline_view_draw_page(ReplayTimelineView *self,
                                 GtkPrintOperation *operation,
                                 GtkPrintContext *context,
                                 int page_nr)
{
  cairo_t *cr;
  GtkPrintSettings *settings;
  gdouble scale, page_ratio;
  PangoLayout *layout;

  cr = gtk_print_context_get_cairo_context(context);
  settings = gtk_print_operation_get_print_settings(operation);
  scale = gtk_print_settings_get_double(settings, "event-view-scale");
  page_ratio = gtk_print_settings_get_double(settings, "event-view-page-ratio");
  cairo_save(cr);
  cairo_scale(cr, scale, scale);

  layout = gtk_print_context_create_pango_layout(context);

  draw_graphs(self, cr, layout, page_nr, page_ratio);
  g_assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
  g_object_unref(layout);

  cairo_restore(cr);

}

void replay_timeline_view_end_print(ReplayTimelineView *self,
                                 GtkPrintOperation *operation,
                                 GtkPrintContext *context)
{
}


void replay_timeline_view_redraw_marks(ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  gtk_widget_queue_draw(priv->event_graph);
}

void replay_timeline_view_set_hpaned_position(ReplayTimelineView *self,
                                           gint position)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  gtk_paned_set_position(GTK_PANED(priv->hpaned), position);
}

gint replay_timeline_view_get_hpaned_position(ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;
  gint position = 0;

  g_return_val_if_fail(REPLAY_IS_TIMELINE_VIEW(self), position);

  priv = self->priv;

  position = gtk_paned_get_position(GTK_PANED(priv->hpaned));

  return position;
}

void replay_timeline_view_set_vpaned_position(ReplayTimelineView *self,
                                           gint position)
{
  ReplayTimelineViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_TIMELINE_VIEW(self));

  priv = self->priv;

  gtk_paned_set_position(GTK_PANED(priv->vpaned), position);
}

gint replay_timeline_view_get_vpaned_position(ReplayTimelineView *self)
{
  ReplayTimelineViewPrivate *priv;
  gint position = 0;

  g_return_val_if_fail(REPLAY_IS_TIMELINE_VIEW(self), position);

  priv = self->priv;

  position = gtk_paned_get_position(GTK_PANED(priv->vpaned));

  return position;
}
