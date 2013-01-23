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

/*!
 * a GtkWidget which inherits from GtkDrawingArea, and which plots the Intervals
 * contained within a ReplayIntervalList
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <math.h>
#include <replay/replay-events.h>
#include <replay/replay-event-graph.h>
#include <replay/replay-node-tree.h>
#include <replay/replay-interval-list.h>
#include <replay/replay-config.h>

/* inherit from GtkDrawingArea */
G_DEFINE_TYPE(ReplayEventGraph, replay_event_graph, GTK_TYPE_DRAWING_AREA);

struct _ReplayEventGraphPrivate
{
  /* the device pixels per unit - for x direction this is number of
     pixels per microsecond, while for y direction this is the number of pixels
     per each node */
  gdouble x_resolution;
  gdouble y_resolution;

  gboolean label_messages;

  GtkAdjustment *hadj;
  gdouble hadj_value;
  GtkAdjustment *vadj;

  /* draw events onto a backing surface, and then blit this onto our
   * drawable surface, along with the marks etc - this gets done in an idle
   * callback so we don't block the mainloop for too long */
  guint render_id;
  cairo_surface_t *surface;
  gint64 render_start_timestamp;
  gint64 render_end_timestamp;

  /* we can also be called to render to a different cairo context in which case
   * we do this entirely in the main loop */
  gboolean main_loop_render;

  /* user can mark a region and zoom to that */
  gboolean marking;
  gdouble mark_start; /* pixel location of mark start */
  gdouble mark_end; /* pixel location of mark end */

  /* user can select an event */
  gboolean find_selected;
  /* iter for event found when redrawing event graph */
  GtkTreeIter *found_selected_iter;
  double selected_x;
  double selected_y;
  GList *selected_events;

  ReplayEventStore *event_store;
  gulong current_changed_id;
  gulong times_changed_id;

  /* ReplayIntervalList */
  ReplayIntervalList *interval_list;
  gulong interval_inserted_id;
  gulong interval_changed_id;

  /* filtered node_tree */
  GtkTreeModelFilter *f_node_tree;
  ReplayNodeTree *node_tree;

  /* the popup menu to allow a user to zoom to marked times and clear marks */
  GtkWidget *menu;
  GtkWidget *zoom_to_marks_item;
  GtkWidget *clear_marks_item;
};


#define REPLAY_EVENT_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_EVENT_GRAPH, ReplayEventGraphPrivate))

/* signals we emit */
enum
{
  X_RESOLUTION_CHANGED = 0,
  Y_RESOLUTION_CHANGED,
  RENDERING,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};


/* function prototypes */
static void replay_event_graph_dispose(GObject *object);
static void replay_event_graph_finalize(GObject *object);
static gboolean replay_event_graph_configure(GtkWidget *widget,
                                          GdkEventConfigure *configure);
static gboolean replay_event_graph_blit(GtkWidget *widget,
                                     cairo_t *cr);
static gboolean replay_event_graph_scroll(GtkWidget *widget,
                                       GdkEventScroll *event);
static gboolean replay_event_graph_map_event(GtkWidget *widget,
                                          GdkEventAny *event);
static gboolean replay_event_graph_motion_notify_event(GtkWidget *widget,
                                                    GdkEventMotion *event);
static gboolean replay_event_graph_button_release_event(GtkWidget *widget,
                                                     GdkEventButton *event);
static gboolean replay_event_graph_button_press_event(GtkWidget *widget,
                                                   GdkEventButton *event);
static void replay_event_graph_select_event(ReplayEventGraph *self,
                                         GtkTreeIter *iter);

/* renders the event graph */
static void _draw_event_graph(ReplayEventGraph *self,
                              cairo_t *cr,
                              PangoLayout *layout,
                              gboolean main_loop);

static gint replay_event_graph_n_nodes(const ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;

  priv = self->priv;

  return (priv->f_node_tree ?
          gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->f_node_tree), NULL) :
          0);

}

static gboolean replay_event_graph_is_plottable(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv = self->priv;
  gboolean plottable = FALSE;

  if (priv->interval_list &&
      (replay_event_store_get_start_timestamp(priv->event_store) !=
       replay_event_store_get_end_timestamp(priv->event_store)) &&
      (replay_interval_list_num_intervals(priv->interval_list,
                                       REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS) > 0 ||
       replay_interval_list_num_intervals(priv->interval_list,
                                       REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS) > 0 ||
       replay_interval_list_num_intervals(priv->interval_list,
                                       REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY) > 0 ) &&
      priv->y_resolution != 0.0)
  {
    plottable = ((replay_event_graph_n_nodes(self) > 0) &&
                 gtk_widget_get_allocated_width(GTK_WIDGET(self)) > 0 &&
                 gtk_widget_get_allocated_height(GTK_WIDGET(self)) > 0);
  }
  return plottable;
}


static gdouble replay_event_graph_height(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv = self->priv;
  gdouble height = gtk_widget_get_allocated_height(GTK_WIDGET(self));

  if (replay_event_graph_is_plottable(self))
  {
    /* height is the number of nodes in the node list with each at
       y_resolution high */
    height = replay_event_graph_n_nodes(self) * priv->y_resolution;
  }
  return height;
}

gdouble replay_event_graph_drawable_height(ReplayEventGraph *self)
{
  return replay_event_graph_height(self);
}

gdouble replay_event_graph_drawable_width(ReplayEventGraph *self)
{
  return gtk_widget_get_allocated_width(GTK_WIDGET(self));
}

static gdouble replay_event_graph_width(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv = self->priv;
  gdouble width = replay_event_graph_drawable_width(self);

  if (replay_event_graph_is_plottable(self))
  {
    gint64 diff = (replay_event_store_get_end_timestamp(priv->event_store) -
                   replay_event_store_get_start_timestamp(priv->event_store));

    width = MAX(width, priv->x_resolution*diff);
  }
  return width;
}


static void stop_idle_render(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  if (priv->render_id)
  {
    g_signal_emit(self, signals[RENDERING], 0, FALSE);
    g_source_remove(priv->render_id);
    priv->render_id = 0;
  }
  priv->render_start_timestamp = 0;
  priv->render_end_timestamp = 0;
}

static void replay_event_graph_create_new_surface(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  GtkWidget *widget;

  priv = self->priv;
  widget = GTK_WIDGET(self);

  if (gtk_widget_get_window(widget) != NULL)
  {
    cairo_surface_t *old_surface = NULL;
    cairo_operator_t op;
    gint width, height;
    cairo_t *cr;

    /* cancel any existing render */
    stop_idle_render(self);

    /* create surface using allocated self size */
    if (priv->surface)
    {
      /* hold reference on old surface so we can copy it to new surface so we
       * don't get a garbled new surface with uninitialized data */
      old_surface = cairo_surface_reference(priv->surface);
      cairo_surface_destroy(priv->surface);
      priv->surface = NULL;
    }

    width = replay_event_graph_drawable_width(self);
    height = replay_event_graph_drawable_height(self);
    priv->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               width,
                                               height);
    cr = cairo_create(priv->surface);

    /* clear new surface */
    op = cairo_get_operator(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(cr, 1.0f, 1.0f, 1.0f);
    cairo_paint(cr);
    cairo_set_operator(cr, op);

    /* copy contents of old surface to new one */
    if (old_surface)
    {
      /* copy old surface */
      cairo_set_source_surface(cr, old_surface, 0.0, 0.0);
      cairo_paint(cr);
      cairo_surface_destroy(old_surface);
    }

    /* also set properties of adjustments */
    g_object_set(priv->hadj,
                 "step-increment", replay_event_graph_drawable_width(self) * 0.1,
                 "page-increment", replay_event_graph_drawable_width(self) * 0.9,
                 NULL);

    g_object_set(priv->vadj,
                 "step-increment", gtk_widget_get_allocated_height(GTK_WIDGET(self)) * 0.1,
                 "page-increment", gtk_widget_get_allocated_height(GTK_WIDGET(self)) * 0.9,
                 NULL);
    cairo_destroy(cr);
  }
}

PangoLayout *create_pango_layout(ReplayEventGraph *self,
                                 cairo_t *cr)
{
  PangoLayout *layout = NULL;
  GtkStyleContext *style;
  const PangoFontDescription *orig_desc;
  PangoFontDescription *desc;
  gint size;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(self), NULL);

  layout = pango_cairo_create_layout(cr);

  pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

  /* use same font description as widget but reduce font size */
  style = gtk_widget_get_style_context(GTK_WIDGET(self));
  orig_desc = gtk_style_context_get_font(style,
                                         gtk_widget_get_state_flags(GTK_WIDGET(self)));
  desc = pango_font_description_copy(orig_desc);
  size = (gint)(pango_font_description_get_size(desc) * 0.9);
  pango_font_description_set_size(desc, size);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  /* set font options as defaults for current screen */
  pango_cairo_context_set_font_options(pango_layout_get_context(layout),
                                       gdk_screen_get_font_options(gdk_screen_get_default()));
  pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                     gdk_screen_get_resolution(gdk_screen_get_default()));

  return layout;
}

