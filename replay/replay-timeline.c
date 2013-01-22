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

/**
 * SECTION:replay-timeline
 * @short_description: Displays a simple timeline
 * @see_also: #ReplayTimelineView
 *
 * #ReplayTimeline is a simple widget to display a timeline
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <math.h>
#include <replay/replay-timeline.h>
#include <replay/replay-config.h>

/* inherit from GtkDrawingArea */
G_DEFINE_TYPE(ReplayTimeline, replay_timeline, GTK_TYPE_DRAWING_AREA);

#define REPLAY_TIMELINE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_TIMELINE, ReplayTimelinePrivate))

#define REPLAY_TIMELINE_DEFAULT_HEIGHT 75

#define DEVICE_MINIMUM_MINOR_TICKS_PIXELS 50.0

struct _ReplayTimelinePrivate
{
        /* keep a backing surface to render easily for printing */
        cairo_surface_t *surface;
        cairo_t *cr;
        PangoLayout *layout;

        GtkAdjustment *h_adj;

        gint64 start;
        gint64 end;
        gint64 diff;

        /* whether to show times in absolute or relative values */
        gboolean absolute_time;

        /* how many pixels per microsecond */
        gdouble x_resolution;

};

/* function definitions */
static void replay_timeline_finalize(GObject *object);
static void replay_timeline_get_preferred_width(GtkWidget *widget,
                                                gint *minimal_width,
                                                gint *natural_width);
static void replay_timeline_get_preferred_height(GtkWidget *widget,
                                                 gint *minimal_height,
                                                 gint *natural_height);
static gboolean replay_timeline_blit(GtkWidget *widget,
                                     cairo_t *cr);
static gboolean replay_timeline_scroll(GtkWidget *widget,
                                       GdkEventScroll *event);
static gboolean replay_timeline_configure_event(GtkWidget *widget,
                                                GdkEventConfigure *event);
static gboolean replay_timeline_map_event(GtkWidget *widget,
                                          GdkEventAny *event);

static gboolean replay_timeline_is_plottable(const ReplayTimeline *self)
{
        ReplayTimelinePrivate *priv;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(self), FALSE);

        priv = self->priv;

        return (priv->diff > 0 && priv->x_resolution);
}

gdouble replay_timeline_drawable_height(const ReplayTimeline *self)
{
        return gtk_widget_get_allocated_height(GTK_WIDGET(self));
}

gdouble replay_timeline_drawable_width(const ReplayTimeline *self)
{
        return gtk_widget_get_allocated_width(GTK_WIDGET(self));
}

/* limit tick length to 20 pixels max */
#define DEVICE_MAJOR_TICK_WITH_TEXT_LENGTH(self) (MIN(replay_timeline_drawable_height(self) / 3.0, 20.0))
#define DEVICE_TEXT_OFFSET_FROM_MAJOR_TICK(self) (MIN(replay_timeline_drawable_height(self) / 12.0, 5.0))
#define DEVICE_MAJOR_TICK_NO_TEXT_LENGTH(self) (DEVICE_MAJOR_TICK_WITH_TEXT_LENGTH(self) * 0.75)
#define DEVICE_MINOR_TICK_LENGTH(self) (DEVICE_MAJOR_TICK_WITH_TEXT_LENGTH(self) / 2.0)


static void replay_timeline_create_new_surface(ReplayTimeline *self)
{
        ReplayTimelinePrivate *priv;
        cairo_operator_t op;

        priv = self->priv;

        if (!gtk_widget_get_window(GTK_WIDGET(self))) {
                return;
        }

        /* create surface using allocated self size */
        if (priv->surface)
        {
                g_object_unref(priv->layout);
                cairo_destroy(priv->cr);
                cairo_surface_destroy(priv->surface);
                priv->surface = NULL;
        }
        priv->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                   replay_timeline_drawable_width(self),
                                                   replay_timeline_drawable_height(self));

        priv->cr = cairo_create(priv->surface);
        priv->layout = pango_cairo_create_layout(priv->cr);

        /* set font options as defaults for current screen */
        pango_cairo_context_set_font_options(pango_layout_get_context(priv->layout),
                                             gdk_screen_get_font_options(gdk_screen_get_default()));
        pango_cairo_context_set_resolution(pango_layout_get_context(priv->layout),
                                           gdk_screen_get_resolution(gdk_screen_get_default()));

        /* clear new surface */
        op = cairo_get_operator(priv->cr);
        cairo_set_operator(priv->cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgb(priv->cr, 1.0f, 1.0f, 1.0f);
        cairo_paint(priv->cr);
        cairo_set_operator(priv->cr, op);
}

