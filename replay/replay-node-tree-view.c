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
 * GtkWidget to handle drawing of an ReplayNodeTree - inherits from
 * GtkDrawingArea
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <replay/replay-node-tree-view.h>
#include <replay/replay-config.h>

/* inherit from GtkDrawingArea */
G_DEFINE_TYPE(ReplayNodeTreeView, replay_node_tree_view, GTK_TYPE_DRAWING_AREA);

#define REPLAY_NODE_TREE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_NODE_TREE_VIEW, ReplayNodeTreeViewPrivate))

#define REPLAY_NODE_TREE_VIEW_DEFAULT_WIDTH 120

struct _ReplayNodeTreeViewPrivate
{
  /* keep a backing surface to render easily for printing */
  cairo_surface_t *surface;
  cairo_t *cr;
  PangoLayout *layout;

  GtkAdjustment *v_adj;

  /* how many pixels per node in list */
  gdouble y_resolution;

  /* ReplayNodeTree */
  GtkTreeModelFilter *f_node_tree; /* filtered tree - shows only visible
                                    * nodes */
  ReplayNodeTree *node_tree; /* the real node tree */

  gint ol_sig_id[3];
};

/* function definitions */
static void replay_node_tree_view_finalize(GObject *object);
static void replay_node_tree_view_get_preferred_width(GtkWidget *widget,
                                                   gint *minimal_width,
                                                   gint *natural_width);
static void replay_node_tree_view_get_preferred_height(GtkWidget *widget,
                                                   gint *minimal_height,
                                                   gint *natural_height);
static gboolean replay_node_tree_view_blit(GtkWidget *widget,
                                        cairo_t *cr);
static gboolean replay_node_tree_view_scroll(GtkWidget *widget,
                                          GdkEventScroll *event);
static gboolean replay_node_tree_view_configure(GtkWidget *widget,
                                             GdkEventConfigure *configure);
static gboolean replay_node_tree_view_map_event(GtkWidget *widget,
                                             GdkEventAny *event);

static gint replay_node_tree_view_n_nodes(const ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv;

  priv = self->priv;

  return (priv->f_node_tree ?
          gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->f_node_tree), NULL) :
          0);

}


static gboolean replay_node_tree_view_is_plottable(const ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv = self->priv;
  gboolean plottable = FALSE;

  if (priv->y_resolution)
  {
    plottable = replay_node_tree_view_n_nodes(self) > 0;
  }

  return plottable;
}

static gdouble replay_node_tree_view_height(const ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv = self->priv;
  gdouble height = gtk_widget_get_allocated_height(GTK_WIDGET(self));

  /* height is the number of nodes in the node list with each at
     y_resolution high */
  if (replay_node_tree_view_is_plottable(self))
  {
    height = MAX(height, replay_node_tree_view_n_nodes(self) * priv->y_resolution);
  }
  return height;
}

static gdouble replay_node_tree_view_width(const ReplayNodeTreeView *self)
{
  return gtk_widget_get_allocated_width(GTK_WIDGET(self));
}

gdouble replay_node_tree_view_drawable_height(const ReplayNodeTreeView *self)
{
  return replay_node_tree_view_height(self);
}

gdouble replay_node_tree_view_drawable_width(const ReplayNodeTreeView *self)
{
  return replay_node_tree_view_width(self);
}


static void replay_node_tree_view_create_new_surface(ReplayNodeTreeView *self)
{
  GtkWidget *widget;
  ReplayNodeTreeViewPrivate *priv;

  priv = self->priv;

  widget = GTK_WIDGET(self);

  if (gtk_widget_get_window(widget) != NULL)
  {
    gint width, height;

    /* create surface using allocated self size */
    if (priv->surface)
    {
      g_object_unref(priv->layout);
      cairo_destroy(priv->cr);
      cairo_surface_destroy(priv->surface);
      priv->surface = NULL;
    }
    width = replay_node_tree_view_drawable_width(self);
    height = replay_node_tree_view_drawable_height(self);
    priv->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               width, height);
    priv->cr = cairo_create(priv->surface);
    priv->layout = pango_cairo_create_layout(priv->cr);

    /* set font options as defaults for current screen */
    pango_cairo_context_set_font_options(pango_layout_get_context(priv->layout),
                                         gdk_screen_get_font_options(gdk_screen_get_default()));
    pango_cairo_context_set_resolution(pango_layout_get_context(priv->layout),
                                       gdk_screen_get_resolution(gdk_screen_get_default()));
  }
}