static void replay_event_graph_draw_region(ReplayEventGraph *self,
                                        gdouble start,
                                        gdouble width)
{
  if (gtk_widget_get_mapped(GTK_WIDGET(self)))
  {
    ReplayEventGraphPrivate *priv;
    cairo_t *cr;
    PangoLayout *layout;

    priv = self->priv;

    cr = cairo_create(priv->surface);
    layout = create_pango_layout(self, cr);

    /* if we don't yet have an event store don't bother looking for start and
       end times */
    if (priv->event_store)
    {
      gdouble visible_start, visible_end;
      gint64 prev_start_timestamp, prev_end_timestamp;
      gint64 visible_start_timestamp, visible_end_timestamp;

      prev_start_timestamp = priv->render_start_timestamp;
      prev_end_timestamp = priv->render_end_timestamp;

      stop_idle_render(self);

      visible_start = ((gdouble)replay_event_store_get_start_timestamp(priv->event_store) +
                       (priv->hadj_value / priv->x_resolution));
      visible_end = (visible_start +
                     ((gdouble)replay_event_graph_drawable_width(self) / priv->x_resolution));
      visible_start_timestamp = floor(visible_start);
      visible_end_timestamp = ceil(visible_end);

      /* if only drawing a particular region, need to take into account region
         of current draw, otherwise just draw all again */
      if (width > 0.0)
      {
        gdouble area_start, area_end;
        gint64 area_start_timestamp, area_end_timestamp;
        gdouble pixel_start, pixel_width;

        area_start = visible_start + (start / priv->x_resolution);
        area_end = area_start + (width / priv->x_resolution);
        area_start_timestamp = floor(area_start);
        area_end_timestamp = ceil(area_end);

        /* limit to visible area */
        priv->render_start_timestamp = CLAMP((prev_start_timestamp > 0 ?
                                          MIN(prev_start_timestamp, area_start_timestamp) :
                                          area_start_timestamp),
                                         visible_start_timestamp,
                                         visible_end_timestamp);
        priv->render_end_timestamp = CLAMP(MAX(prev_end_timestamp, area_end_timestamp),
                                       visible_start_timestamp,
                                       visible_end_timestamp);

        pixel_start = ((gdouble)(priv->render_start_timestamp - visible_start_timestamp) *
                       priv->x_resolution);
        pixel_width = ((gdouble)(priv->render_end_timestamp - priv->render_start_timestamp) *
                       priv->x_resolution);

        /* clip as drawing a reduced area */
        cairo_rectangle(cr, pixel_start, 0.0f, pixel_width,
                        replay_event_graph_drawable_height(self));
        cairo_clip(cr);
      }
      else
      {
        priv->render_start_timestamp = visible_start_timestamp;
        priv->render_end_timestamp  = visible_end_timestamp;
      }
    }

    _draw_event_graph(self, cr, layout, FALSE);
    g_object_unref(layout);
    cairo_destroy(cr);
  }
}

/*
 * Redraw the self - redraws to backing surface in a separate thread
 * which then and then queues a redraw_event for the widget to make sure we get
 * exposed - only bothers to redraw though if the self is currently
 * mapped
 */
static gboolean replay_event_graph_redraw(ReplayEventGraph *self)
{
  replay_event_graph_draw_region(self, 0.0, 0.0);
  return FALSE;
}

static void replay_event_graph_reset_vadj(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  gdouble value, upper, page_size;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));
  priv = self->priv;


  g_object_get(priv->vadj,
               "value", &value,
               "upper", &upper,
               "page-size", &page_size,
               NULL);
  upper = MAX(0, (replay_event_graph_height(self) -
                  gtk_widget_get_allocated_height(GTK_WIDGET(self))));

  /* make sure we don't exceed upper limit */
  if (value > upper - page_size)
  {
    gtk_adjustment_set_value(priv->vadj, upper - page_size);
  }

  /* set properties of v_adj */
  g_object_set(priv->vadj,
               "upper", upper,
               NULL);

}

static void replay_event_graph_reset_hadj(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  gdouble value, upper, page_size;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));
  priv = self->priv;

  g_object_get(priv->hadj,
               "value", &value,
               "upper", &upper,
               "page-size", &page_size,
               NULL);
  upper = MAX(0, (replay_event_graph_width(self) -
                  replay_event_graph_drawable_width(self)));

  /* make sure we don't exceed upper limit */
  if (value > upper - page_size)
  {
    gtk_adjustment_set_value(priv->hadj, upper - page_size);
  }

  /* set properties of h_adj */
  g_object_set(priv->hadj,
               "upper", upper,
               NULL);
}

static gboolean replay_event_graph_configure(GtkWidget *widget,
                                          GdkEventConfigure *configure)
{
  ReplayEventGraph *self;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), FALSE);
  self = REPLAY_EVENT_GRAPH(widget);

  /* reset upper limit of v_adj and h_adj and make sure value doesn't exceed it
  */
  replay_event_graph_reset_vadj(self);
  /* do for h_adj too */
  replay_event_graph_reset_hadj(self);

  /* create a new backing surface */
  replay_event_graph_create_new_surface(self);

  replay_event_graph_redraw(REPLAY_EVENT_GRAPH(self));

  return TRUE;
}

static gboolean replay_event_graph_map_event(GtkWidget *widget,
                                          GdkEventAny *event)
{
  ReplayEventGraph *self;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), FALSE);
  self = REPLAY_EVENT_GRAPH(widget);

  /* hand to parent map_event */
  if (GTK_WIDGET_CLASS(replay_event_graph_parent_class)->map_event)
  {
    GTK_WIDGET_CLASS(replay_event_graph_parent_class)->map_event(widget, event);
  }

  replay_event_graph_redraw(self);

  return TRUE;
}

static void f_node_tree_row_inserted(GtkTreeModel *model,
                                     GtkTreePath *path,
                                     GtkTreeIter *iter,
                                     gpointer user_data)
{
  ReplayEventGraph *self;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(user_data));

  self = REPLAY_EVENT_GRAPH(user_data);

  replay_event_graph_reset_vadj(self);
  replay_event_graph_create_new_surface(self);
  replay_event_graph_redraw(self);
}

static void f_node_tree_row_deleted(GtkTreeModel *model,
                                    GtkTreePath *path,
                                    gpointer user_data)
{
  ReplayEventGraph *self;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(user_data));

  self = REPLAY_EVENT_GRAPH(user_data);

  replay_event_graph_reset_vadj(self);
  replay_event_graph_create_new_surface(self);
  replay_event_graph_redraw(self);
}

static void replay_interval_list_interval_inserted_changed(ReplayIntervalList *interval_list,
                                                        ReplayEventInterval *interval,
                                                        gpointer user_data)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gint64 start_timestamp, end_timestamp;
  gdouble visible_start, visible_end;
  gint64 visible_start_timestamp, visible_end_timestamp;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(user_data));

  self = REPLAY_EVENT_GRAPH(user_data);
  priv = self->priv;

  replay_event_interval_get_start(interval, NULL, &start_timestamp);
  replay_event_interval_get_end(interval, NULL, &end_timestamp);

  visible_start = ((gdouble)replay_event_store_get_start_timestamp(priv->event_store) +
                   (priv->hadj_value / priv->x_resolution));
  visible_end = (visible_start +
                 (replay_event_graph_drawable_width(self) /
                  priv->x_resolution));
  visible_start_timestamp = (gint64)floor(visible_start);
  visible_end_timestamp = (gint64)ceil(visible_end);

  if ((start_timestamp >= visible_start_timestamp &&
       start_timestamp <= visible_end_timestamp) ||
      (end_timestamp >= visible_start_timestamp &&
       end_timestamp <= visible_end_timestamp) ||
      (start_timestamp < visible_start_timestamp &&
       end_timestamp > visible_end_timestamp))
  {
    /* interval is visible in time */
    double start, width;

    start = floor((gdouble)(visible_start_timestamp - start_timestamp) * priv->x_resolution);
    width = ceil((gdouble)(end_timestamp - start_timestamp) * priv->x_resolution);

    replay_event_graph_draw_region(self, start, width);
  }
}

void replay_event_graph_unset_data(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  /* stop any render */
  stop_idle_render(self);

  if (priv->interval_list)
  {
    g_assert(priv->event_store && priv->interval_list && priv->node_tree
             && priv->f_node_tree);
    g_signal_handler_disconnect(priv->event_store,
                                priv->times_changed_id);
    g_signal_handler_disconnect(priv->event_store,
                                priv->current_changed_id);
    g_signal_handler_disconnect(priv->interval_list,
                                priv->interval_inserted_id);
    g_signal_handler_disconnect(priv->interval_list,
                                priv->interval_changed_id);
    g_object_unref(priv->interval_list);
    priv->interval_list = NULL;

    g_object_unref(priv->f_node_tree);
    priv->f_node_tree = NULL;
    priv->node_tree = NULL;

    if (priv->selected_events)
    {
      g_list_foreach(priv->selected_events, (GFunc)g_object_unref, NULL);
      g_list_free(priv->selected_events);
      priv->selected_events = NULL;
    }
    /* reset canvas properties */
    priv->mark_start = -1.0;
    priv->mark_end = -1.0;

    g_object_set(priv->hadj, "value", 0.0, NULL);

    /* manually reset x res since will be ignored if we have nothing to plot */
    priv->x_resolution = REPLAY_DEFAULT_X_RESOLUTION;
    g_signal_emit(self,
                  signals[X_RESOLUTION_CHANGED],
                  0,
                  priv->x_resolution);
    replay_event_graph_create_new_surface(self);
    replay_event_graph_redraw(self);
  }
}