/*
 * Redraw the self - redraws to backing surface, and then queues a
 * redraw_event for the widget to make sure we get exposed - only bothers to
 * redraw though if the self is currently mapped
 */
static gboolean replay_timeline_redraw(ReplayTimeline *self)
{
        ReplayTimelinePrivate *priv;
        GtkWidget *widget;

        priv = self->priv;
        widget = GTK_WIDGET(self);

        if (priv->surface != NULL && gtk_widget_get_mapped(widget)) {
                replay_timeline_draw(self, priv->cr, priv->layout);
                gtk_widget_queue_draw(widget);
        }
        return FALSE;
}

/* x-resolution related stuff */
gboolean replay_timeline_set_x_resolution(ReplayTimeline *self, gdouble x_resolution)
{
        ReplayTimelinePrivate *priv;
        gboolean changed = FALSE;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(self), FALSE);

        priv = self->priv;

        /* only set x resolution if we actually have some data to plot - ie
           the concept of x_resolution is only valid when start and end are
           set and are not the same */
        if (replay_timeline_is_plottable(self)) {
                changed = (priv->x_resolution != x_resolution);
                if (changed) {
                        priv->x_resolution = x_resolution;

                        /* redraw backing surface */
                        replay_timeline_redraw(self);
                }
        }
        return changed;
}

void replay_timeline_set_absolute_time(ReplayTimeline *self, gboolean absolute_time)
{
        ReplayTimelinePrivate *priv;

        g_return_if_fail(REPLAY_IS_TIMELINE(self));

        priv = self->priv;
        if (priv->absolute_time != absolute_time) {
                priv->absolute_time = absolute_time;

                /* causes a full redraw */
                replay_timeline_redraw(self);
        }
}

void replay_timeline_set_bounds(ReplayTimeline *self,
                                gint64 start,
                                gint64 end)
{
        ReplayTimelinePrivate *priv;
        gboolean changed = FALSE;

        g_return_if_fail(REPLAY_IS_TIMELINE(self));
        g_return_if_fail(start <= end);

        priv = self->priv;

        if (priv->start != start) {
                priv->start = start;
                changed = TRUE;
        }

        if (priv->end != end) {
                priv->end = end;
                changed = TRUE;
        }

        if (changed) {
                priv->diff = priv->end - priv->start;
                replay_timeline_redraw(self);
        }
}

/*!
 * GObject related stuff
 */
static void replay_timeline_class_init(ReplayTimelineClass *klass)
{
        GObjectClass *obj_class;
        GtkWidgetClass *widget_class;

        obj_class = G_OBJECT_CLASS(klass);
        widget_class = GTK_WIDGET_CLASS(klass);

        obj_class->finalize = replay_timeline_finalize;

        /* override gtk widget handler for draw and scroll and size allocate
         * etc */
        widget_class->map_event = replay_timeline_map_event;
        widget_class->configure_event = replay_timeline_configure_event;
        widget_class->draw = replay_timeline_blit;
        widget_class->scroll_event = replay_timeline_scroll;
        widget_class->get_preferred_width = replay_timeline_get_preferred_width;
        widget_class->get_preferred_height = replay_timeline_get_preferred_height;

        /* add private data */
        g_type_class_add_private(obj_class, sizeof(ReplayTimelinePrivate));

}