/*
 * Redraw the self - redraws to backing surface, and then queues a
 * redraw_event for the widget to make sure we get exposed - only bothers to
 * redraw though if the self is currently mapped
 */
static gboolean replay_node_tree_view_redraw(ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv;
  GtkWidget *widget;

  priv = self->priv;
  widget = GTK_WIDGET(self);

  if (priv->surface != NULL && gtk_widget_get_mapped(widget))
  {
    replay_node_tree_view_draw(self, priv->cr,
                            priv->layout);
    gtk_widget_queue_draw(widget);
  }
  return FALSE;
}


/*
 * Resets the v_adj for size of data and recreates surface and redraws to it then
 * exposes entire region - returns FALSE so can be used as an idle callback
 */
static gboolean replay_node_tree_modified(ReplayNodeTreeView *self)
{
  gdouble value, upper, page_size;
  ReplayNodeTreeViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(self), FALSE);

  priv = self->priv;

  /* reset upper limit of v_adj and make sure value doesn't exceed it */
  g_object_get(priv->v_adj,
               "value", &value,
               "upper", &upper,
               "page-size", &page_size,
               NULL);
  upper = MAX(0, (replay_node_tree_view_height(self) -
                  gtk_widget_get_allocated_height(GTK_WIDGET(self))));

  /* make sure we don't exceed upper limit */
  if (value > upper - page_size)
  {
    gtk_adjustment_set_value(priv->v_adj, upper - page_size);
  }

  /* set properties of v_adj */
  g_object_set(priv->v_adj,
               "upper", upper,
               NULL);

  /* redraw */
  replay_node_tree_view_create_new_surface(self);
  replay_node_tree_view_redraw(self);
  return FALSE;
}

static void replay_node_tree_row_inserted_cb(GtkTreeModel *model,
                                          GtkTreePath *path,
                                          GtkTreeIter *iter,
                                          ReplayNodeTreeView *self)
{
  replay_node_tree_modified(self);
}

static void replay_node_tree_row_deleted_cb(GtkTreeModel *model,
                                         GtkTreePath *path,
                                         ReplayNodeTreeView *self)
{
  /* row-deleted gets emitted BEFORE the actual row is removed, so if we simply
   * go and do a redraw now, we will end up drawing the deleted row - so instead
   * schedule a redraw to run as idle callback from the mainloop */
  gdk_threads_add_idle((GSourceFunc)replay_node_tree_modified, self);
}


/* node_tree related stuff */
void replay_node_tree_view_unset_data(ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_NODE_TREE_VIEW(self));

  priv = self->priv;

  if (priv->node_tree)
  {
    g_assert(priv->f_node_tree);
    g_signal_handler_disconnect(priv->f_node_tree,
                                priv->ol_sig_id[0]);
    g_signal_handler_disconnect(priv->f_node_tree,
                                priv->ol_sig_id[1]);
    g_signal_handler_disconnect(priv->f_node_tree,
                                priv->ol_sig_id[2]);
    g_object_unref(priv->f_node_tree);
    priv->f_node_tree = NULL;
    priv->node_tree = NULL;
  }
  /* now redraw */
  replay_node_tree_view_redraw(self);
}
void replay_node_tree_view_set_data(ReplayNodeTreeView *self,
                                 GtkTreeModelFilter *f_node_tree)
{
  ReplayNodeTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_NODE_TREE_VIEW(self));

  priv = self->priv;

  replay_node_tree_view_unset_data(self);

  priv->f_node_tree = g_object_ref(f_node_tree);
  priv->node_tree = REPLAY_NODE_TREE(gtk_tree_model_filter_get_model(f_node_tree));

  /* listen for updates */
  priv->ol_sig_id[0] = g_signal_connect(priv->f_node_tree,
                                        "row-inserted",
                                        G_CALLBACK(replay_node_tree_row_inserted_cb),
                                        self);
  priv->ol_sig_id[1] = g_signal_connect_swapped(priv->f_node_tree,
                                                "row-changed",
                                                G_CALLBACK(replay_node_tree_view_redraw),
                                                self);

  priv->ol_sig_id[2] = g_signal_connect(priv->f_node_tree,
                                        "row-deleted",
                                        G_CALLBACK(replay_node_tree_row_deleted_cb),
                                        self);
  /* now redraw */
  replay_node_tree_view_create_new_surface(self);
  replay_node_tree_view_redraw(self);
}