void replay_event_graph_set_data(ReplayEventGraph *self,
                              ReplayIntervalList *interval_list,
                              GtkTreeModelFilter *f_node_tree)
{
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));
  g_return_if_fail(interval_list != NULL);

  priv = self->priv;

  replay_event_graph_unset_data(self);

  priv->event_store = g_object_ref(replay_interval_list_get_event_store(interval_list));
  priv->interval_list = g_object_ref(interval_list);
  priv->times_changed_id = g_signal_connect_swapped(priv->event_store,
                                                    "times-changed",
                                                    G_CALLBACK(replay_event_graph_reset_hadj),
                                                    self);
  priv->current_changed_id = g_signal_connect_swapped(priv->event_store,
                                                      "current-changed",
                                                      G_CALLBACK(replay_event_graph_select_event),
                                                      self);
  priv->interval_inserted_id = g_signal_connect(priv->interval_list,
                                                "interval-inserted",
                                                G_CALLBACK(replay_interval_list_interval_inserted_changed),
                                                self);
  priv->interval_changed_id = g_signal_connect(priv->interval_list,
                                               "interval-changed",
                                               G_CALLBACK(replay_interval_list_interval_inserted_changed),
                                               self);

  priv->f_node_tree = g_object_ref(f_node_tree);
  /* keep a pointer to child node tree for convenience */
  priv->node_tree = REPLAY_NODE_TREE(gtk_tree_model_filter_get_model(f_node_tree));
  g_signal_connect(priv->f_node_tree,
                   "row-inserted",
                   G_CALLBACK(f_node_tree_row_inserted),
                   self);

  g_signal_connect(priv->f_node_tree,
                   "row-deleted",
                   G_CALLBACK(f_node_tree_row_deleted),
                   self);

  /* make sure we don't exceed max x resolution - if neither of these cause us
     to redraw, then redraw manually */
  if (!replay_event_graph_set_x_resolution(self,
                                        replay_event_graph_get_x_resolution(self)) ||
      !replay_event_graph_set_y_resolution(self,
                                        replay_event_graph_get_y_resolution(self)))
  {
    /* now set max value and redraw */
    gdouble h_upper, v_upper;
    v_upper = MAX(0, (replay_event_graph_height(self) -
                      gtk_widget_get_allocated_height(GTK_WIDGET(self))));
    h_upper = MAX(0, (replay_event_graph_width(self) -
                      replay_event_graph_drawable_width(self)));
    g_object_set(priv->hadj,
                 "upper", h_upper,
                 NULL);
    g_object_set(priv->vadj,
                 "upper", v_upper,
                 NULL);

    replay_event_graph_create_new_surface(self);
    replay_event_graph_redraw(self);
  }
}

/* x-resolution related stuff */
gboolean replay_event_graph_set_x_resolution(ReplayEventGraph *self, gdouble x_resolution)
{
  ReplayEventGraphPrivate *priv;
  gboolean changed = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(self), changed);

  priv = self->priv;

  /* only set x resolution if we actually have some data to plot - ie the
     concept of x_resolution is only valid when start and end are set and are
     not the same */
  if (replay_event_graph_is_plottable(self))
  {
    /* make sure new x-resolution is limited so we don't show anything beyond
     * the entire visible region - min resolution will be showing the entire
     * graph in the visible area */
    x_resolution = MAX(x_resolution,
                       (replay_event_graph_drawable_width(self) /
                        (replay_event_store_get_end_timestamp(priv->event_store) -
                         replay_event_store_get_start_timestamp(priv->event_store))));
    changed = priv->x_resolution != x_resolution;

    if (changed)
    {
      gdouble upper;
      gdouble value;

      /* if we have a selected event, instead of making it so the left hand edge
       * stays on same event (ie. the value of the adjustment is simply scaled)
       * instead scroll so selected event is at center of visible region with
       * new zoom level */
      if (priv->selected_events)
      {
        ReplayEvent *event = (ReplayEvent *)(priv->selected_events->data);
        value = (((replay_event_get_timestamp(event) -
                   replay_event_store_get_start_timestamp(priv->event_store)) * x_resolution) -
                 replay_event_graph_drawable_width(self) / 2);
      }
      else
      {
        value = (priv->hadj_value *
                 x_resolution / priv->x_resolution);
      }

      /* also need to scale marked region too */
      if (priv->mark_start != -1.0)
      {
        priv->mark_start *= x_resolution / priv->x_resolution;
      }
      if (priv->mark_end != -1.0)
      {
        priv->mark_end *= x_resolution / priv->x_resolution;
      }
      priv->x_resolution = x_resolution;
      upper = MAX(0, (replay_event_graph_width(self) -
                      replay_event_graph_drawable_width(self)));

      /* make sure we don't exceed upper limit */
      value = MIN(value, upper);

      /* set properties of h_adj */
      g_object_set(priv->hadj,
                   "upper", upper,
                   NULL);
      gtk_adjustment_set_value(priv->hadj, value);

      replay_event_graph_redraw(self);

      g_signal_emit(self,
                    signals[X_RESOLUTION_CHANGED],
                    0,
                    priv->x_resolution);

    }
  }
  return changed;
}

gdouble replay_event_graph_get_x_resolution(const ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(self), -1.0);

  priv = self->priv;
  return priv->x_resolution;
}


/* y-resolution related stuff */
gboolean replay_event_graph_set_y_resolution(ReplayEventGraph *self, gdouble y_resolution)
{
  ReplayEventGraphPrivate *priv;
  gboolean changed = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(self), changed);

  priv = self->priv;

  /* only set y resolution if we actually have some data to plot - ie the
     concept of y_resolution is only valid when start and end are set and are
     not the same */
  if (replay_event_graph_is_plottable(self))
  {
    changed = priv->y_resolution != y_resolution;

    if (changed)
    {
      /* need to scale value so we stay in same spot on graph */
      gdouble upper;
      gdouble value = gtk_adjustment_get_value(priv->vadj);
      value = value * y_resolution / priv->y_resolution;

      priv->y_resolution = y_resolution;
      upper = MAX(0, (replay_event_graph_height(self) -
                      gtk_widget_get_allocated_height(GTK_WIDGET(self))));

      /* make sure we don't exceed upper limit */
      value = MIN(value, upper);

      /* set properties of v_adj */
      g_object_set(priv->vadj,
                   "upper", upper,
                   NULL);

      gtk_adjustment_set_value(priv->vadj, value);

      replay_event_graph_redraw(self);

      g_signal_emit(self,
                    signals[Y_RESOLUTION_CHANGED],
                    0,
                    priv->y_resolution);

      g_object_notify(G_OBJECT(self), "y-resolution");
    }
  }

  return changed;
}
gdouble replay_event_graph_get_y_resolution(const ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(self), -1.0);

  priv = self->priv;
  return priv->y_resolution;
}

void replay_event_graph_set_label_messages(ReplayEventGraph *self,
                                        gboolean label_messages)
{
  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  if (self->priv->label_messages != label_messages)
  {
    self->priv->label_messages = label_messages;
    replay_event_graph_redraw(self);
  }
}

static int id_to_index(const ReplayEventGraph *self,
                       const gchar *id)
{
  ReplayEventGraphPrivate *priv;
  GtkTreeIter iter;
  int indice = -1;

  priv = self->priv;

  if (replay_node_tree_get_iter_for_id(priv->node_tree, &iter, id))
  {
    GtkTreePath *path, *f_path;
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->node_tree), &iter);
    f_path = gtk_tree_model_filter_convert_child_path_to_path(priv->f_node_tree,
                                                              path);
    if (f_path != NULL)
    {
      indice = gtk_tree_path_get_indices(f_path)[0];
      gtk_tree_path_free(f_path);
    }
    gtk_tree_path_free(path);
  }

  return indice;
}

static void replay_event_graph_select_event(ReplayEventGraph *self,
                                         GtkTreeIter *iter)
{
  ReplayEventGraphPrivate *priv;
  ReplayEvent *event;
  GList *events;
  gdouble value, lower, upper, page_size;
  gint width, height;
  GtkTreeIter obj_iter;
  const gchar *node = NULL;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));
  g_return_if_fail(iter != NULL);

  priv = self->priv;

  /* use all events at this timestamp */
  events = replay_event_store_get_events(priv->event_store, iter);
  if (priv->selected_events)
  {
    g_list_foreach(priv->selected_events, (GFunc)g_object_unref, NULL);
    g_list_free(priv->selected_events);
    priv->selected_events = NULL;
  }

  priv->selected_events = events;

  /* use the first event */
  event = (ReplayEvent *)(events->data);

  /* scroll to the new event in time - should cause redraw if needed */
  g_object_get(priv->hadj,
               "value", &value,
               "lower", &lower,
               "upper", &upper,
               "page-size", &page_size,
               NULL);
  width = replay_event_graph_drawable_width(self);
  height = gtk_widget_get_allocated_height(GTK_WIDGET(self));

  /* only bother if there is scrollable room */
  if (page_size < upper - lower)
  {
    /* scroll by difference from value to x position of start of
     * invocation */
    gdouble diff = ((replay_event_get_timestamp(event) -
                    replay_event_store_get_start_timestamp(priv->event_store))
                   * priv->x_resolution);
    /* this is the start of the invocation, but is nicer to center it has
     * some context - round to nearest whole-numbered pixel value */
    gdouble new_value = (gint)(diff) - ((gint)width / 2);
    /* only bother if not currently visible and place at start of visible
     * region - this will set visible region to start with left hand side at
     * the event - if we are scrolling forwards that is fine, but if we are
     * scrolling backwards, we need to set it so the right hand side is at
     * the event */
    if (new_value < value - (width / 2.0) ||
        new_value > value + (width / 2.0))
    {
      /* make sure we don't scroll past the boundaries */
      new_value = MAX(MIN(new_value, upper - page_size), lower);
      gtk_adjustment_set_value(priv->hadj, new_value);
    }
  }

  /* scroll to the new event node if it has one */
  node = replay_event_get_node_id(event);
  if (node && replay_node_tree_get_iter_for_id(priv->node_tree, &obj_iter, node))
  {
    GtkTreePath *obj_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->node_tree),
                                                    &obj_iter);
    GtkTreePath *f_path = gtk_tree_model_filter_convert_child_path_to_path(priv->f_node_tree,
                                                                           obj_path);
    if (f_path)
    {
      g_object_get(priv->vadj,
                   "value", &value,
                   "lower", &lower,
                   "upper", &upper,
                   "page-size", &page_size,
                   NULL);

      /* only bother if there is scrollable room */
      if (page_size < upper - lower)
      {
        /* this will scroll so is at top of view - we want the minimum distance
           to make it visible basically */
        gdouble new_value = gtk_tree_path_get_indices(f_path)[0]*priv->y_resolution;
        /* only bother if not already in visible region */
        if (new_value < value || new_value > value + height)
        {
          new_value -= (height - priv->y_resolution);
          new_value = MAX(MIN(new_value, upper - page_size), lower);
          gtk_adjustment_set_value(priv->vadj, new_value);
        }
      }
      gtk_tree_path_free(f_path);
    }
    gtk_tree_path_free(obj_path);
  }
  gtk_widget_queue_draw(GTK_WIDGET(self));
}


