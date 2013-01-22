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

#ifndef __REPLAY_TIMELINE_H__
#define __REPLAY_TIMELINE_H__

#include <gtk/gtk.h>
#include <replay/replay-timestamp.h>

G_BEGIN_DECLS

/* all the usual boiler-plate stuff for doing a GObject */

#define REPLAY_TYPE_TIMELINE \
  (replay_timeline_get_type())
#define REPLAY_TIMELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), REPLAY_TYPE_TIMELINE, ReplayTimeline))
#define REPLAY_TIMELINE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), TIMELINE, ReplayTimelineClass))
#define REPLAY_IS_TIMELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), REPLAY_TYPE_TIMELINE))
#define REPLAY_IS_TIMELINE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), REPLAY_TYPE_TIMELINE))
#define REPLAY_TIMELINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), REPLAY_TYPE_TIMELINE, ReplayTimelineClass))

typedef struct _ReplayTimelinePrivate ReplayTimelinePrivate;
typedef struct _ReplayTimeline {
  GtkDrawingArea parent;

  /*< private >*/
  ReplayTimelinePrivate *priv;
} ReplayTimeline;

typedef struct _ReplayTimelineClass {
  /*< private >*/
  GtkDrawingAreaClass parent_class;

} ReplayTimelineClass;

/* public functions */
GType replay_timeline_get_type(void);
GtkWidget *replay_timeline_new(GtkAdjustment *hadj);
gboolean replay_timeline_set_x_resolution(ReplayTimeline *timeline, gdouble x_resolution);
void replay_timeline_set_absolute_time(ReplayTimeline *timeline, gboolean absolute_time);
void replay_timeline_set_bounds(ReplayTimeline *timeline,
                             gint64 start, gint64 end);
gdouble replay_timeline_drawable_width(const ReplayTimeline *timeline);
gdouble replay_timeline_drawable_height(const ReplayTimeline *timeline);
void replay_timeline_draw(const ReplayTimeline *timeline, cairo_t *cr,
                       PangoLayout *layout);

G_END_DECLS

#endif /* __REPLAY_TIMELINE_H__ */