/* y-resolution related stuff */
gboolean replay_node_tree_view_set_y_resolution(ReplayNodeTreeView *self, gdouble y_resolution)
{
  ReplayNodeTreeViewPrivate *priv;
  gboolean changed = FALSE;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(self), FALSE);

  priv = self->priv;
  if (replay_node_tree_view_is_plottable(self))
  {
    changed = y_resolution != priv->y_resolution;

    if (changed)
    {
      priv->y_resolution = y_resolution;

      replay_node_tree_view_redraw(self);
    }
  }
  return changed;
}

gdouble replay_node_tree_view_get_y_resolution(const ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv;
  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(self), -1.0);

  priv = self->priv;
  return priv->y_resolution;
}


/*!
 * GObject related stuff
 */
static void replay_node_tree_view_class_init(ReplayNodeTreeViewClass *klass)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  obj_class->finalize = replay_node_tree_view_finalize;

  /* override gtk widget handler for draw and scroll and size
     allocate etc */
  widget_class->draw = replay_node_tree_view_blit;
  widget_class->scroll_event = replay_node_tree_view_scroll;
  widget_class->configure_event = replay_node_tree_view_configure;
  widget_class->map_event = replay_node_tree_view_map_event;
  widget_class->get_preferred_width = replay_node_tree_view_get_preferred_width;
  widget_class->get_preferred_height = replay_node_tree_view_get_preferred_height;

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayNodeTreeViewPrivate));

}

static void replay_node_tree_view_init(ReplayNodeTreeView *self)
{
  ReplayNodeTreeViewPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail(REPLAY_IS_NODE_TREE_VIEW(self));

  self->priv = priv = REPLAY_NODE_TREE_VIEW_GET_PRIVATE(self);
  widget = GTK_WIDGET(self);

  priv->y_resolution = REPLAY_DEFAULT_Y_RESOLUTION;

  /* set the events we care about */
  gtk_widget_add_events(widget,
                        GDK_SCROLL_MASK |
                        GDK_STRUCTURE_MASK);

  /* since we have a backing surface we essentially do double-buffering so can
   * disable gtk's own double buffering */
  gtk_widget_set_app_paintable(widget, TRUE);
  gtk_widget_set_double_buffered(widget, FALSE);
}

static void replay_node_tree_view_get_preferred_width(GtkWidget *widget,
                                                   gint *minimal_width,
                                                   gint *natural_width) {
  /* let parent size_request first then we just do max of both */
  if (GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->get_preferred_width)
  {
    GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->get_preferred_width(widget,
                                                                    minimal_width,
                                                                    natural_width);
  }
  *minimal_width = MAX(*minimal_width, REPLAY_NODE_TREE_VIEW_DEFAULT_WIDTH);
  *natural_width = MAX(*natural_width, REPLAY_NODE_TREE_VIEW_DEFAULT_WIDTH);
}

static void replay_node_tree_view_get_preferred_height(GtkWidget *widget,
                                                   gint *minimal_height,
                                                   gint *natural_height) {
  if (GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->get_preferred_height)
  {
    GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->get_preferred_height(widget,
                                                                    minimal_height,
                                                                    natural_height);
  }
}

/* Free any remaining allocated memory etc */
static void replay_node_tree_view_finalize(GObject *object)
{
  ReplayNodeTreeView *self;
  ReplayNodeTreeViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_NODE_TREE_VIEW(object));

  self = REPLAY_NODE_TREE_VIEW(object);
  priv = self->priv;

  if (priv->v_adj)
  {
    g_object_unref(priv->v_adj);
  }

  if (priv->surface)
  {
    g_object_unref(priv->layout);
    cairo_destroy(priv->cr);
    cairo_surface_destroy(priv->surface);
  }

  /* call parent finalize */
  G_OBJECT_CLASS(replay_node_tree_view_parent_class)->finalize(object);
}