/*!
 * GObject related stuff
 */
static void replay_event_graph_class_init(ReplayEventGraphClass *klass)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  obj_class->dispose = replay_event_graph_dispose;
  obj_class->finalize = replay_event_graph_finalize;

  /* override gtk widget handler for draw and scroll etc */
  widget_class->draw = replay_event_graph_blit;
  widget_class->configure_event = replay_event_graph_configure;
  widget_class->scroll_event = replay_event_graph_scroll;
  widget_class->map_event = replay_event_graph_map_event;
  widget_class->button_press_event = replay_event_graph_button_press_event;
  widget_class->motion_notify_event = replay_event_graph_motion_notify_event;
  widget_class->button_release_event = replay_event_graph_button_release_event;

  /* install class signals */
  signals[X_RESOLUTION_CHANGED] = g_signal_new("x-resolution-changed",
                                               G_OBJECT_CLASS_TYPE(obj_class),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(ReplayEventGraphClass,
                                                               x_resolution_changed),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__DOUBLE,
                                               G_TYPE_NONE,
                                               1,
                                               G_TYPE_DOUBLE);

  signals[Y_RESOLUTION_CHANGED] = g_signal_new("y-resolution-changed",
                                               G_OBJECT_CLASS_TYPE(obj_class),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(ReplayEventGraphClass,
                                                               y_resolution_changed),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__DOUBLE,
                                               G_TYPE_NONE,
                                               1,
                                               G_TYPE_DOUBLE);

  signals[RENDERING] = g_signal_new("rendering",
                                    G_OBJECT_CLASS_TYPE(obj_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET(ReplayEventGraphClass,
                                                    rendering),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__BOOLEAN,
                                    G_TYPE_NONE,
                                    1,
                                    G_TYPE_BOOLEAN);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayEventGraphPrivate));

}

static void clear_marks_cb(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  priv->mark_start = -1.0;
  priv->mark_end = -1.0;
  priv->marking = FALSE;
  gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void zoom_to_marked_times_cb(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  /* set x resolution so we fit in from mark_start to mark_end - this will also
   * scale mark start and end so just scroll to min of them afterwards  */
  replay_event_graph_set_x_resolution(self,
                                   (priv->x_resolution *
                                    replay_event_graph_drawable_width(self) /
                                    fabs(priv->mark_end - priv->mark_start)));

  /* scroll to start of marked region - mark_end may come before mark start */
  gtk_adjustment_set_value(priv->hadj,
                           MIN(priv->mark_start, priv->mark_end));

  clear_marks_cb(self);
}

/*
 * Need to unref everything which we hold a reference to but which might hold a
 * reference to us - but make sure we don't unref them twice since dispose can
 * get called multiple times - easiest to unref every object which we have a
 * ref to
 */
static void replay_event_graph_dispose(GObject *object)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(object));

  self = REPLAY_EVENT_GRAPH(object);
  priv = self->priv;

  replay_event_graph_unset_data(self);

  if (priv->menu)
  {
    gtk_widget_destroy(priv->menu);
    priv->menu = NULL;
  }

  /* call parent dispose */
  G_OBJECT_CLASS(replay_event_graph_parent_class)->dispose(object);
}

/* Free any remaining allocated memory etc */
static void replay_event_graph_finalize(GObject *object)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(object));

  self = REPLAY_EVENT_GRAPH(object);
  priv = self->priv;

  if (priv->surface)
  {
    cairo_surface_destroy(priv->surface);
    priv->surface = NULL;
  }

  /* call parent finalize */
  G_OBJECT_CLASS(replay_event_graph_parent_class)->finalize(object);
}


static void replay_event_graph_init(ReplayEventGraph *self)
{
  ReplayEventGraphPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  self->priv = priv = REPLAY_EVENT_GRAPH_GET_PRIVATE(self);
  widget = GTK_WIDGET(self);

  priv->x_resolution = REPLAY_DEFAULT_X_RESOLUTION;
  priv->y_resolution = REPLAY_DEFAULT_Y_RESOLUTION;
  priv->mark_start = -1.0;
  priv->mark_end = -1.0;
  priv->node_tree = NULL;

  priv->menu = gtk_menu_new();
  priv->zoom_to_marks_item = gtk_menu_item_new_with_mnemonic("Zoom to _marked region");
  priv->clear_marks_item = gtk_menu_item_new_with_mnemonic("_Clear marks");
  g_signal_connect_swapped(priv->zoom_to_marks_item, "activate",
                           G_CALLBACK(zoom_to_marked_times_cb), self);
  g_signal_connect_swapped(priv->clear_marks_item, "activate",
                           G_CALLBACK(clear_marks_cb), self);
  gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu),
                        priv->zoom_to_marks_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(priv->menu),
                        priv->clear_marks_item);
  gtk_widget_show(priv->zoom_to_marks_item);
  gtk_widget_show(priv->clear_marks_item);
  gtk_menu_attach_to_widget(GTK_MENU(priv->menu), widget, NULL);

  /* set the events we care about */
  gtk_widget_add_events(widget,
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_SCROLL_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK |
                        GDK_STRUCTURE_MASK);

  /* since we have a backing surface we essentially do double-buffering so can
   * disable gtk's own double buffering */
  gtk_widget_set_app_paintable(widget, TRUE);
  gtk_widget_set_double_buffered(widget, FALSE);
}

static void do_popup_menu(ReplayEventGraph *self, GdkEventButton *event)
{
  int button, event_time;
  ReplayEventGraphPrivate *priv;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  /* set sensitivities */
  gtk_widget_set_sensitive(priv->clear_marks_item, priv->mark_start >= 0);
  gtk_widget_set_sensitive(priv->zoom_to_marks_item, priv->mark_end >= 0);

  if (event)
  {
    button = event->button;
    event_time = event->time;
  }
  else
  {
    button = 0;
    event_time = gtk_get_current_event_time();
  }
  gtk_menu_popup(GTK_MENU(priv->menu), NULL, NULL, NULL, NULL, button, event_time);
}

static void scroll_event_graph(ReplayEventGraph *self,
                               gdouble x, gdouble y)
{
  ReplayEventGraphPrivate *priv;
  gdouble value, lower, upper, page_size;

  priv = self->priv;

  if (y)
  {
    g_object_get(priv->vadj,
                 "value", &value,
                 "lower", &lower,
                 "upper", &upper,
                 "page-size", &page_size,
                 NULL);

    /* scroll by 1 node height */
    value += y;

    /* make sure we don't scroll past the boundaries */
    value = MAX(MIN(value, upper - page_size), lower);
    gtk_adjustment_set_value(priv->vadj, value);
  }

  if (x)
  {

    g_object_get(priv->hadj,
                 "value", &value,
                 "lower", &lower,
                 "upper", &upper,
                 "page-size", &page_size,
                 NULL);
    value += x;
    /* make sure we don't scroll past the boundaries */
    value = MAX(MIN(value, upper - page_size), lower);
    gtk_adjustment_set_value(priv->hadj, value);
  }
}


static gboolean replay_event_graph_scroll(GtkWidget *widget,
                                       GdkEventScroll *event)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), FALSE);

  self = REPLAY_EVENT_GRAPH(widget);
  priv = self->priv;

  if (event->direction == GDK_SCROLL_UP)
  {
    /* if holding control and scroll up, zoom in */
    if (event->state & GDK_CONTROL_MASK)
    {
      replay_event_graph_set_x_resolution(self, priv->x_resolution * 1.5);
      handled = TRUE;
    }
    /* if holding down shift and scroll up, scroll to left */
    else if (event->state & GDK_SHIFT_MASK)
    {
      /* scroll 100 pixels to left */
      scroll_event_graph(self, -100.0, 0.0);
      handled = TRUE;
    }
    /* otherwise simply scroll up by 1 node height */
    else
    {
      scroll_event_graph(self, 0.0, -priv->y_resolution);
      handled = TRUE;
    }
  }
  else if (event->direction == GDK_SCROLL_DOWN)
  {
    /* if holding control and scroll down, zoom out */
    if (event->state & GDK_CONTROL_MASK)
    {
      replay_event_graph_set_x_resolution(self, priv->x_resolution / 1.5);
      handled = TRUE;
    }
    /* if holding down shift and scroll down, scroll right */
    else if (event->state & GDK_SHIFT_MASK)
    {
      /* scroll 100 pixels to right */
      scroll_event_graph(self, 100.0, 0.0);
      handled = TRUE;
    }
    /* otherwise simply scroll down by 1 node height */
    else
    {
      scroll_event_graph(self, 0.0, priv->y_resolution);
      handled = TRUE;
    }
  }
  else if (event->direction == GDK_SCROLL_LEFT)
  {
    scroll_event_graph(self, -100.0, 0.0);
    handled = TRUE;
  }
  else if (event->direction == GDK_SCROLL_RIGHT)
  {
    scroll_event_graph(self, 100.0, 0.0);
    handled = TRUE;
  }
  return handled;
}