static void replay_timeline_init(ReplayTimeline *self)
{
        ReplayTimelinePrivate *priv;
        GtkWidget *widget;

        g_return_if_fail(REPLAY_IS_TIMELINE(self));

        self->priv = priv = REPLAY_TIMELINE_GET_PRIVATE(self);
        widget = GTK_WIDGET(self);

        priv->x_resolution = REPLAY_DEFAULT_X_RESOLUTION;
        priv->absolute_time = REPLAY_DEFAULT_ABSOLUTE_TIME;

        priv->start = 0;
        priv->end = 0;

        /* set the events we care about */
        gtk_widget_add_events(widget,
                              GDK_SCROLL_MASK |
                              GDK_STRUCTURE_MASK);

        /* since we have a backing surface we essentially do double-buffering so can
         * disable gtk's own double buffering */
        gtk_widget_set_app_paintable(widget, TRUE);
        gtk_widget_set_double_buffered(widget, FALSE);
}

static void replay_timeline_get_preferred_width(GtkWidget *widget,
                                                gint *minimal_width,
                                                gint *natural_width) {
        /* let parent size_request first then we just do max of both */
        if (GTK_WIDGET_CLASS(replay_timeline_parent_class)->get_preferred_width) {
                GTK_WIDGET_CLASS(replay_timeline_parent_class)
                        ->get_preferred_width(widget,
                                              minimal_width,
                                              natural_width);
        }
}

static void replay_timeline_get_preferred_height(GtkWidget *widget,
                                                 gint *minimal_height,
                                                 gint *natural_height) {
        if (GTK_WIDGET_CLASS(replay_timeline_parent_class)->get_preferred_height) {
                GTK_WIDGET_CLASS(replay_timeline_parent_class)
                        ->get_preferred_height(widget,
                                               minimal_height,
                                               natural_height);
        }
        *minimal_height = MAX(*minimal_height, REPLAY_TIMELINE_DEFAULT_HEIGHT);
        *natural_height = MAX(*natural_height, REPLAY_TIMELINE_DEFAULT_HEIGHT);
}

/* Free any remaining allocated memory etc */
static void replay_timeline_finalize(GObject *object)
{
        ReplayTimeline *self;
        ReplayTimelinePrivate *priv;

        g_return_if_fail(REPLAY_IS_TIMELINE(object));

        self = REPLAY_TIMELINE(object);
        priv = self->priv;

        g_clear_object(&priv->h_adj);

        if (priv->surface) {
                g_object_unref(priv->layout);
                cairo_destroy(priv->cr);
                cairo_surface_destroy(priv->surface);
        }

        /* call parent finalize */
        G_OBJECT_CLASS(replay_timeline_parent_class)->finalize(object);
}

static gboolean replay_timeline_scroll(GtkWidget *widget,
                                       GdkEventScroll *event)
{
        ReplayTimelinePrivate *priv;
        GdkScrollDirection direction;
        gdouble value, lower, upper, page_size;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(widget), FALSE);

        priv = REPLAY_TIMELINE(widget)->priv;

        /* if holding down shift and scroll up, scroll to right */
        if (event->direction == GDK_SCROLL_UP && event->state & GDK_SHIFT_MASK) {
                direction = GDK_SCROLL_LEFT;
        } else if (event->direction == GDK_SCROLL_DOWN && event->state & GDK_SHIFT_MASK) {
                direction = GDK_SCROLL_RIGHT;
        } else if (event->direction == GDK_SCROLL_LEFT ||
                   event->direction == GDK_SCROLL_RIGHT) {
                direction = event->direction;
        } else {
                return FALSE;
        }
        g_object_get(priv->h_adj,
                     "value", &value,
                     "lower", &lower,
                     "upper", &upper,
                     "page-size", &page_size,
                     NULL);

        /* scroll 100 pixels */
        value += (direction == GDK_SCROLL_RIGHT ? +1 : -1)*100;

        /* make sure we don't scroll past the boundaries */
        value = MAX(MIN(value, upper - page_size), lower);
        gtk_adjustment_set_value(priv->h_adj, value);
        return TRUE;
}