static gboolean replay_node_tree_view_scroll(GtkWidget *widget,
                                          GdkEventScroll *event)
{
  ReplayNodeTreeView *self;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(widget), FALSE);

  self = REPLAY_NODE_TREE_VIEW(widget);

  if (event->direction == GDK_SCROLL_UP ||
      event->direction == GDK_SCROLL_DOWN)
  {
      ReplayNodeTreeViewPrivate *priv = self->priv;
      gdouble value, lower, upper, page_size;

      g_object_get(priv->v_adj,
                   "value", &value,
                   "lower", &lower,
                   "upper", &upper,
                   "page-size", &page_size,
                   NULL);

      /* scroll by 1 node height */
      value += ((event->direction == GDK_SCROLL_DOWN ? +1 : -1) *
                priv->y_resolution);

      /* make sure we don't scroll past the boundaries */
      value = MAX(MIN(value, upper - page_size), lower);
      gtk_adjustment_set_value(priv->v_adj, value);
      handled = TRUE;
  }
  return handled;
}

static gboolean replay_node_tree_view_configure(GtkWidget *widget,
                                             GdkEventConfigure *configure)
{
  ReplayNodeTreeView *self;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(widget), FALSE);
  self = REPLAY_NODE_TREE_VIEW(widget);

  /* create a new backing surface */
  replay_node_tree_view_create_new_surface(self);

  replay_node_tree_view_redraw(self);

  /* hand to parent configure */
  if (GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->configure_event)
  {
    GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->configure_event(widget,
                                                                       configure);
  }

  return TRUE;
}

static gboolean replay_node_tree_view_map_event(GtkWidget *widget,
                                             GdkEventAny *event)
{
  ReplayNodeTreeView *self;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(widget), FALSE);
  self = REPLAY_NODE_TREE_VIEW(widget);

  /* hand to parent map_event */
  if (GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->map_event)
  {
    GTK_WIDGET_CLASS(replay_node_tree_view_parent_class)->map_event(widget,
                                                                 event);
  }

  replay_node_tree_view_redraw(self);

  return TRUE;
}

static void replay_node_tree_draw_row(const ReplayNodeTreeView *self,
                                   cairo_t *cr,
                                   PangoLayout *layout,
                                   GtkTreeIter *iter,
                                   int i)
{
  ReplayNodeTreeViewPrivate *priv;
  gdouble x = 0, y = 0, dx = 0, dy = 0;
  gchar *label = NULL;
  int text_width = 0, text_height = 0;
  GtkWidget *widget;
  GtkStyleContext *style;
  GdkRGBA text_color;

  widget = GTK_WIDGET(self);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_color(style,
                              gtk_widget_get_state_flags(widget),
                              &text_color);
  priv = self->priv;

  /* easiest to use gtk_tree_model_get rather than
     replay_node_tree_get_node_*_event since that needs node ID, but we have an
     iter right here which references the correct row so avoids a lookup */
  gtk_tree_model_get(GTK_TREE_MODEL(priv->f_node_tree),
                     iter,
                     REPLAY_NODE_TREE_COL_LABEL, &label,
                     -1);
  if (label)
  {
    /* draw rows with children as bold */
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->f_node_tree),
                                       iter) > 0)
    {
      gchar *old_label = label;
      label = g_strdup_printf("<span weight='bold'>%s</span>", old_label);
      g_free(old_label);
    }
  }

  /* draw tick with labels save default state */
  cairo_save(cr);

  /* scale by y resolution */
  cairo_scale(cr, 1, priv->y_resolution);

  /* go to right hand side at index of node */
  x = replay_node_tree_view_width(self);
  y = i + 0.5;
  cairo_move_to(cr, x, y);

  /* do tick - to left by 5% pixels of entire width limited to 10 pixels MAX */
  dx = -(MIN(10.0, replay_node_tree_view_width(self) * 0.05));
  cairo_device_to_user_distance(cr, &dx, &dy);
  x += dx;
  cairo_line_to(cr, x, y);

  /* restore to get normal line widths */
  cairo_restore(cr);
  cairo_stroke(cr);

  /* label tick */
  if (label)
  {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    pango_layout_set_markup(layout, label, -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    if (text_width > dx)
    {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
    }
    else
    {
      pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    }

    dx = 0;
    dy = text_height;

    cairo_save(cr);
    cairo_scale(cr, 1, priv->y_resolution);
    cairo_device_to_user_distance(cr, &dx, &dy);

    x = 0;
    y = y - (dy / 2); /* offset labels by half their height from the start of the
                         ticks */

    cairo_move_to(cr, x, y);

    cairo_restore(cr);
    pango_cairo_show_layout(cr, layout);

    cairo_set_source_rgb(cr,
                         text_color.red,
                         text_color.green,
                         text_color.blue);
    cairo_stroke(cr);

    g_free(label);
  }
}