/* mark related stuff */
static void update_mark_end_from_event(ReplayEventGraph *self,
                                       GdkEvent *event)
{
  cairo_region_t *region;
  gdouble x, y;
  ReplayEventGraphPrivate *priv;

  priv = self->priv;

  g_assert(priv->marking);

  /* only valid when we have data to draw */
  if (replay_event_graph_is_plottable(self))
  {
    double mark;
    /* get the mark time from this event */
    if (event->type == GDK_BUTTON_PRESS)
    {
      x = event->button.x;
      y = event->button.y;
    }
    else if (event->type == GDK_MOTION_NOTIFY)
    {
      x = event->motion.x;
      y = event->motion.y;
    }
    else
    {
      return;
    }

    /* mark needs to be offset from start of visible region */
    mark = priv->hadj_value + (gdouble)x;
    /* restrict mark to be a sensible value */
    mark = MAX(0.0, mark);
    mark = CLAMP(mark, 0.0,
                 (replay_event_store_get_end_timestamp(priv->event_store) *
                  priv->x_resolution));

    /* if outside region, then scroll */
    region = gdk_window_get_visible_region(gtk_widget_get_window(GTK_WIDGET(self)));
    if (!cairo_region_contains_point(region, x, y))
    {
      if (priv->mark_end >= 0.0)
      {
        gdouble value, upper, lower, page_size;

        g_object_get(priv->hadj,
                     "value", &value,
                     "lower", &lower,
                     "upper", &upper,
                     "page-size", &page_size,
                     NULL);

        /* scroll automatically by distance from current mark_end */
        value += (mark - priv->mark_end);

        /* make sure we don't scroll past the boundaries */
        value = MAX(MIN(value, upper - page_size), lower);
        gtk_adjustment_set_value(priv->hadj, value);
      }
    }

    cairo_region_destroy(region);

    if (priv->mark_start < 0.0)
    {
      if (mark >= 0.0)
      {
        priv->mark_start = mark;
        gtk_widget_queue_draw(GTK_WIDGET(self));
      }
    }
    else if (fabs(mark - priv->mark_start) >= 5.0)
    {
      priv->mark_end = mark;
      /* redraw marks */
      gtk_widget_queue_draw(GTK_WIDGET(self));
    }
  }
}


/* mouse handling for motion notify */
static gboolean replay_event_graph_motion_notify_event(GtkWidget *widget,
                                                    GdkEventMotion *event)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), ret);
  self = REPLAY_EVENT_GRAPH(widget);
  priv = self->priv;

  /* only valid if have data to plot */
  if (replay_event_graph_is_plottable(self))
  {
    /* only redraw if we are marking */
    if (priv->marking)
    {
      update_mark_end_from_event(self, (GdkEvent *)event);
      ret = TRUE;
    }
  }

  /* call to get future events after this time - see docs for
   * GDK_POINTER_MOTION_HINT_MASK - use instead of gdk_window_get_pointer() to
   * request further motion notifies, because it also works for extension events
   * where motion notifies are provided for devices other than the core
   * pointer */
  gdk_event_request_motions(event);

  return ret;
}


static gboolean replay_event_graph_button_release_event(GtkWidget *widget,
                                                     GdkEventButton *event)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), ret);

  self = REPLAY_EVENT_GRAPH(widget);
  priv = self->priv;

  /* only valid when we have data to plot */
  if (replay_event_graph_is_plottable(self))
  {
    if (priv->marking && event->button == 1)
    {
      update_mark_end_from_event(self,
                                 (GdkEvent *)event);
      priv->marking = FALSE;
      ret = TRUE;
    }
  }
  return ret;
}

static void find_selected_event(ReplayEventGraph *self,
                                gint x, int y)
{
  ReplayEventGraphPrivate *priv;
  cairo_surface_t *surf;
  cairo_t *cr;

  priv = self->priv;

  priv->selected_x = x;
  priv->selected_y = y;
  priv->find_selected = TRUE;

  /* do render to an offscreen image source to find selected */
  surf = cairo_surface_create_similar(priv->surface,
                                      CAIRO_CONTENT_COLOR_ALPHA,
                                      cairo_image_surface_get_width(priv->surface),
                                      cairo_image_surface_get_height(priv->surface));
  cr = cairo_create(surf);
  replay_event_graph_draw(REPLAY_EVENT_GRAPH(self), cr);
  cairo_destroy(cr);
  cairo_surface_destroy(surf);
}

static gboolean replay_event_graph_button_press_event(GtkWidget *widget,
                                                   GdkEventButton *event)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), ret);
  self = REPLAY_EVENT_GRAPH(widget);
  priv = self->priv;

  /* only mark if have data to plot and was single click */
  if (replay_event_graph_is_plottable(self))
  {
    /* for single click do marks for button 1, allow user to click with button
     * 2 and drag event graph around to change view, or show menu for button 3
     * */
    if (event->type == GDK_BUTTON_PRESS)
    {
      if (event->button == 1)
      {
        /* also reset marks */
        priv->mark_start = -1.0;
        priv->mark_end = -1.0;

        priv->marking = TRUE;
        /* only redraw marks overlay */
        gtk_widget_queue_draw(widget);
        ret = TRUE;
      }
      else if (event->button == 3)
      {
        do_popup_menu(self, event);
        ret = TRUE;
      }
    }
    /* select an event if double click */
    else if (event->type == GDK_2BUTTON_PRESS)
    {
      find_selected_event(self, event->x,
                          event->y + gtk_adjustment_get_value(priv->vadj));
      ret = TRUE;
    }
  }

  return ret;
}

static gboolean draw_node_generic_event(ReplayEventGraph *self,
                                        cairo_t *cr,
                                        ReplayEvent *event,
                                        double r, double g, double b, double a)
{
  ReplayEventGraphPrivate *priv;
  double x, y;
  double start, end;
  gboolean found_selected = FALSE;
  const gchar *name;

  g_assert(event != NULL);

  priv = self->priv;

  start = replay_event_store_get_start_timestamp(priv->event_store);
  end = replay_event_store_get_end_timestamp(priv->event_store);
  /* get y position */
  name = replay_event_get_node_id(event);
  g_assert(name != NULL);

  y = (gdouble)id_to_index(self, name) + 0.5;

  if (y >= 0.0 && replay_event_get_timestamp(event) >= start && replay_event_get_timestamp(event) <= end)
  {
    /* save good state */
    cairo_save(cr);

    /* set line widths etc */
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_line_width(cr, priv->y_resolution / 5.0);

    /* save without scalings yet applied */
    cairo_save(cr);

    /* scale by resolutions */
    cairo_scale(cr, priv->x_resolution, priv->y_resolution);

    /* get x position */
    x = replay_event_get_timestamp(event) - start;

    g_assert(x >= 0);

    /* take into account scroll position */
    x -= (priv->hadj_value / priv->x_resolution);

    cairo_move_to(cr, x, y);

    cairo_rel_line_to(cr, 1.0/priv->x_resolution, 0);

    /* restore to set line widths etc */
    cairo_restore(cr);

    /* set line widths etc - do as larger for searching if selected */
    cairo_set_line_width(cr, priv->y_resolution / 2.5);

    /* now see if is selected or not */
    if (priv->find_selected &&
        cairo_in_stroke(cr, priv->selected_x, priv->selected_y))
    {
      found_selected = TRUE;
    }
    cairo_set_line_width(cr, priv->y_resolution / 5.0);
    cairo_stroke(cr);
    /* restore to good state */
    cairo_restore(cr);

  }
  return found_selected;
}

static gboolean draw_node_created_deleted(ReplayEventGraph *self,
                                          cairo_t *cr,
                                          ReplayEvent *info)
{
  gboolean found_selected = FALSE;
  /* do event in black */
  if (info)
  {
    found_selected = draw_node_generic_event(self, cr, info,
                                             0, 0, 0, 1);
  }
  return found_selected;
}