static gboolean replay_timeline_configure_event(GtkWidget *widget,
                                                GdkEventConfigure *configure)
{
        ReplayTimeline *self;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(widget), FALSE);
        /* hand to parent configure if it exists */
        if (GTK_WIDGET_CLASS(replay_timeline_parent_class)->configure_event) {
                GTK_WIDGET_CLASS(replay_timeline_parent_class)->configure_event
                        (widget, configure);
        }

        self = REPLAY_TIMELINE(widget);

        /* create a new backing surface */
        replay_timeline_create_new_surface(self);

        replay_timeline_redraw(self);

        return TRUE;
}

static gboolean replay_timeline_map_event(GtkWidget *widget,
                                          GdkEventAny *event)
{
        ReplayTimeline *self;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(widget), FALSE);
        self = REPLAY_TIMELINE(widget);

        /* hand to parent map_event if it exists*/
        if (GTK_WIDGET_CLASS(replay_timeline_parent_class)->map_event) {
                GTK_WIDGET_CLASS(replay_timeline_parent_class)->map_event
                        (widget, event);
        }

        replay_timeline_redraw(self);

        return TRUE;
}

static const guint time_steps[REPLAY_NUM_TIME_UNITS - 1] = {
        1000, /* 1000us in 1ms */
        1000, /* 1000ms in 1s */
        60, /* 60s in 1m */
        60, /* 60m in 1h */
        24 /* 24h in 1d */
};

static const gint64 usec_multipliers[REPLAY_NUM_TIME_UNITS] = {
        (gint64)1,
        (gint64)1000,
        (gint64)1000 * 1000,
        (gint64)60 * 1000 * 1000,
        (gint64)60 * 60 * 1000 * 1000,
        (gint64)24 * 60 * 60 * 1000 * 1000
};

static gint64 roundup(gint64 value,
                      gint64 step)
{
        return (value - ((value + step) % step) + step);
}

static void
draw_time_label(const ReplayTimeline *self,
                cairo_t *cr,
                PangoLayout *layout,
                gdouble x,
                gint64 tick)
{
        ReplayTimelinePrivate *priv = self->priv;
        PangoRectangle extents;
        gchar *label;
        gdouble y;

        label = replay_timestamp_to_string(tick, priv->absolute_time);
        pango_layout_set_text(layout, label, -1);
        pango_layout_get_pixel_extents(layout, NULL, &extents);
        x += extents.x - (extents.width / 2.0);
        y = (DEVICE_MAJOR_TICK_NO_TEXT_LENGTH(self) +
             DEVICE_TEXT_OFFSET_FROM_MAJOR_TICK(self) +
             extents.y);

        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, layout);
        g_free(label);
}

static const gint b1000_divisors[] = {500, 200, 100, 50, 20, 10, 5, 2, 1, 0};
static const gint b60_divisors[] = {12, 6, 4, 3, 2, 1, 0};

static void
draw_ticks(const ReplayTimeline *self,
           cairo_t *cr,
           PangoLayout *layout)
{
        ReplayTimelinePrivate *priv;
        gdouble start, end;
        gint64 target, minor, major, tick;
        gint i;

        priv = self->priv;

        /* we draw the first tick at the start time (rounded up to the next
         * scale amount) and then the rest of the ticks at multiplies[scale]
         * increments */
        start = ((gdouble)(priv->absolute_time ? priv->start : 0) +
                  (gtk_adjustment_get_value(priv->h_adj) / priv->x_resolution));
        end = start + (gdouble)gtk_widget_get_allocated_width(GTK_WIDGET(self)) /
                priv->x_resolution;

        /* ideally the unlabelled ticks should be
         * DEVICE_MINIMUM_MINOR_TICKS_PIXELS apart so the time division we
         * need to use then is DEVICE_MINIMUM_MINOR_TICKS_PIXELS /
         * x_resolution - then find the time which is closest (but potentially
         * smaller) than this */
        target = floor(DEVICE_MINIMUM_MINOR_TICKS_PIXELS / priv->x_resolution);
        minor = 1;
        major = minor;
        for (i = REPLAY_TIME_UNIT_MICROSECONDS;
             i < REPLAY_NUM_TIME_UNITS && target > major;
             i++) {
                int j;
                const gint *divisions = (i <= REPLAY_TIME_UNIT_SECONDS ?
                                         b1000_divisors :
                                         b60_divisors);
                for (j = 0; divisions[j] != 0 && target > major; j++) {
                        minor = major;
                        major = usec_multipliers[i] / divisions[j];
                }
        }

        for (tick = roundup((gint64)floor(start), minor);
             tick <= end;
             tick += minor)
        {
                gdouble x = (tick - start) * priv->x_resolution;
                gboolean ismajor = !(tick % major);

                cairo_move_to(cr, x, 0.0);
                cairo_line_to(cr, x,
                              (ismajor ?
                               DEVICE_MAJOR_TICK_NO_TEXT_LENGTH(self) :
                               DEVICE_MINOR_TICK_LENGTH(self)));
                if (ismajor) {
                        draw_time_label(self, cr, layout, x, tick);
                }
        }
        cairo_stroke(cr);
}