void replay_node_tree_view_draw(const ReplayNodeTreeView *self,
                             cairo_t *cr,
                             PangoLayout *layout)
{
  const ReplayNodeTreeViewPrivate *priv;
  GtkWidget *widget;
  GtkStyleContext *style;
  GdkRGBA text_color;
  cairo_operator_t op;

  priv = self->priv;
  widget = GTK_WIDGET(self);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_color(style,
                              gtk_widget_get_state_flags(widget),
                              &text_color);

  /* save original state */
  cairo_save(cr);

  /* clear new surface */
  op = cairo_get_operator(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgb(cr, 1.0f, 1.0f, 1.0f);
  cairo_paint(cr);
  cairo_set_operator(cr, op);

  /* draw gtk box */
  gtk_render_frame(style,
                   cr,
                   0, 0,
                   replay_node_tree_view_drawable_width(self),
                   replay_node_tree_view_drawable_height(self));

  /* set defaults for all drawing */
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgb(cr,
                       text_color.red,
                       text_color.green,
                       text_color.blue);

  if (replay_node_tree_view_is_plottable(self))
  {
    const PangoFontDescription *desc;
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean keep_going;

    desc = gtk_style_context_get_font(style,
                                      gtk_widget_get_state_flags(widget));
    pango_layout_set_font_description(layout, desc);

    /* set width of layout before we use any scalings */
    pango_layout_set_width(layout,
                           (((int)(MAX(0.9 * replay_node_tree_view_width(self),
                                       replay_node_tree_view_width(self) - 15.0))) *
                            PANGO_SCALE));

    /* only draw top level */
    for (keep_going = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->f_node_tree),
                                                    &iter);
         keep_going;
         keep_going = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->f_node_tree),
                                               &iter))
    {
      int i;
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->f_node_tree),
                                     &iter);
      i = gtk_tree_path_get_indices(path)[0];
      replay_node_tree_draw_row(self, cr, layout, &iter, i);
      gtk_tree_path_free(path);
    }
  }
  /* restore back to original state */
  cairo_restore(cr);
}

/*!
 * draw handler for the event graph - blit backing surface onto
 * self and overlay with marks
 */
static gboolean replay_node_tree_view_blit(GtkWidget *widget,
                                        cairo_t *cr)
{
  ReplayNodeTreeViewPrivate *priv;
  ReplayNodeTreeView *self;
  gdouble yoffset;
  cairo_operator_t op;

  g_return_val_if_fail(REPLAY_IS_NODE_TREE_VIEW(widget), FALSE);

  self = REPLAY_NODE_TREE_VIEW(widget);
  priv = self->priv;

  g_assert(priv->surface != NULL);

  /* clear entire window so we don't get scrolling artifacts */
  op = cairo_get_operator(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgb(cr, 1.0f, 1.0f, 1.0f);
  cairo_paint(cr);
  cairo_set_operator(cr, op);

  /* blit surface onto window */
  yoffset = gtk_adjustment_get_value(priv->v_adj);
  cairo_surface_flush(priv->surface);
  cairo_set_source_surface(cr,
                           priv->surface,
                           0.0, -yoffset);
  cairo_paint(cr);

  return FALSE;
}


/*!
 * other public functions
 */
GtkWidget *replay_node_tree_view_new(GtkAdjustment *v_adj)
{
  ReplayNodeTreeView *self;
  ReplayNodeTreeViewPrivate *priv;

  self = g_object_new(REPLAY_TYPE_NODE_TREE_VIEW, NULL);
  priv = self->priv;

  priv->v_adj = g_object_ref(v_adj);

  /* listen for changes in value of vertical adjustment - since we only blit the
     currently visible region, when this changes, we need to reblit as the view
     will have scrolled */
  g_signal_connect_swapped(priv->v_adj,
                           "value-changed",
                           G_CALLBACK(gtk_widget_queue_draw),
                           self);

  return GTK_WIDGET(self);
}