static gboolean draw_message_pass(ReplayEventGraph *self,
                                  cairo_t *cr,
                                  PangoLayout *layout,
                                  ReplayMsgSendEvent *a,
                                  ReplayMsgRecvEvent *b)
{
  /* draw a line from a to b if both are non-null, otherwise draw a
     line from kernel to a or kernel to b */
  ReplayEventGraphPrivate *priv;
  double x[2], y[2], dx, dy, max_x, angle;
  double start, end;
  gboolean found_selected = FALSE;
  gdouble scroll_ms;
  gchar *style;

  priv = self->priv;

  start = replay_event_store_get_start_timestamp(priv->event_store);
  end = replay_event_store_get_end_timestamp(priv->event_store);

  if (a != NULL)
  {
    /* get x[0] time (a) */
    x[0] = replay_event_get_timestamp(REPLAY_EVENT(a)) - start;

    /* get y[0] position (a) */
    y[0] = id_to_index(self, replay_msg_send_event_get_node(a))
      + 0.5;

    if (b != NULL)
    {
      x[1] = replay_event_get_timestamp(REPLAY_EVENT(b)) - start;

      /* get y[1] position (b) */
      y[1] = id_to_index(self, replay_msg_recv_event_get_node(b))
        + 0.5;
    }
    else
    {
      x[1] = x[0];
      y[1] = 0.0;
    }
  }
  else
  {
    if (b != NULL)
    {
      x[1] = replay_event_get_timestamp(REPLAY_EVENT(b)) - start;

      /* get y[1] position (b) */
      y[1] = id_to_index(self, replay_msg_recv_event_get_node(b))
        + 0.5;
    }
    else
    {
      /* can't draw if have no events to draw */
      return found_selected;
    }

    x[0] = x[1];
    /* set to kernel height */
    y[0] = 0.0;
  }

  g_assert(x[0] <= x[1]);

  /* get max x position */
  max_x = end - start;

  /* skip if same group */
  if (x[1] < 0 || x[0] > max_x || y[0] < 0 || y[1] < 0 ||
      (y[0] == y[1] && (g_ascii_strcasecmp(replay_msg_send_event_get_node(a), replay_msg_recv_event_get_node(b)) != 0)))
  {
    return found_selected;
  }

  /* take into account scroll position */
  scroll_ms = priv->hadj_value / priv->x_resolution;
  x[0] -= scroll_ms;
  x[1] -= scroll_ms;

  /* save good state */
  cairo_save(cr);

  /* save this state before scaling */
  cairo_save(cr);

  /* scale by resolutions */
  cairo_scale(cr, priv->x_resolution, priv->y_resolution);

  cairo_move_to(cr, x[0], y[0]);
  /* if is from one node to itself, draw as an arc */
  if (y[0] == y[1])
  {
    cairo_curve_to(cr,
                   x[0] + ((x[1] - x[0]) / 3.0),
                   y[0] - 0.45,
                   x[0] + (2.0*((x[1] - x[0]) / 3.0)),
                   y[0] - 0.45,
                   x[1],
                   y[1]);
  }
  else
  {
    cairo_line_to(cr, x[1], y[1]);
  }

  /* draw arrow head with length of arms on arrowhead of 0.25*y_resolution */
  dy = 0.25;
  dx = 0.25*(priv->y_resolution / priv->x_resolution);

  /* arrowhead - y axis is inverted so need to negate it */
  angle = atan(-(y[1] - y[0]) / (x[1] - x[0])*(priv->y_resolution / priv->x_resolution));

  /* each part of arrowhead has angle (angle +/- pi/4) */
  cairo_rel_move_to(cr,
                    /* need to move back along x axis */
                    -dx*cos(angle + G_PI_4),
                    /* since y is inverted anyway */
                    dy*sin(angle + G_PI_4));
  cairo_line_to(cr, x[1], y[1]);

  cairo_rel_move_to(cr,
                    -dx*cos(angle - G_PI_4),
                    dy*sin(angle - G_PI_4));
  cairo_line_to(cr, x[1], y[1]);

  /* restore to line width etc state */
  cairo_restore(cr);

  /* set line widths etc - do as 2.0 for searching if selected */
  cairo_set_line_width(cr, 2.0);

  /* now see if is selected or not */
  if (priv->find_selected &&
      cairo_in_stroke(cr, priv->selected_x, priv->selected_y))
  {
    found_selected = TRUE;
  }
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_set_line_width(cr, 0.5);

  if (g_variant_lookup(replay_msg_send_event_get_props(a), "line-style", "s", &style))
  {
    if (g_ascii_strcasecmp(style, "dashed") == 0)
    {
      double dash = 5.0;
      cairo_set_dash(cr, &dash, 1, 0.0);
    }
    else if (g_ascii_strcasecmp(style, "dotted") == 0)
    {
      double dashes[] = { 1.0, 5,0 };
      cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.0);
    }
    g_free(style);
  }
  cairo_stroke(cr);
  if (priv->label_messages)
  {
    gchar *description;
    if (g_variant_lookup(replay_msg_send_event_get_props(a), "description", "s", &description))
    {
      gint width;
      gint text_width, text_height;

      dx = ((x[1] - x[0])*priv->x_resolution);
      dy = ((y[1] - y[0])*priv->y_resolution);
      width =  (int)sqrtf(dx * dx + dy * dy);
      pango_layout_set_width(layout, width*PANGO_SCALE);
      pango_layout_set_markup(layout, description, -1);
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
      if (text_width <= width)
      {
        angle = atanf(((y[1] - y[0]) * priv->y_resolution) /
                      ((x[1] - x[0]) * priv->x_resolution));
        /* move above text height */
        cairo_move_to(cr,
                      x[0]*priv->x_resolution + text_height*sinf(angle),
                      y[0]*priv->y_resolution - text_height*cosf(angle));
        cairo_rotate(cr, angle);
        pango_cairo_show_layout(cr, layout);
      }
      g_free(description);
    }
  }

  /* restore to original state */
  cairo_restore(cr);
  return found_selected;
}

static gboolean draw_node_active(ReplayEventGraph *self,
                                 cairo_t *cr,
                                 ReplayActivityStartEvent *start_event,
                                 ReplayActivityEndEvent *end_event)
{
  ReplayEventGraphPrivate *priv;
  double y, start_x, end_x, max_x;
  gint64 start, end;
  gboolean found_selected = FALSE;
  double scroll_ms;
  GdkRGBA rgba;
  double alpha = 1.0;
  GVariant *props;
  gchar *color;
  const gchar *node;

  g_return_val_if_fail(start_event, found_selected);
  node = replay_activity_start_event_get_node(start_event);
  g_return_val_if_fail(!end_event ||
                       (g_ascii_strcasecmp(node,
                                           replay_activity_end_event_get_node(end_event)) == 0),
                       found_selected);

  priv = self->priv;

  start = replay_event_store_get_start_timestamp(priv->event_store);
  end = replay_event_store_get_end_timestamp(priv->event_store);

  /* get max x position */
  max_x = end - start;

  /* get start x position - if no start position then use initial start time -
   * SHOULD PROBABLY USE TIME OF NODE CREATION!!! */
  start_x = MAX(replay_event_get_timestamp(REPLAY_EVENT(start_event)) - start, 0);
  /* get end x position - if no end_event then use final time in event list -
   * SHOULD PROBABLY USE TIME OF NODE DELETION!!! */
  end_x = MIN((end_event ? replay_event_get_timestamp(REPLAY_EVENT(end_event)) : end) - start, max_x);

  /* get y position */
  y = id_to_index(self, node) + 0.5;

  /* if end_x is less than zero or start_x is greater than max_x then don't use
     this event - also if has NO duration then is not worth showing either */
  if (end_x < 0 || start_x > max_x || y < 0 || end_x == start_x)
  {
    return found_selected;
  }

  /* take into account scroll position now so we can limit the length of our
   * straight line */
  scroll_ms = (priv->hadj_value / priv->x_resolution);
  start_x = MAX(0, start_x - scroll_ms);
  end_x = MIN((gdouble)replay_event_graph_drawable_width(self) / priv->x_resolution,
              MAX(0, end_x - scroll_ms));

  /* save initial state */
  cairo_save(cr);

  cairo_save(cr);

  /* scale by resolutions */
  cairo_scale(cr, priv->x_resolution, priv->y_resolution);

  /* do running line */
  cairo_move_to(cr, start_x, y);
  cairo_rel_line_to(cr, end_x - start_x, 0);

  /* restore to line width state etc */
  cairo_restore(cr);

  /* set bigger line width to try and find_selected */
  cairo_set_line_width(cr, priv->y_resolution);

  if (priv->find_selected &&
      cairo_in_stroke(cr, priv->selected_x, priv->selected_y))
  {
    found_selected = TRUE;
  }
  cairo_set_line_width(cr, priv->y_resolution / 1.2);

  /* colourise based on activity colour at intensity */
  props = replay_activity_start_event_get_props(start_event);
  if (g_variant_lookup(props, "color", "s", &color))
  {
    gdk_rgba_parse(&rgba, color);
    g_free(color);
  }
  else
  {
    gdk_rgba_parse(&rgba, "#999");
  }

  /* overwrite alpha with level if it exists */
  g_variant_lookup(props, "level", "d", &rgba.alpha);
  cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, alpha);

  cairo_stroke(cr);
  /* restore to original state */
  cairo_restore(cr);
  return found_selected;
}

static gboolean draw_node_color(ReplayEventGraph *self,
                                cairo_t *cr,
                                ReplayEvent *start_event,
                                ReplayEvent *end_event)
{
  ReplayEventGraphPrivate *priv;
  double y, start_x, end_x, max_x;
  double start, end;
  gboolean found_selected = FALSE;
  double scroll_ms;
  gchar *color_string = NULL;
  GVariant *props;
  GdkRGBA color;
  const gchar *node;

  g_return_val_if_fail(start_event, found_selected);
  g_return_val_if_fail((REPLAY_IS_NODE_CREATE_EVENT(start_event) ||
                        REPLAY_IS_NODE_PROPS_EVENT(start_event)) &&
                       (!end_event || REPLAY_IS_NODE_DELETE_EVENT(end_event) ||
                        REPLAY_IS_NODE_PROPS_EVENT(end_event)), found_selected);
  node = (REPLAY_IS_NODE_CREATE_EVENT(start_event) ?
          replay_node_create_event_get_id(REPLAY_NODE_CREATE_EVENT(start_event)) :
          replay_node_props_event_get_id(REPLAY_NODE_PROPS_EVENT(start_event)));
  props = (REPLAY_IS_NODE_CREATE_EVENT(start_event) ?
          replay_node_create_event_get_props(REPLAY_NODE_CREATE_EVENT(start_event)) :
          replay_node_props_event_get_props(REPLAY_NODE_PROPS_EVENT(start_event)));
  /* check node names match */
  g_return_val_if_fail(!end_event ||
                       (g_ascii_strcasecmp(node,
                                           replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(end_event))) == 0) ||
                       (g_ascii_strcasecmp(node,
                                           replay_node_props_event_get_id(REPLAY_NODE_PROPS_EVENT(end_event))) == 0),
                       found_selected);

  priv = self->priv;

  start = replay_event_store_get_start_timestamp(priv->event_store);
  end = replay_event_store_get_end_timestamp(priv->event_store);

  /* get max x position */
  max_x = end - start;

  /* get start x position  */
  start_x = replay_event_get_timestamp(start_event) - start;
  /* get end x position - if no end_event then use final time in event list -
   * TODO: SHOULD PROBABLY USE TIME OF NODE DELETION!!! */
  end_x = (end_event ? replay_event_get_timestamp(end_event) : end) - start;

  /* get y position */
  y = id_to_index(self, node) + 0.5;


  /* if end_x is less than zero or start_x is greater than max_x then
     don't use this event */
  if (end_x < 0 || start_x > max_x || y < 0)
  {
    return found_selected;
  }

  start_x = MAX(start_x, 0);
  end_x = MIN(end_x, max_x);

  g_assert(start_x >= 0 && end_x >= 0);
  g_assert(start_x <= end_x);

  /* take into account scroll position now so we can limit the length of our
   * straight line */
  scroll_ms = (priv->hadj_value / priv->x_resolution);
  start_x = MAX(0, start_x - scroll_ms);
  end_x = MIN((gdouble)replay_event_graph_drawable_width(self) / priv->x_resolution,
              MAX(0, end_x - scroll_ms));

  /* save initial state */
  cairo_save(cr);

  cairo_save(cr);

  /* scale by resolutions */
  cairo_scale(cr, priv->x_resolution, priv->y_resolution);

  /* do color line */
  cairo_move_to(cr, start_x, y);
  cairo_rel_line_to(cr, end_x - start_x, 0);

  /* restore to line width state etc */
  cairo_restore(cr);

  /* set bigger line width to try and find_selected */
  cairo_set_line_width(cr, priv->y_resolution);

  if (priv->find_selected &&
      cairo_in_stroke(cr, priv->selected_x, priv->selected_y))
  {
    found_selected = TRUE;
  }
  cairo_set_line_width(cr, priv->y_resolution / 10);

  /* colourise based on colour */
  g_variant_lookup(props, "color", "s", &color_string);
  gdk_rgba_parse(&color, color_string);
  g_free(color_string);
  cairo_set_source_rgba(cr,
                        color.red,
                        color.green,
                        color.blue,
                        0.2);

  cairo_stroke(cr);
  /* restore to original state */
  cairo_restore(cr);
  return found_selected;
}