void replay_timeline_draw(const ReplayTimeline *self,
                          cairo_t *cr,
                          PangoLayout *layout)
{
        const ReplayTimelinePrivate *priv;
        cairo_operator_t op;
        GtkWidget *widget;
        GtkStyleContext *style;
        GdkRGBA text_color;

        g_return_if_fail(REPLAY_IS_TIMELINE(self));

        priv = self->priv;
        widget = GTK_WIDGET(self);
        style = gtk_widget_get_style_context(widget);
        gtk_style_context_get_color(style,
                                    gtk_widget_get_state_flags(widget),
                                    &text_color);

        /* save initial state */
        cairo_save(cr);

        /* clear to white */
        op = cairo_get_operator(priv->cr);
        cairo_set_operator(priv->cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgb(priv->cr, 1.0f, 1.0f, 1.0f);
        cairo_paint(priv->cr);
        cairo_set_operator(priv->cr, op);

        /* draw gtk box */
        gtk_render_frame(style,
                         cr,
                         0, 0,
                         replay_timeline_drawable_width(self),
                         replay_timeline_drawable_height(self));

        /* set defaults for all drawing */
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr,
                             text_color.red,
                             text_color.green,
                             text_color.blue);

        if (replay_timeline_is_plottable(self)) {
                /* setup fonts */
                const PangoFontDescription *desc;
                GtkStateFlags state;

                state = gtk_widget_get_state_flags(widget);
                desc = gtk_style_context_get_font(style, state);
                pango_layout_set_font_description(layout, desc);

                /* draw ticks */
                draw_ticks(self, cr, layout);
        }
        /* restore to original state */
        cairo_restore(cr);
}

/*!
 * draw handler for the event graph - blit backing surface onto
 * self and overlay with marks
 */
static gboolean replay_timeline_blit(GtkWidget *widget,
                                     cairo_t *cr)
{
        ReplayTimelinePrivate *priv;

        g_return_val_if_fail(REPLAY_IS_TIMELINE(widget), FALSE);

        priv = REPLAY_TIMELINE(widget)->priv;

        g_assert(priv->surface != NULL);

        cairo_surface_flush(priv->surface);
        cairo_set_source_surface(cr, priv->surface, 0.0, 0.0);
        cairo_paint(cr);

        return FALSE;
}


/*!
 * other public functions
 */
GtkWidget *replay_timeline_new(GtkAdjustment *h_adj)
{
        ReplayTimeline *self;
        ReplayTimelinePrivate *priv;

        self = g_object_new(REPLAY_TYPE_TIMELINE, NULL);
        priv = self->priv;

        priv->h_adj = g_object_ref(h_adj);

        /* listen for changes in value of horizontal adjustment - since we
           only draw the currently visible region, when this changes, we need
           to redraw as the view will have scrolled */
        g_signal_connect_swapped(priv->h_adj,
                                 "value-changed",
                                 G_CALLBACK(replay_timeline_redraw),
                                 self);
        return GTK_WIDGET(self);
}