static void draw_marks(ReplayEventGraph *self, cairo_t *cr)
{
  ReplayEventGraphPrivate *priv;
  GtkWidget *widget;

  widget = GTK_WIDGET(self);
  priv = self->priv;

  if (replay_event_graph_is_plottable(self))
  {
    GdkRGBA select_color;
    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    gtk_style_context_get_background_color(style,
                                           GTK_STATE_FLAG_SELECTED,
                                           &select_color);

    /* draw selected region */
    if (priv->mark_start >= 0.0 && priv->mark_end >= 0.0)
    {
      gdouble offset = priv->hadj_value;
      cairo_save(cr);
      /* draw rectangle filled in selection colour */
      cairo_set_line_width(cr, 0.5);
      cairo_set_source_rgba(cr,
                            select_color.red,
                            select_color.green,
                            select_color.blue,
                            select_color.alpha);

      cairo_move_to(cr, priv->mark_start - offset, 0.0);
      cairo_line_to(cr, priv->mark_start - offset, gtk_widget_get_allocated_height(GTK_WIDGET(self)));
      cairo_stroke(cr);
      cairo_move_to(cr, priv->mark_end - offset, 0.0);
      cairo_line_to(cr, priv->mark_end - offset, gtk_widget_get_allocated_height(GTK_WIDGET(self)));
      cairo_stroke(cr);
      cairo_set_source_rgba(cr,
                            select_color.red,
                            select_color.green,
                            select_color.blue,
                            select_color.alpha * 0.2);
      cairo_rectangle(cr, priv->mark_start - offset, 0.0,
                      priv->mark_end - priv->mark_start,
                      gtk_widget_get_allocated_height(GTK_WIDGET(self)));
      cairo_fill(cr);
      cairo_restore(cr);
    }
    /* draw current event time positions */
    if (priv->selected_events)
    {
      double x;
      double x_len;
      double y_len;
      GList *events;
      ReplayEvent *event;

      event = (ReplayEvent *)priv->selected_events->data;
      x = replay_event_get_timestamp(event) - replay_event_store_get_start_timestamp(priv->event_store);
      x -= (priv->hadj_value / priv->x_resolution);

      x_len = (replay_event_graph_drawable_width(self)) /
        priv->x_resolution;
      y_len = (gtk_widget_get_allocated_height(GTK_WIDGET(self))) /
        priv->y_resolution;

      /* save initial state */
      cairo_save(cr);

      cairo_set_source_rgba(cr,
                            select_color.red,
                            select_color.green,
                            select_color.blue,
                            select_color.alpha * 0.5);
      cairo_set_line_width(cr, 0.5);

      cairo_save(cr);

      /* scale by resolutions */
      cairo_scale(cr, priv->x_resolution, priv->y_resolution);
      cairo_move_to(cr, x, 0);
      cairo_line_to(cr, x, y_len);

      /* restore to line widths */
      cairo_restore(cr);
      cairo_stroke(cr);

      /* also draw region to where we are up to */
      cairo_save(cr);

      /* scale by resolutions */
      cairo_scale(cr, priv->x_resolution, priv->y_resolution);

      cairo_rectangle(cr, 0, 0, x, y_len);
      cairo_set_source_rgba(cr,
                            select_color.red,
                            select_color.green,
                            select_color.blue,
                            select_color.alpha * 0.1);
      cairo_fill(cr);

      /* restore to line widths */
      cairo_restore(cr);

      /* restore initial state */
      cairo_restore(cr);

      for (events = priv->selected_events;
           events != NULL;
           events = events->next)
      {
        double y;
        const gchar *name;

        event = (ReplayEvent *)(events->data);
        name = replay_event_get_node_id(event);

        if (name != NULL)
        {
          y = id_to_index(self, name) + 0.5;
          y -= (gtk_adjustment_get_value(priv->vadj) / priv->y_resolution);

          /* save initial state */
          cairo_save(cr);

          cairo_set_source_rgba(cr,
                                select_color.red,
                                select_color.green,
                                select_color.blue,
                                select_color.alpha * 0.5);
          cairo_set_line_width(cr, 0.5);

          cairo_save(cr);

          /* scale by resolutions */
          cairo_scale(cr, priv->x_resolution, priv->y_resolution);

          cairo_move_to(cr, 0, y);
          cairo_line_to(cr, x_len, y);

          /* restore to line widths */
          cairo_restore(cr);
          cairo_stroke(cr);

          /* restore initial state */
          cairo_restore(cr);
        }
      }
    }

  }

}

static void draw_interval(ReplayEventGraph *self,
                          cairo_t *cr,
                          PangoLayout *layout,
                          ReplayEventInterval *interval)
{
  gboolean found_selected_start = FALSE;
  gboolean found_selected_end = FALSE;
  GtkTreeIter start_iter, end_iter;
  ReplayEvent *start, *end;
  ReplayEventGraphPrivate *priv;
  ReplayEventIntervalType interval_type;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  /* get start and end */
  interval_type = replay_event_interval_get_interval_type(interval);
  replay_event_interval_get_start(interval, &start_iter, NULL);
  replay_event_interval_get_end(interval, &end_iter, NULL);
  start = replay_event_store_get_event(priv->event_store, &start_iter);
  end = replay_event_store_get_event(priv->event_store, &end_iter);

  if (start == end)
  {
    switch (interval_type)
    {
      case REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS:
        /* do we have the start or end of a node exists interval */
        g_assert(REPLAY_IS_NODE_CREATE_EVENT(start) ||
                 REPLAY_IS_NODE_DELETE_EVENT(start));
        found_selected_start = draw_node_created_deleted(self,
                                                         cr,
                                                         start);
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS:
        /* do we have the start or end of a msg pass interval */
        g_assert(REPLAY_IS_MSG_SEND_EVENT(start) ||
                 REPLAY_IS_MSG_RECV_EVENT(start));
        if (REPLAY_IS_MSG_SEND_EVENT(start))
        {
          found_selected_start = draw_message_pass(self,
                                                   cr,
                                                   layout,
                                                   REPLAY_MSG_SEND_EVENT(start),
                                                   NULL);
        }
        else
        {
          found_selected_start = draw_message_pass(self,
                                                   cr,
                                                   layout,
                                                   NULL,
                                                   REPLAY_MSG_RECV_EVENT(start));
        }
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR:
        /* do we have the start or end of a node color interval */
        g_assert(REPLAY_IS_NODE_CREATE_EVENT(start) ||
                 REPLAY_IS_NODE_PROPS_EVENT(start));
        found_selected_start = draw_node_color(self,
                                               cr,
                                               start,
                                               NULL);
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY:
        g_assert(REPLAY_IS_ACTIVITY_START_EVENT(start));
        found_selected_start = draw_node_active(self,
                                                cr,
                                                REPLAY_ACTIVITY_START_EVENT(start),
                                                NULL);
        break;

      case REPLAY_EVENT_INTERVAL_NUM_TYPES:
      default:
        g_assert_not_reached();
        break;
    }
  }
  else
  {
    switch (interval_type)
    {
      case REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS:
        g_assert(REPLAY_IS_NODE_CREATE_EVENT(start) &&
                 REPLAY_IS_NODE_DELETE_EVENT(end));
        found_selected_start = draw_node_created_deleted(self,
                                                         cr,
                                                         start);
        found_selected_end = draw_node_created_deleted(self,
                                                       cr,
                                                       end);
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS:
        g_assert(REPLAY_IS_MSG_SEND_EVENT(start) &&
                 REPLAY_IS_MSG_RECV_EVENT(end));
        found_selected_start = draw_message_pass(self,
                                                 cr,
                                                 layout,
                                                 REPLAY_MSG_SEND_EVENT(start),
                                                 REPLAY_MSG_RECV_EVENT(end));
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_NODE_COLOR:
        g_assert((REPLAY_IS_NODE_CREATE_EVENT(start) ||
                  REPLAY_IS_NODE_PROPS_EVENT(start)) &&
                 (REPLAY_IS_NODE_DELETE_EVENT(end) ||
                  REPLAY_IS_NODE_PROPS_EVENT(end)));
        found_selected_start = draw_node_color(self,
                                               cr,
                                               start,
                                               end);
        break;

      case REPLAY_EVENT_INTERVAL_TYPE_NODE_ACTIVITY:
        g_assert(REPLAY_IS_ACTIVITY_START_EVENT(start) &&
                 REPLAY_IS_ACTIVITY_END_EVENT(end));
        found_selected_start = draw_node_active(self,
                                                cr,
                                                REPLAY_ACTIVITY_START_EVENT(start),
                                                REPLAY_ACTIVITY_END_EVENT(end));
        break;

      case REPLAY_EVENT_INTERVAL_NUM_TYPES:
      default:
        g_assert_not_reached();
        break;
    }

  }
  if (found_selected_start || found_selected_end)
  {
    priv->found_selected_iter = gtk_tree_iter_copy(found_selected_start ?
                                                   &start_iter :
                                                   &end_iter);
  }
  g_object_unref(start);
  g_object_unref(end);
}

typedef struct _IdleRenderData
{
  ReplayEventGraph *self;
  cairo_t *cr;
  PangoLayout *layout;
  gboolean main_loop;
  GList *intervals;
  guint id;
} IdleRenderData;

static void free_idle_render_data(IdleRenderData *data)
{
  g_list_foreach(data->intervals, (GFunc)replay_event_interval_unref, NULL);
  g_list_free(data->intervals);
  g_object_unref(data->layout);
  cairo_destroy(data->cr);
  g_free(data);
}

static gboolean render_intervals_with_timeout(ReplayEventGraph *self,
                                              cairo_t *cr,
                                              PangoLayout *layout,
                                              GList **intervals,
                                              guint timeout)
{
  ReplayEventGraphPrivate *priv;
  GTimer *timer = NULL;

  priv = self->priv;

  if (timeout)
  {
    timer = g_timer_new();
  }

  while (*intervals != NULL)
  {
    ReplayEventInterval *interval;

    interval = (ReplayEventInterval *)((*intervals)->data);
    /* if timeout specified respect it */
    if (!timeout || (guint)(g_timer_elapsed(timer, NULL) * 1000.0) < timeout)
    {
      draw_interval(self, cr, layout, interval);
    }
    else
    {
      /* break out of loop since timer has expired */
      break;
    }
    replay_event_interval_unref(interval);
    *intervals = g_list_delete_link(*intervals, *intervals);
  }
  if (priv->find_selected && priv->found_selected_iter)
  {
    /* found selected so set new selected event */
    GtkTreeIter *iter = priv->found_selected_iter;
    priv->find_selected = FALSE;
    priv->found_selected_iter = NULL;
    replay_event_store_set_current(priv->event_store, iter);
    gtk_tree_iter_free(iter);
  }

  if (timeout)
  {
    g_timer_destroy(timer);
  }

  /* always queue a draw */
  gtk_widget_queue_draw(GTK_WIDGET(self));

  return (*intervals != NULL);
}

static gboolean idle_render_intervals(gpointer data)
{
  IdleRenderData *idle_data;
  ReplayEventGraph *self;
  gboolean ret = FALSE;

  idle_data = (IdleRenderData *)data;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(idle_data->self), FALSE);

  self = REPLAY_EVENT_GRAPH(idle_data->self);

  /* render for 20ms at most */
  ret = render_intervals_with_timeout(self, idle_data->cr, idle_data->layout,
                                      &(idle_data->intervals), 20);

  if (!ret)
  {
    stop_idle_render(self);
  }
  return ret;
}


static void _draw_event_graph(ReplayEventGraph *self,
                              cairo_t *cr,
                              PangoLayout *layout,
                              gboolean main_loop)
{
  ReplayEventGraphPrivate *priv;
  cairo_operator_t op;
  GList *intervals = NULL;

  g_return_if_fail(cr != NULL);
  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  priv = self->priv;

  g_assert(main_loop || priv->render_id == 0);

  /* save initial state */
  cairo_save(cr);

  /* clear surface */
  op = cairo_get_operator(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgb(cr, 1.0f, 1.0f, 1.0f);
  cairo_paint(cr);
  cairo_set_operator(cr, op);

  /* set defaults for all drawing */
  cairo_set_line_width(cr, 2.0);

  /* emit rendering - will emit FALSE for rendering when we finish
   * idle_render_intervals */
  g_signal_emit(self,
                signals[RENDERING],
                0,
                TRUE);

  /* if start and end are NULL we have nothing to draw */
  if (replay_event_graph_is_plottable(self))
  {
    gint64 start_timestamp, end_timestamp;
    ReplayEventIntervalType type;

    /* if trying to find selected event, only lookup time around selected event
     * */
    if (priv->find_selected)
    {
      end_timestamp = start_timestamp = (replay_event_store_get_start_timestamp(priv->event_store) +
                                 ((priv->hadj_value + priv->selected_x)
                                  / priv->x_resolution));
    }
    else
    {
      start_timestamp = priv->render_start_timestamp;
      end_timestamp  = priv->render_end_timestamp;
    }

    /* draw intervals in 3 passes to layer info correctly */
    for (type = REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS;
         type < REPLAY_EVENT_INTERVAL_NUM_TYPES;
         type++)
    {
      intervals = g_list_concat(intervals,
                                replay_interval_list_lookup(priv->interval_list,
                                                         type,
                                                         start_timestamp,
                                                         end_timestamp,
                                                         NULL));
    }
  }
  /* do rendering in an idle callback if not requested to do main loop render */
  if (!main_loop)
  {
    IdleRenderData *idle_data;

    /* setup for idle render */
    idle_data = g_malloc0(sizeof(*idle_data));
    idle_data->self = self;
    idle_data->cr = cairo_reference(cr);
    idle_data->layout = g_object_ref(layout);
    idle_data->intervals = intervals;

    g_assert(priv->render_id == 0);
    priv->render_id = gdk_threads_add_idle_full(G_PRIORITY_LOW,
                                                idle_render_intervals,
                                                idle_data,
                                                (GDestroyNotify)free_idle_render_data);
  }
  else
  {
    render_intervals_with_timeout(self, cr, layout, &intervals, 0);
    g_assert(intervals == NULL);
    g_signal_emit(self, signals[RENDERING], 0, FALSE);
  }
  cairo_restore(cr);
}


/*
 * Render event graph to the provided cairo context - MUST BE CALLED FROM THE
 * MAINLOOP
 */
void replay_event_graph_draw(ReplayEventGraph *self, cairo_t *cr)
{
  PangoLayout *layout;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(self));

  /* render in main loop */
  layout = create_pango_layout(self, cr);

  _draw_event_graph(self, cr, layout, TRUE);
  g_object_unref(layout);

  draw_marks(self, cr);
}

/*!
 * draw handler for the event graph - blit backing surface onto
 * self and overlay with marks
 */
static gboolean replay_event_graph_blit(GtkWidget *widget,
                                     cairo_t *cr)
{
  gdouble yoffset;
  ReplayEventGraphPrivate *priv;
  ReplayEventGraph *self;
  cairo_operator_t op;

  g_return_val_if_fail(REPLAY_IS_EVENT_GRAPH(widget), FALSE);

  self = REPLAY_EVENT_GRAPH(widget);
  priv = self->priv;

  g_assert(priv->surface != NULL);

  /* double buffer all drawing operations to the window */
  cairo_push_group(cr);

  /* fill exposed region white */
  op = cairo_get_operator(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgb(cr, 1.0f, 1.0f, 1.0f);
  cairo_paint(cr);
  cairo_set_operator(cr, op);

  /* blit surface onto window */
  yoffset = gtk_adjustment_get_value(priv->vadj);
  cairo_surface_flush(priv->surface);
  cairo_set_source_surface(cr, priv->surface, 0.0, -yoffset);
  cairo_paint(cr);

  /* draw marks directly onto self */
  draw_marks(self, cr);

  /* paint double-buffered group to window */
  cairo_pop_group_to_source(cr);
  cairo_paint(cr);

  return FALSE;
}

static void hadj_value_changed(GtkAdjustment *hadj,
                               gpointer data)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;
  gdouble hadj_value;

  g_return_if_fail(REPLAY_IS_EVENT_GRAPH(data));

  self = REPLAY_EVENT_GRAPH(data);
  priv = self->priv;

  hadj_value = gtk_adjustment_get_value(priv->hadj);

  if (hadj_value != priv->hadj_value)
  {
    priv->hadj_value = hadj_value;

    replay_event_graph_redraw(self);
  }
}

/*!
 * other public functions
 */
GtkWidget *replay_event_graph_new(GtkAdjustment *hadj, GtkAdjustment *vadj)
{
  ReplayEventGraph *self;
  ReplayEventGraphPrivate *priv;

  self = g_object_new(REPLAY_TYPE_EVENT_GRAPH, NULL);
  priv = self->priv;

  priv->hadj = g_object_ref(hadj);
  priv->vadj = g_object_ref(vadj);

  /* listen for changes in value adjustments - since we only draw the currently
     visible region in time, when this changes, we need to redraw as the view
     will have scrolled */
  g_signal_connect(priv->hadj,
                   "value-changed",
                   G_CALLBACK(hadj_value_changed),
                   self);
  /* but since we draw the entire lot of nodes in the surface, we just need to
   * reblit the current viewable area if vadj changes */
  g_signal_connect_swapped(priv->vadj,
                           "value-changed",
                           G_CALLBACK(gtk_widget_queue_draw),
                           self);
  return GTK_WIDGET(self);
}
