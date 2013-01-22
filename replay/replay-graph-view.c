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
 * SECTION:replay-graph-view
 * @brief: OpenGL based 3D view of node / edge graph
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkgl.h>
#include <gtk/gtkgl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pangoft2.h>
#include <math.h>
#include <replay/replay.h>
#include <replay/replay-events.h>
#include <replay/replay-graph-view.h>
#include <replay/replay-string-utils.h>
#include <replay/replay-event-string.h>

/* Inherit from GtkDrawingArea */
G_DEFINE_TYPE(ReplayGraphView, replay_graph_view, GTK_TYPE_DRAWING_AREA);

#define REPLAY_GRAPH_VIEW_GSETTINGS_PATH REPLAY_GSETTINGS_PATH "/graph-view"
#define ATTRACT_CONSTANT_KEY "attract-constant"
#define REPEL_CONSTANT_KEY "repel-constant"
#define CENTRAL_PULL_CONSTANT_KEY "central-pull-constant"
#define FRICTION_CONSTANT_KEY "friction-constant"
#define SPRING_LENGTH_KEY "spring-length"

#define MONOSPACE_FONT_KEY "monospace-font-name"
#define DEFAULT_MONOSPACE_FONT ("Monospace 10")

#define DEFAULT_MAX_FPS 100.0
#define LAYOUT_INTERVAL_MS 20

#define DELTA_PAN 10.0f

#define NUM_SPHERE_SLICES 20
#define NUM_SPHERE_STACKS 20

static const GLfloat highlight_emission[4] = {0.4f, 0.4f, 0.4f, 1.0f};
static const GLfloat no_emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};

#define NODE_DISTANCE_TO_CAMERA_RATIO 10.0

#define NODE_RADIUS       1.0
#define SELF_EDGE_RADIUS    0.2
#define EDGE_WIDTH 0.040 * NODE_RADIUS

#define NODE_MASS 1.0
#define EDGE_MASS 1.0*NODE_MASS

#define CLAMP_0_360(x) do {                     \
  if (x >= 360.0)                             \
  {                                           \
    x -= 360.0;                               \
  }                                           \
  if (x < 0)                                  \
  {                                           \
    x += 360.0;                               \
  }                                           \
} while (0)

enum {
  PROP_SELECTED_NODE_IDS = 1,
  PROP_MAX_FPS = 2,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];
typedef struct _ReplayGraphViewNode ReplayGraphViewNode;

typedef struct _NodeActivity
{
  gchar *name;
  ReplayPropertyStore *props;
} NodeActivity;

static gboolean props_get_double(ReplayPropertyStore *props,
                                 const gchar *name,
                                 gdouble *value)
{
  GVariant *prop = NULL;
  gboolean ret = FALSE;

  prop = replay_property_store_lookup(props, name);
  if (prop)
  {
    *value = g_variant_get_double(prop);
    ret = TRUE;
    g_variant_unref(prop);
  }
  return ret;
}

static gboolean parse_color_to_flv(const gchar *string,
                                   GLfloat color[4])
{
  GdkRGBA rgba = {0};
  gboolean ret = FALSE;

  if (string != NULL)
  {
    ret = gdk_rgba_parse(&rgba, string);
  }

  color[0] = (GLfloat)rgba.red;
  color[1] = (GLfloat)rgba.green;
  color[2] = (GLfloat)rgba.blue;
  color[3] = (GLfloat)rgba.alpha;

  return ret;
}

static gboolean props_get_string(ReplayPropertyStore *props,
                                 const gchar *name,
                                 gchar **string)
{
  gboolean ret = FALSE;
  GVariant *prop = NULL;

  prop = replay_property_store_lookup(props, name);
  if (prop)
  {
    if (string != NULL)
    {
      *string = g_variant_dup_string(prop, NULL);
    }
    ret = TRUE;
    g_variant_unref(prop);
  }
  return ret;
}

static gboolean props_get_color(ReplayPropertyStore *props,
                                GLfloat color[4])
{
  gchar *spec = NULL;
  gboolean ret = FALSE;

  ret = props_get_string(props, "color", &spec);
  if (ret)
  {
    ret = parse_color_to_flv(spec, color);
    g_free(spec);
  }
  return ret;
}

static gdouble node_activity_get_level(NodeActivity *activity)
{
  gdouble level = 1.0;
  props_get_double(activity->props, "level", &level);
  return level;
}

static gboolean node_activity_get_color(NodeActivity *activity,
                                        GLfloat color[4])
{
  gboolean ret = FALSE;

  ret = props_get_color(activity->props, color);
  if (ret)
  {
    /* substitute alpha with level of activity */
    gdouble alpha = color[3];
    props_get_double(activity->props, "level", &alpha);
    color[3] = alpha;
  }
  return ret;
}

struct _ReplayGraphViewNode
{
  GtkTreeIter iter; /* iter to row in ReplayNodeTree for this node */
  guint index; /* index in priv->visible_nodes, or priv->hidden_nodes - also
                  used as gl_name for picking */
  /* is this a group node */
  gboolean group;

  /* name for this node */
  gchar *name;

  GLfloat color[4];

  gchar *label; /* an node gets labelled with its label or name */
  gchar *event_label;

  /* we have a GPtrArray which stores GPtrArray's of edges - each GPtrArray of
   * edges contains all the edges which are between 2 given nodes, and each of
   * the two node's share this GPtrArray of edges. */
  GPtrArray *edges;

  gboolean hidden;

  gdouble mass; /* mass of node */
  gdouble radius; /* radius node is drawn at - since volume of a sphere is
                   * 4/3*pi*r^3, we will make mass == volume, so radius is
                   * (3/(4*pi)*mass)^1/3 */
  GList *activities;
  float sx; /* x coord */
  float sy;
  float sz;

  float vx; /* x vel */
  float vy;
  float vz;

  float fx; /* x force */
  float fy;
  float fz;

  ReplayGraphViewNode *parent; /* when we group nodes together they have a
                             * single parent node */
  GList *children; /* on node can have multiple children IF IT IS A GROUP
                    * NODE */
  guint num_children;
};

typedef struct _ReplayGraphViewEdge
{
  gboolean directed;
  ReplayGraphViewNode *tail;
  ReplayGraphViewNode *head;

  gchar *name;
  gchar *tail_name;
  gchar *head_name;

  ReplayPropertyStore *props;

  guint index; /* index in edge array - also used as gl_name for picking */

  /* some extra info to show when hovering */
  gchar *hover_info;

  /* edges can have an event_label */
  gchar *event_label;

  /* position of event label along edge */
  gdouble position;
} ReplayGraphViewEdge;

static gdouble edge_get_weight(ReplayGraphViewEdge *edge)
{
  gdouble weight = 1.0;
  props_get_double(edge->props, "weight", &weight);
  return weight;
}

static gboolean edge_get_color(ReplayGraphViewEdge *edge,
                               GLfloat color[4])
{
  return props_get_color(edge->props, color);
}

struct _ReplayGraphViewPrivate
{
  gdouble max_fps;
  gboolean absolute_time;
  gboolean show_hud;
  gboolean two_dimensional;

  GLuint active_node_glow_texture;
  GLuint node_display_list;
  GLuint group_display_list;
  GLuint edge_body_display_list;
  GLuint edge_head_display_list;
  GLuint self_edge_display_list;

  GLuint gl_error;
  gboolean label_nodes;
  gboolean label_edges;
  ReplayEventStore *event_store;
  GtkTreeIter *iter;
  guint processing_id;
  GtkTreePath *processing_start;
  GtkTreePath *processing_end;
  guint event_inserted_id;
  guint current_changed_id;
  gboolean forward;
  ReplayNodeTree *node_tree;
  guint row_changed_id;
  guint row_inserted_id;
  guint render_timeout_id; /* we set a timeout to render scene for animation etc
                              - if this is zero, we are not animating currently
                              and would need to render manually for timing
                              fps */
  GTimer *fps_timer;
  gdouble fps;
  GdkGLConfig *gl_config;

  /* cairo context and surface to act as OpenGL texture source to render 2D HUD
   * using cairo */
  PangoLayout *layout;
  cairo_t *cr;
  unsigned char *surface_data;
  GLuint hud_texture;

  GThread *layout_thread;
  GMainLoop *layout_loop;
  GMainContext *layout_context;
  GSource *layout_source;
  gboolean reset_viewing_volume;

  /* protect our graph structures from access by the main thread (which both
   * reads the graph structure and updates it) and the layout thread (which
   * simply reads the graph structure but doesn't modify it) - since the main
   * thread is the only one to modify the graph, we can safely have the main
   * thread traverse the graph without the lock held, but the main thread MUST
   * take the lock when modifying the graph and the layout thread MUST take the
   * lock when traversing the graph */
  GMutex lock;

  /* we maintain a Hashtable of ReplayGraphViewNodes */
  GHashTable *node_table;
  GHashTable *edge_table;

  GList *dead_edges; /* edges which don't point to an node yet - if we
                      * are running in reverse, this is likely if edges aren't
                      * cleaned up - so when we add an node, run through this
                      * list and see if any entries point to the new node and
                      * then we can increase its mass etc */

  /* we also maintain two lists - one containing all the visible nodes, the
   * other containing the hidden ones - nodes are hidden when they are a
   * descendant of the hidden group */
  GPtrArray *visible_nodes;
  GPtrArray *hidden_nodes; /* when user hides an node, it gets
                            * added to the hidden list */
  GHashTable *selected_nodes; /* contains a subset of visible_nodes for the
                               * currently selected set of node - lookup
                               * via node ID of ReplayGraphViewNode */
  GHashTable *picked_nodes; /* nodes picked in the most recent pick
                             * operation - total of picked and selected
                             * nodes should be drawn highlighted */
  ReplayGraphViewNode *hovered_node;
  ReplayGraphViewEdge *hovered_edge;
  GList *active_nodes;

  /* opengl near and far clipping pane distances */
  float z_near;
  float z_far;
  /* furthest and closest distance of camera from origin */
  float max_camera_distance;
  float min_camera_distance;
  /* all nodes will be within a bounding box of priv->max_node_distance - make
   * this proportional to priv->max_camera_distance */
  float max_node_distance;

  /* field of view in y axis - make sure we fit in all nodes within a bounding
   * box of priv->max_node_distance at a total distance away of priv->max_camera_distance
   * from origin - see OpenGL Redbook Chapter 3 'Troubleshooting
   * Transformations' for more on this:
   *
   * Camera can be at most distance priv->max_camera_distance from origin, and can be
   * observing the sphere of nodes with radius of the bounding sphere of the
   * bounding box of priv->max_node_distance so make sure the field of view can fit
   * this all in - FOVY is then twice the arctangent of the ratio of these
   * distances as degrees
   *
   * bounding sphere has radius sqrt(3 * (priv->max_node_distance^2))
   *
   * ((2.0 * atan2f(MAX_RADIUS, sqrt(3 * (priv->max_node_distance^2)))) * 180.0 /
   * M_PI)
   *
   * keep a cached copy here so we don't have to keep calculating it
   */
  float fovy;

  /* focal point within the scene to 'center' at */
  const float *pIx;
  const float *pIy;
  const float *pIz;

  /* position to translate to when rendering scene */
  float x, y, z;

  /* we store the current rotation transformation matrix and modify it when we
   * do rotations */
  GLdouble rotation[16];
  GLdouble start_pos[3];

  /* some parameters for graph */
  gdouble central_pull_constant;
  gdouble attract_constant;
  gdouble repel_constant;
  gdouble friction_constant;
  gdouble spring_length;

  /* to drag and select nodes */
  gboolean selecting;
  float x1;
  float y1;
  float x2;
  float y2;

  /* to middle click and rotate the entire scene */
  gboolean rotating;

  /* middle click and CTRL to pan the scene */
  gboolean panning;

  /* moving the hovered node */
  gboolean moving;

  /* when we label edges, we add them to this list to easily remove their labels
   * on the next event we process instead of having to search the entire list
   * of edges each time */
  GList *labelled_edges;

  /* when we label nodes, we add them to this list to easily remove their
   * labels on the next event we process instead of having to search the entire
   * list of nodes each time */
  GList *labelled_nodes;

  GtkWidget *menu;
  GtkActionGroup *select_actions;
  GtkActionGroup *view_actions;

  GtkEntryCompletion *search_completion;
  GtkTreeModel *visible_nodes_model;
  GtkWidget *search_entry;
  GtkWidget *search_window;

  /* set a timeout to hide searching window if no activity after 5 seconds */
  guint searching_id;

  /* store graph force properties etc in gsettings */
  GSettings *settings;
  GSettings *font_settings;

  /* debugging timing etc */
  gboolean debug;
  GTimer *debug_timer;
  gdouble render_ms;
};

/* signals we emit */
enum
{
  PROCESSING,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

#define REPLAY_GRAPH_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_GRAPH_VIEW, ReplayGraphViewPrivate))

/* error handling */
#define REPLAY_GRAPH_VIEW_ERROR replay_graph_view_error_quark()

static GQuark replay_graph_view_error_quark(void)
{
  return g_quark_from_static_string("state-graph-error-quark");
}

/* different error codes */
enum {
  INVALID_EDGE_ERROR = 0,
  INVALID_NODE_ERROR,
};

static void remove_node(ReplayGraphView *self, ReplayGraphViewNode *node);

/* function prototypes */
static void replay_graph_view_dispose(GObject *object);
static void replay_graph_view_finalize(GObject *object);
static gboolean replay_graph_view_draw(GtkWidget *widget,
                                    cairo_t *cr);
static void replay_graph_view_realize(GtkWidget *widget);
static void replay_graph_view_unrealize(GtkWidget *widget);
static gboolean replay_graph_view_configure_event(GtkWidget *widget,
                                               GdkEventConfigure *configure);
static gboolean replay_graph_view_map_event(GtkWidget *widget,
                                         GdkEventAny *event);
static gboolean replay_graph_view_unmap_event(GtkWidget *widget,
                                           GdkEventAny *event);
static gboolean replay_graph_view_visibility_notify_event(GtkWidget *widget,
                                                       GdkEventVisibility *event);
static gboolean replay_graph_view_button_press_event(GtkWidget *widget,
                                                  GdkEventButton *event);
static gboolean replay_graph_view_button_release_event(GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean replay_graph_view_motion_notify_event(GtkWidget *self,
                                                   GdkEventMotion *event);
static gboolean replay_graph_view_scroll_event(GtkWidget *widget,
                                            GdkEventScroll *event);
static gboolean replay_graph_view_key_press_event(GtkWidget *widget,
                                               GdkEventKey *event);
/*
 * Render graph - may or may not succeed since we ratelimit actual renders of
 * graph to max_fps - always returns TRUE (useful for using as a timeout
 * callback)
 */
static gboolean replay_graph_view_render_ratelimit(gpointer data);
static void replay_graph_view_render_graph(ReplayGraphView *self);
static gboolean replay_graph_view_run_animation(ReplayGraphView *self);
static gboolean replay_graph_view_stop_animation(ReplayGraphView *self);

/* zero point of interest */
static const float zero = 0.0f;

/* UI callbacks */
static void invert_selection_cb(GtkAction *action,
                                gpointer data);
static void toggle_two_dimensional_cb(GtkAction *action,
                                      gpointer data);

static GtkActionEntry action_entries[] =
{
  /* name, stock_id, label, accelerator, tooltip, callback */
  { "MenuAction", NULL, NULL },
  { "InvertSelectionAction", NULL, N_("Invert selection"), "<control>I",
    "Invert selection", G_CALLBACK(invert_selection_cb) },
};

static GtkToggleActionEntry toggle_entries[] =
{
  { "Toggle2DAction", NULL, N_("View 2D mode"), "<control>2",
    "View in 2D mode", G_CALLBACK(toggle_two_dimensional_cb), FALSE },
};

static const gchar * const ui = "<ui>"
                                "<popup name='ReplayGraphViewMenu' action='MenuAction'>"
                                "<menuitem action='Toggle2DAction'/>"
                                "<menuitem action='InvertSelectionAction'/>"
                                "</popup>"
                                "</ui>";

static void
replay_graph_view_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  ReplayGraphView *self = REPLAY_GRAPH_VIEW(object);

  switch (prop_id) {
    case PROP_SELECTED_NODE_IDS:
      g_value_set_pointer(value, replay_graph_view_get_selected_node_ids(self));
      break;

    case PROP_MAX_FPS:
      g_value_set_double(value, replay_graph_view_get_max_fps(self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
replay_graph_view_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  ReplayGraphView *self = REPLAY_GRAPH_VIEW(object);

  switch (prop_id) {
    case PROP_SELECTED_NODE_IDS:
      replay_graph_view_set_selected_node_ids(self, g_value_get_pointer(value));
      break;

    case PROP_MAX_FPS:
      replay_graph_view_set_max_fps(self, g_value_get_double(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static ReplayGraphViewNode *get_canonical_node(ReplayGraphViewNode *node)
{
  g_assert(node != NULL);

  /* if node has no parent, it is the canonical one */
  if (node->parent == NULL)
  {
    return node;
  }
  return get_canonical_node(node->parent);
}


static gboolean node_is_in_array(ReplayGraphViewNode *node,
                                 GPtrArray *array)
{
  return (node->index < array->len ?
          ((ReplayGraphViewNode *)g_ptr_array_index(array, node->index) ==
           node) :
          FALSE);
}

/* apply a force to each node, either attractive or repulsive */
static void add_force_to_nodes(ReplayGraphView *self,
                               ReplayGraphViewNode *a,
                               ReplayGraphViewNode *b,
                               gboolean attract,
                               gdouble attract_weight)
{
  ReplayGraphViewPrivate *priv;
  float dx, dy, dz, dist, force;

  priv = self->priv;

  /* should only add force to nodes which have no parent */
  g_assert(a->parent == NULL && b->parent == NULL);
  g_assert(a != b);

  dx = b->sx - a->sx;
  dy = b->sy - a->sy;
  dz = b->sz - a->sz;

  dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));

  /* make dist always greater than or equal to 1 to prevent div / 0 errors */
  dist = MAX(dist, 0.0000001);

  if (attract)
  {
    /* elastic forces: we basically want a linear force as the distance moves
     * away from the ideal spring length - but augment based on attract_weight
     * such that those with a lower attract weight exert a smaller force */
    force = MIN(0.0f, (priv->attract_constant *
                       (dist - (priv->spring_length / attract_weight)) *
                       attract_weight));
  }
  else
  {
    /* gravitational force */
    force = priv->repel_constant * a->mass * b->mass / (dist * dist);
  }
  a->fx += (force * (dx/dist));
  a->fy += (force * (dy/dist));
  a->fz += (force * (dz/dist));

  b->fx -= (force * (dx/dist));
  b->fy -= (force * (dy/dist));
  b->fz -= (force * (dz/dist));
}


/*!
 * GObject related stuff
 */
static void replay_graph_view_class_init(ReplayGraphViewClass *klass)
{
  GObjectClass *obj_class;
  GtkWidgetClass *widget_class;

  obj_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  /* override GObject finalize */
  obj_class->get_property = replay_graph_view_get_property;
  obj_class->set_property = replay_graph_view_set_property;
  obj_class->dispose = replay_graph_view_dispose;
  obj_class->finalize = replay_graph_view_finalize;

  /* override gtk widget handler for draw, realize and configure */
  widget_class->draw = replay_graph_view_draw;
  widget_class->configure_event = replay_graph_view_configure_event;
  widget_class->realize = replay_graph_view_realize;
  widget_class->unrealize = replay_graph_view_unrealize;
  widget_class->map_event = replay_graph_view_map_event;
  widget_class->unmap_event = replay_graph_view_unmap_event;
  widget_class->visibility_notify_event = replay_graph_view_visibility_notify_event;
  widget_class->button_press_event = replay_graph_view_button_press_event;
  widget_class->button_release_event = replay_graph_view_button_release_event;
  widget_class->motion_notify_event = replay_graph_view_motion_notify_event;
  widget_class->scroll_event = replay_graph_view_scroll_event;
  widget_class->key_press_event = replay_graph_view_key_press_event;

  properties[PROP_SELECTED_NODE_IDS] = g_param_spec_boxed("selected-node-ids",
                                                          "selected-node-ids",
                                                          "The ids of the nodes which are currently selected",
                                                          G_TYPE_STRV,
                                                          G_PARAM_READWRITE);
  g_object_class_install_property(obj_class, PROP_SELECTED_NODE_IDS, properties[PROP_SELECTED_NODE_IDS]);
  properties[PROP_MAX_FPS] = g_param_spec_double("max-fps",
                                                 "max-fps",
                                                 "The maximum framerate to animate",
                                                 0.0, DEFAULT_MAX_FPS, DEFAULT_MAX_FPS,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property(obj_class, PROP_MAX_FPS, properties[PROP_MAX_FPS]);
  signals[PROCESSING] = g_signal_new("processing",
                                    G_OBJECT_CLASS_TYPE(obj_class),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET(ReplayGraphViewClass,
                                                    processing),
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__BOOLEAN,
                                    G_TYPE_NONE,
                                    1,
                                    G_TYPE_BOOLEAN);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayGraphViewPrivate));
}

static void unselect_all_nodes(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv = self->priv;

  g_hash_table_remove_all(priv->selected_nodes);
  g_hash_table_remove_all(priv->picked_nodes);
}


static void add_name_to_array(gpointer key,
                              gpointer value,
                              gpointer user_data)
{
  gchar *name = g_strdup((gchar *)key);
  g_ptr_array_add((GPtrArray *)user_data, name);
}

void
replay_graph_view_set_selected_node_ids(ReplayGraphView *self,
                                     const gchar **node_ids)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  unselect_all_nodes(self);

  if (node_ids) {
    const gchar **id;
    for (id = node_ids; *id != NULL; id++) {
      ReplayGraphViewNode *node = g_hash_table_lookup(priv->node_table, id);
      /* add to selected if node is visible, otherwise ignore */
      if (node && node_is_in_array(node, priv->visible_nodes)) {
        g_hash_table_insert(priv->selected_nodes, node->name, node);
      }
    }
  }
  g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_SELECTED_NODE_IDS]);
}

/**
 * replay_graph_view_get_selected_node_ids:
 * @self: The #ReplayGraphView
 *
 * Returns: (transfer full): A NULL-terminated array of the selected node ids
 */
gchar **
replay_graph_view_get_selected_node_ids(ReplayGraphView *self) {
  ReplayGraphViewPrivate *priv;
  GPtrArray *names = NULL;
  gchar **ids = NULL;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), NULL);

  priv = self->priv;

  /* only do something if have selected / picked / hovered node */
  if (priv->hovered_node ||
      g_hash_table_size(priv->picked_nodes) ||
      g_hash_table_size(priv->selected_nodes))
  {
    /* make sure hovered node is not selected / picked otherwise we will
     * try to add it twice */
    names = g_ptr_array_new();
    if (priv->hovered_node)
    {
      g_hash_table_remove(priv->picked_nodes,
                          priv->hovered_node->name);
      g_hash_table_remove(priv->selected_nodes,
                          priv->hovered_node->name);
    }
    g_hash_table_foreach(priv->picked_nodes,
                         add_name_to_array,
                         names);
    g_hash_table_foreach(priv->selected_nodes,
                         add_name_to_array,
                         names);
    if (priv->hovered_node)
    {
      g_ptr_array_add(names, g_strdup(priv->hovered_node->name));
    }
    /* null terminate */
    g_ptr_array_add(names, NULL);
    ids = (gchar **)names->pdata;
    /* free GPtrArray but keep pdata to return as ids */
    g_ptr_array_free(names, FALSE);
  }
  return ids;
}

/**
 * replay_graph_view_get_max_fps:
 * @self: The #ReplayGraphView
 *
 * Returns: The current maximum fps
 */
gdouble
replay_graph_view_get_max_fps(ReplayGraphView *self) {
  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), 0.0);
  return self->priv->max_fps;
}

/**
 * replay_graph_view_set_max_fps:
 * @self: The #ReplayGraphView
 * @max_fps: The maximum fps to set
 *
 */
void
replay_graph_view_set_max_fps(ReplayGraphView *self,
                           gdouble max_fps) {
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;
  if (priv->max_fps != max_fps) {
    gboolean animated = (priv->render_timeout_id != 0);
    /* stop and restart animation using new max_fps */
    replay_graph_view_stop_animation(self);
    priv->max_fps = max_fps;
    if (animated) {
      replay_graph_view_run_animation(self);
    }
  }
}

static void toggle_two_dimensional_cb(GtkAction *action,
                                      gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(data));

  self = REPLAY_GRAPH_VIEW(data);
  priv = self->priv;

  replay_graph_view_set_two_dimensional(self, !priv->two_dimensional);
}

static void invert_selection(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  GHashTable *new_selected_nodes;
  unsigned int i;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* make sure hovered node is not selected otherwise we will try to add it
   * twice */
  if (priv->hovered_node)
  {
    g_hash_table_remove(priv->picked_nodes,
                        priv->hovered_node->name);
    g_hash_table_remove(priv->selected_nodes,
                        priv->hovered_node->name);
  }
  new_selected_nodes = g_hash_table_new(g_str_hash, g_str_equal);
  /* add all nodes from visible which are not in picked or selected_nodes
   * to new_selected_nodes */
  for (i = 0; i < priv->visible_nodes->len; i++)
  {
    ReplayGraphViewNode *node = (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes, i);
    if (g_hash_table_lookup(priv->selected_nodes, node->name) == NULL &&
        g_hash_table_lookup(priv->picked_nodes, node->name) == NULL)
    {
      g_hash_table_insert(new_selected_nodes, node->name, node);
    }
  }
  unselect_all_nodes(self);
  g_hash_table_destroy(priv->selected_nodes);
  priv->selected_nodes = new_selected_nodes;
}

static void invert_selection_cb(GtkAction *action,
                                gpointer data)
{
  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(data));

  invert_selection(REPLAY_GRAPH_VIEW(data));
}

static void replay_graph_view_apply_forces(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  unsigned int i;
  GHashTable *edges_table;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* put each GPtrArray of edges into hash table to make sure we don't apply
   * forces twice to same set of edges */
  edges_table = g_hash_table_new(g_direct_hash, g_direct_equal);

  /* pull node to centre and act on node by other nodes */
  for (i = 0; i < priv->visible_nodes->len; i++)
  {
    unsigned int j;
    ReplayGraphViewNode *node = g_ptr_array_index(priv->visible_nodes, i);

    /* pull node to centre */
    node->fx -= (priv->central_pull_constant * node->mass * node->sx);
    node->fy -= (priv->central_pull_constant * node->mass * node->sy);
    node->fz -= (priv->central_pull_constant * node->mass * node->sz);

    /* add repulsion forces ("gravity")
     * note: repulsion forces on this node by nodes earlier in the list
     * have already been calculated - so just go from here onwards in list */
    for (j = node->index; j < priv->visible_nodes->len; j++)
    {
      ReplayGraphViewNode *curr_node = g_ptr_array_index(priv->visible_nodes,
                                                      j);

      if (node != curr_node)
      {
        add_force_to_nodes(self, node, curr_node, FALSE, 0.0);
      }
    }

    /* add attraction forces ("elasticity") */
    for (j = 0; j < node->edges->len; j++)
    {
      GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, j);
      guint k;

      /* skip this set of edges if have already done for a previous node */
      if (g_hash_table_lookup(edges_table, edges))
      {
        continue;
      }

      g_hash_table_insert(edges_table, edges, edges);

      for (k = 0; k < edges->len; k++)
      {
        ReplayGraphViewEdge *edge =
          (ReplayGraphViewEdge *)g_ptr_array_index(edges, k);
        /* since links are one-way in the data structure,
         * we apply the force to both nodes */
        if (edge->head)
        {
          gdouble weight = edge_get_weight(edge);
          /* for attractive forces, we only add force if nodes aren't ignoring
           * their edges and if edge weight is non-zero */
          if (edge->tail != edge->head && weight &&
              !(node_is_in_array(edge->head, priv->hidden_nodes)))
          {
            add_force_to_nodes(self, edge->tail, edge->head, TRUE,
                               weight);
          }
        }
      }
    }
  }
  g_hash_table_destroy(edges_table);
}

static void reset_viewing_volume(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  GtkWidget *widget;
  float width, height, ratio;

  widget = GTK_WIDGET(self);
  priv = self->priv;

  /* recalculate from max_node_distance */
  priv->max_camera_distance = priv->max_node_distance * NODE_DISTANCE_TO_CAMERA_RATIO;
  /* z_far must be twice max camera distance for a spherical viewing volume */
  priv->z_far = priv->max_camera_distance * 2.0;
  priv->fovy = ((2.0 * atan2f(sqrtf(3 *
                                    (priv->max_node_distance *
                                     priv->max_node_distance)),
                              priv->max_camera_distance)) * 180.0 / M_PI);

  /* save transform state so we can restore back to current matrix */
  glPushAttrib(GL_TRANSFORM_BIT);
  /* reset view */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  width = (float)gtk_widget_get_allocated_width(widget);
  height = (float)gtk_widget_get_allocated_height(widget);
  ratio = width / height;
  /* set clipping volume */
  gluPerspective(priv->fovy, ratio, priv->z_near, priv->z_far);

  /* reset back to previous matrix */
  glPopAttrib();
}

static void update_node_position(ReplayGraphView *self,
                                 ReplayGraphViewNode *node,
                                 float dt)
{
  ReplayGraphViewPrivate *priv;
  float max;

  priv = self->priv;

  /* apply forces */
  node->vx += node->fx * dt;
  node->vy += node->fy * dt;
  node->vz += node->fz * dt;

  if (priv->friction_constant)
  {
    node->vx /= (priv->friction_constant * node->mass);
    node->vy /= (priv->friction_constant * node->mass);
    node->vz /= (priv->friction_constant * node->mass);
  }

  node->sx -= node->vx * dt;
  node->sy -= node->vy * dt;
  node->sz -= node->vz * dt;
  if (priv->two_dimensional)
  {
    node->sz = 0.0f;
  }

  max = MAX(MAX(fabs(node->sx), fabs(node->sy)), fabs(node->sz));

  /* expand viewing volume if needed */
  if (max + node->radius > priv->max_node_distance)
  {
    priv->max_node_distance = max + node->radius;
  }

  /* reset forces */
  node->fx = 0;
  node->fy = 0;
  node->fz = 0;
}

static gboolean replay_graph_view_update_node_positions(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  guint i;
  float old_max_node_distance;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), FALSE);

  priv = self->priv;

  /* we are called from layout thread so must take lock to make sure graph
   * does not change under us from main thread */
  g_mutex_lock(&priv->lock);

  replay_graph_view_apply_forces(self);

  /* save old max position */
  old_max_node_distance = priv->max_node_distance;

  /* Move visible nodes */
  for (i = 0; i < priv->visible_nodes->len; i++)
  {
    ReplayGraphViewNode *node = g_ptr_array_index(priv->visible_nodes, i);

    update_node_position(self, node, 1.0);
  }
  g_mutex_unlock(&priv->lock);

  /* if changed get main loop to reset view */
  if (priv->max_node_distance > old_max_node_distance)
  {
    priv->reset_viewing_volume = TRUE;
  }
  return TRUE;
}

static gboolean hide_search_window(gpointer data)
{
  ReplayGraphViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);

  priv = REPLAY_GRAPH_VIEW(data)->priv;

  gtk_widget_hide(priv->search_window);

  /* only call once */
  return FALSE;
}

static void search_entry_changed(GtkEditable *editable,
                                 gpointer data)
{
  guint i;
  gchar *text;
  size_t len;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(data));

  priv = REPLAY_GRAPH_VIEW(data)->priv;

  /* unselect any existing selection */
  g_hash_table_remove_all(priv->selected_nodes);

  text = gtk_editable_get_chars(editable, 0, -1);
  if (text)
  {
    len = strlen(text);

    if (len > 0)
    {
      /* select all which match on start of type */
      for (i = 0; i < priv->visible_nodes->len; i++)
      {
        ReplayGraphViewNode *node =
          (ReplayGraphViewNode *) g_ptr_array_index(priv->visible_nodes, i);

        /* search on label */
        if (node->label && g_ascii_strncasecmp(text, node->label, len) == 0)
        {
          g_hash_table_insert(priv->selected_nodes, node->name, node);
        }
      }
    }
    g_free(text);
  }
}

static gboolean search_entry_key_press_event(GtkWidget *widget,
                                             GdkEventKey *event,
                                             gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);

  self = REPLAY_GRAPH_VIEW(data);
  priv = self->priv;

  /* cancel timeout which automatically hides search entry */
  g_source_remove(priv->searching_id);
  priv->searching_id = 0;

  if (event->keyval == GDK_KEY_Escape)
  {
    /* just close window */
    hide_search_window(self);
  }
  else if (event->keyval == GDK_KEY_Return)
  {
    /* force a search on manual press of enter */
    search_entry_changed(GTK_EDITABLE(priv->search_entry), self);

    /* then close window */
    hide_search_window(self);
  }
  else
  {
    /* extend timeout which automatically hides search entry */
    priv->searching_id = g_timeout_add_seconds(5,
                                               hide_search_window,
                                               self);
  }
  /* make sure the signal keeps propagating so text gets inserted */
  return FALSE;
}

static gboolean search_entry_focus_out_event(GtkWidget *widget,
                                             GdkEventFocus *event,
                                             gpointer data)
{
  ReplayGraphViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);

  priv = REPLAY_GRAPH_VIEW(data)->priv;

  if (priv->searching_id)
  {
    g_source_remove(priv->searching_id);
    priv->searching_id = 0;
  }
  hide_search_window(REPLAY_GRAPH_VIEW(data));

  /* GtkEntry needs focus out event itself */
  return FALSE;
}

static void replay_graph_view_settings_changed(GSettings *settings,
                                            gchar *key,
                                            gpointer user_data)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(user_data));

  priv = REPLAY_GRAPH_VIEW(user_data)->priv;

  if (g_ascii_strcasecmp(key, ATTRACT_CONSTANT_KEY) == 0)
  {
    priv->attract_constant = g_settings_get_double(settings,
                                                   ATTRACT_CONSTANT_KEY);
  }
  else if (g_ascii_strcasecmp(key, REPEL_CONSTANT_KEY) == 0)
  {
    priv->repel_constant = g_settings_get_double(settings,
                                                 REPEL_CONSTANT_KEY);
  }
  else if (g_ascii_strcasecmp(key, CENTRAL_PULL_CONSTANT_KEY) == 0)
  {
    priv->central_pull_constant = g_settings_get_double(settings,
                                                        CENTRAL_PULL_CONSTANT_KEY);
  }
  else if (g_ascii_strcasecmp(key, FRICTION_CONSTANT_KEY) == 0)
  {
    priv->friction_constant = g_settings_get_double(settings,
                                                    FRICTION_CONSTANT_KEY);
  }
  else if (g_ascii_strcasecmp(key, SPRING_LENGTH_KEY) == 0)
  {
    priv->spring_length = g_settings_get_double(settings,
                                                SPRING_LENGTH_KEY);
  }
}

static void update_font_description(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  PangoFontDescription *font_desc;
  gchar *font_name;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  font_name = g_settings_get_string(priv->font_settings, MONOSPACE_FONT_KEY);
  if (font_name == NULL)
  {
    font_name = g_strdup(DEFAULT_MONOSPACE_FONT);
  }
  font_desc = pango_font_description_from_string(font_name);
  gtk_widget_override_font(GTK_WIDGET(self), font_desc);
  pango_font_description_free(font_desc);
  g_free(font_name);
}

static void monospace_font_changed(GSettings *settings,
                                   gchar *key,
                                   gpointer user_data)
{
  ReplayGraphView *self;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(user_data));

  self = REPLAY_GRAPH_VIEW(user_data);

  if (g_ascii_strcasecmp(key, MONOSPACE_FONT_KEY) == 0)
  {
    update_font_description(self);
  }

  /* force an update */
  replay_graph_view_render_ratelimit(self);
}

static void reset_view_parameters(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* reset to initial values */
  priv->z_near = 0.1;
  priv->z_far = 1000;

  priv->min_camera_distance = priv->z_near * 10.0;
  /* allow camera to be at radius of spherical viewing volume */
  priv->max_camera_distance = priv->z_far / 2.0;
  priv->max_node_distance = priv->max_camera_distance / NODE_DISTANCE_TO_CAMERA_RATIO;

  /* calculate fov in y axis */
  priv->fovy = ((2.0 * atan2f(sqrtf(3 *
                                    (priv->max_node_distance *
                                     priv->max_node_distance)),
                              priv->max_camera_distance)) * 180.0 / M_PI);

  /* reset to zero point of interest */
  priv->pIx = &zero;
  priv->pIy = &zero;
  priv->pIz = &zero;

  /* reset initial values */
  priv->x = 0.0;
  priv->y = 0.0;
  priv->z = priv->max_camera_distance;
}

static void layout_thread_run(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  priv->layout_context = g_main_context_new();
  priv->layout_loop = g_main_loop_new(priv->layout_context, FALSE);
  /* make all async operations started from this thread run on this
     layout_context rather than in the main loop */
  g_main_context_push_thread_default(priv->layout_context);
  g_main_loop_run(priv->layout_loop);
}

static void replay_graph_view_init(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  GtkWidget *widget;
  GtkUIManager *ui_manager;
  GError *error = NULL;
  GIcon *icon;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  self->priv = priv = REPLAY_GRAPH_VIEW_GET_PRIVATE(self);

  widget = GTK_WIDGET(self);
  priv->max_fps = DEFAULT_MAX_FPS;
  priv->node_table = g_hash_table_new(g_str_hash, g_str_equal);
  priv->edge_table = g_hash_table_new(g_str_hash, g_str_equal);
  priv->show_hud = TRUE;

  priv->visible_nodes = g_ptr_array_new();
  priv->hidden_nodes = g_ptr_array_new();
  priv->selected_nodes = g_hash_table_new(g_str_hash, g_str_equal);
  priv->picked_nodes = g_hash_table_new(g_str_hash, g_str_equal);

  reset_view_parameters(self);

  priv->settings = g_settings_new("au.gov.defence.dsto.parallax.replay.graph-view");
  g_signal_connect(priv->settings, "changed",
                   G_CALLBACK(replay_graph_view_settings_changed), self);
  priv->central_pull_constant = g_settings_get_double(priv->settings,
                                                      CENTRAL_PULL_CONSTANT_KEY);
  priv->central_pull_constant = (priv->central_pull_constant ?
                                 priv->central_pull_constant :
                                 -0.005);
  priv->attract_constant = g_settings_get_double(priv->settings,
                                                 ATTRACT_CONSTANT_KEY);
  priv->attract_constant = (priv->attract_constant ?
                            priv->attract_constant :
                            -0.5);
  priv->repel_constant = g_settings_get_double(priv->settings,
                                               REPEL_CONSTANT_KEY);
  priv->repel_constant = (priv->repel_constant ?
                          priv->repel_constant :
                          100.0);
  priv->friction_constant = g_settings_get_double(priv->settings,
                                                  FRICTION_CONSTANT_KEY);
  priv->friction_constant = (priv->friction_constant ?
                             priv->friction_constant :
                             2.0);
  priv->spring_length = g_settings_get_double(priv->settings,
                                              SPRING_LENGTH_KEY);
  priv->spring_length = (priv->spring_length ?
                         priv->spring_length :
                         60.0);

  /* set to use monospace font and watch for updates to key so can react */
  priv->font_settings = g_settings_new("org.gnome.desktop.interface");
  g_signal_connect(priv->font_settings, "changed",
                   G_CALLBACK(monospace_font_changed), self);

  update_font_description(self);

  /* setup OpenGL stuff from GtkGLExt - this may fail if no opengl, so check it
   * succeeds */
  priv->gl_config = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
                                              GDK_GL_MODE_ALPHA |
                                              GDK_GL_MODE_DEPTH |
                                              GDK_GL_MODE_DOUBLE |
                                              GDK_GL_MODE_MULTISAMPLE);
  if (priv->gl_config == NULL)
  {
    priv->gl_config = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
                                                GDK_GL_MODE_ALPHA |
                                                GDK_GL_MODE_DEPTH |
                                                GDK_GL_MODE_SINGLE |
                                                GDK_GL_MODE_MULTISAMPLE);
    if (priv->gl_config == NULL)
    {
      g_warning("could not start up opengl environment\n");
    }
  }
  if (priv->gl_config)
  {
    if (!gtk_widget_set_gl_capability(widget,
                                      priv->gl_config,
                                      NULL,
                                      TRUE,
                                      GDK_GL_RGBA_TYPE))
    {
      g_warning("could not set gl edge on widget - destroying gl_config");
      g_object_unref(priv->gl_config);
      priv->gl_config = NULL;
    }
  }

  /* set CAN_FOCUS */
  gtk_widget_set_can_focus(widget, TRUE);

  priv->search_entry = gtk_entry_new();
  icon = g_themed_icon_new_with_default_fallbacks("edit-find-symbolic");
  gtk_entry_set_icon_from_gicon(GTK_ENTRY(priv->search_entry),
                                GTK_ENTRY_ICON_PRIMARY,
                                icon);
  g_object_unref(icon);

  priv->select_actions = gtk_action_group_new("ReplayGraphViewSelectActions");
  gtk_action_group_set_translation_domain(priv->select_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions(priv->select_actions, action_entries,
                               G_N_ELEMENTS(action_entries),
                               self);
  priv->view_actions = gtk_action_group_new("ReplayGraphViewActions");
  gtk_action_group_set_translation_domain(priv->view_actions, GETTEXT_PACKAGE);
  gtk_action_group_add_toggle_actions(priv->view_actions, toggle_entries,
                                      G_N_ELEMENTS(toggle_entries),
                                      self);

  ui_manager = gtk_ui_manager_new();
  gtk_ui_manager_insert_action_group(ui_manager, priv->select_actions, 0);
  gtk_ui_manager_insert_action_group(ui_manager, priv->view_actions, 0);
  if (!gtk_ui_manager_add_ui_from_string(ui_manager, ui, -1, &error))
  {
    g_error("error creating menu ui from string: '%s': %s", ui, error->message);
  }
  priv->menu = gtk_ui_manager_get_widget(ui_manager, "/ReplayGraphViewMenu");
  gtk_menu_attach_to_widget(GTK_MENU(priv->menu), widget, NULL);
  g_object_unref(ui_manager);

  /* create a toplevel window with popup menu type so it doesn't show in taskbar
   * or get drawn with decorations etc for search entry */
  priv->search_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(priv->search_window),
                           GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_container_set_border_width(GTK_CONTAINER(priv->search_window),
                                 5);
  gtk_container_add(GTK_CONTAINER(priv->search_window),
                    priv->search_entry);

  /* do selections when user edits entry */
  g_signal_connect(GTK_EDITABLE(priv->search_entry),
                   "changed",
                   G_CALLBACK(search_entry_changed),
                   self);

  /* get key press events so we can hide entry when esc is pressed */
  g_signal_connect(priv->search_entry,
                   "key-press-event",
                   G_CALLBACK(search_entry_key_press_event),
                   self);

  /* get focus out events so we can hide entry when we lose focus */
  g_signal_connect(priv->search_entry,
                   "focus-out-event",
                   G_CALLBACK(search_entry_focus_out_event),
                   self);

  /* just show entry for now - can present window later */
  gtk_widget_show_all(priv->search_entry);

  g_mutex_init(&priv->lock);
  /* create layout thread */
  priv->layout_thread = g_thread_new("graph-layout",
                                     (GThreadFunc)layout_thread_run,
                                     self);
}

static void do_popup_menu(ReplayGraphView *self, GdkEventButton *event)
{
  int button, event_time;
  ReplayGraphViewPrivate *priv;
  gboolean sensitive;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* set sensitivities */
  sensitive = (g_hash_table_size(priv->selected_nodes) > 0 ||
               g_hash_table_size(priv->picked_nodes) > 0 ||
               priv->hovered_node);

  gtk_action_group_set_sensitive(priv->select_actions, sensitive);

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

static void
gl_pango_render_layout(PangoLayout *layout, const PangoRectangle *rect)
{
  int stride;
  cairo_surface_t *surf;
  cairo_t *cr;
  unsigned char *surface_data;
  cairo_operator_t op;

  /* allocate data for a cairo surface to draw onto */
  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, rect->width);
  surface_data = g_malloc(stride * rect->height);
  surf = cairo_image_surface_create_for_data(surface_data,
                                             CAIRO_FORMAT_ARGB32,
                                             rect->width,
                                             rect->height,
                                             stride);

  cr = cairo_create(surf);
  /* cr now has reference on surf so we can drop our reference */
  cairo_surface_destroy(surf);

  /* clear surface */
  op = cairo_get_operator(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0.0f, 0.0f, 0.0f, 0.0f);
  cairo_paint(cr);
  cairo_set_operator(cr, op);

  cairo_rectangle(cr, 0.0, 0.0, rect->width, rect->height);
  /* draw black background with high alpha so occludes whatever is behind it
   * so we can see text more easily but still adds nice transparent effect */
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.8);
  cairo_fill(cr);

  /* default text colour of white */
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

  /* draw our actual text */
  cairo_move_to(cr, 0.0, 0.0);
  pango_cairo_show_layout(cr, layout);
  cairo_destroy(cr);

  /* blend over existing elements - save current blend state then restore */
  glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /* make depth buffer read only when drawing transparent nodes (such as text
   * which has a transparent background) */
  glDepthMask(GL_FALSE);

  /* need to invert since y is inverted for cairo compared to opengl */
  glPixelZoom(1.0, -1.0);

  /* need to use BGRA rather than RGBA since blue and red pixel's are packed
   * this way */
  glDrawPixels(rect->width, rect->height,
               GL_BGRA_EXT, GL_UNSIGNED_BYTE,
               surface_data);

  /* reset pixel zoom */
  glPixelZoom(1.0, 1.0);

  /* restore previous blending state */
  glPopAttrib();
  g_free(surface_data);
}

static void draw_markup_gl(ReplayGraphView *self,
                           const gchar *markup,
                           float x, float y, float z)
{
  ReplayGraphViewPrivate *priv;
  PangoRectangle rect;
  GLfloat text_w, text_h;
  GLfloat tangent_h;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* use HUD PangoLayout so we can measure pixel extents */
  pango_layout_set_markup(priv->layout, markup, -1);

  pango_layout_get_pixel_extents(priv->layout, NULL, &rect);
  text_w = (GLfloat)rect.width;
  text_h = (GLfloat)rect.height;

  /*
   * We need to account for the position in z-space of the text
   * tangent = priv->z_near * tan (priv->fovy / 2.0 * G_PI / 180.0)
   * w = gtk_widget_get_allocated_width(widget)
   * h = gtk_widget_get_allocated_height(widget)
   *
   * x = x + -1.0 * (text_w/w) * tangent * (w/h) = -text_w * tangent / h
   * y = y + -1.0 * (text_h/h) * tangent         = -text_h * tangent / h
   * z = z
   */
  tangent_h = priv->z_near * tan(priv->fovy / 2.0 * G_PI / 180.0);
  tangent_h /= (GLfloat)gtk_widget_get_allocated_height(GTK_WIDGET(self));
  glRasterPos3f((-text_w * tangent_h) + x,
                (-text_h * tangent_h) + y,
                z);

  /* Render text */
  gl_pango_render_layout(priv->layout, &rect);
}

static void
curved_rectangle(cairo_t *cr,
                 double x_0,
                 double y_0,
                 double width,
                 double height,
                 double radius)
{
  double x_1, y_1;
  double x_2, y_2;

  g_return_if_fail(width > 0.0 && height > 0.0);

  x_1 = x_0 + width;
  y_1 = y_0 + height;
  x_2 = (x_0 + x_1) / 2.0;
  y_2 = (y_0 + y_1) / 2.0;

  if (width / 2.0 < radius)
  {

    if (height / 2.0 < radius)
    {
      cairo_move_to(cr, x_0, y_2);
      cairo_curve_to(cr, x_0, y_0, x_0, y_0, x_2, y_0);
      cairo_curve_to(cr, x_1, y_0, x_1, y_0, x_1, y_2);
      cairo_curve_to(cr, x_1, y_1, x_1, y_1, x_2, y_1);
      cairo_curve_to(cr, x_0, y_1, x_0, y_1, x_0, y_2);
    }
    else
    {
      cairo_move_to(cr, x_0, y_0 + radius);
      cairo_curve_to(cr, x_0, y_0, x_0, y_0, x_2, y_0);
      cairo_curve_to(cr, x_1, y_0, x_1, y_0, x_1, y_0 + radius);
      cairo_line_to(cr, x_1, y_1 - radius);
      cairo_curve_to(cr, x_1, y_1, x_1, y_1, x_2, y_1);
      cairo_curve_to(cr, x_0, y_1, x_0, y_1, x_0, y_1 - radius);
    }
  }
  else
  {
    if (height / 2 < radius)
    {
      cairo_move_to(cr, x_0, y_2);
      cairo_curve_to(cr, x_0, y_0, x_0, y_0, x_0 + radius, y_0);
      cairo_line_to(cr, x_1 - radius, y_0);
      cairo_curve_to(cr, x_1, y_0, x_1, y_0, x_1, y_2);
      cairo_curve_to(cr, x_1, y_1, x_1, y_1, x_1 - radius, y_1);
      cairo_line_to(cr, x_0 + radius, y_1);
      cairo_curve_to(cr, x_0, y_1, x_0, y_1, x_0, y_2);
    }
    else
    {
      cairo_move_to(cr, x_0, y_0 + radius);
      cairo_curve_to(cr, x_0, y_0, x_0, y_0, x_0 + radius, y_0);
      cairo_line_to(cr, x_1 - radius, y_0);
      cairo_curve_to(cr, x_1, y_0, x_1, y_0, x_1, y_0 + radius);
      cairo_line_to(cr, x_1, y_1 - radius);
      cairo_curve_to(cr, x_1, y_1, x_1, y_1, x_1 - radius, y_1);
      cairo_line_to(cr, x_0 + radius, y_1);
      cairo_curve_to(cr, x_0, y_1, x_0, y_1, x_0, y_1 - radius);
    }
  }
  cairo_close_path(cr);
}

#define TEXT_BORDER 4

static void draw_markup_to_hud(ReplayGraphView *self,
                               gchar *markup,
                               float x, float y, float z)
{
  /* we need to convert the world coordinates to projected coords */
  GLint viewport[4];
  GLdouble modelview[16];
  GLdouble projection[16];
  GLdouble winx, winy, winz;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* get matrices to use gluProject */
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
  glGetDoublev(GL_PROJECTION_MATRIX, projection);
  glGetIntegerv(GL_VIEWPORT, viewport);

  /* convert our current location to screen coords */
  if (gluProject(x, y, z, modelview, projection, viewport, &winx, &winy, &winz))
  {
    PangoRectangle rect;
    gint width, height, avail_width, avail_height;
    int hudx, hudy;

    hudx = (int)winx;

    /* need to reverse y though */
    hudy = viewport[3] -(int)winy;
    hudx = CLAMP(hudx, 0, gtk_widget_get_allocated_width(GTK_WIDGET(self)));
    hudy = CLAMP(hudy, 0, gtk_widget_get_allocated_height(GTK_WIDGET(self)));

    width = pango_layout_get_width(priv->layout) / PANGO_SCALE;
    height = pango_layout_get_height(priv->layout) / PANGO_SCALE;

    /* determine the maximum available width and height for this layout to fit
     * within given where we want to position it */
    avail_width = width - (hudx + (2 * TEXT_BORDER + 0.5));
    avail_height = height - (hudy + (2 * TEXT_BORDER + 0.5));

    /* draw onto cairo surface */
    pango_layout_set_markup(priv->layout, markup, -1);

    pango_layout_get_pixel_extents(priv->layout, NULL, &rect);

    /* if layout takes more room than is available then give required room */
    if (rect.width > avail_width)
    {
      hudx = MAX(0, hudx - (rect.width - avail_width));
    }
    if (rect.height > avail_height)
    {
      hudy = MAX(0, hudy - (rect.height - avail_height));
    }

    /* offset by half to get crisp lines */
    curved_rectangle(priv->cr,
                     hudx + 0.5,
                     hudy + 0.5,
                     rect.width + 2 * TEXT_BORDER,
                     rect.height + 2 * TEXT_BORDER,
                     2 * TEXT_BORDER);
    /* draw black background with high alpha so occludes whatever is behind it
     * so we can see text more easily but still adds nice transparent effect */
    cairo_set_source_rgba(priv->cr, 0.0, 0.0, 0.0, 0.8);
    cairo_fill_preserve(priv->cr);
    cairo_set_source_rgb(priv->cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(priv->cr, 0.5);
    cairo_stroke(priv->cr);

    /* draw text inside border */
    cairo_move_to(priv->cr, hudx + TEXT_BORDER, hudy + TEXT_BORDER);
    pango_cairo_show_layout(priv->cr, priv->layout);
  }
}

#define ARROW_MULTIPLIER 24.0
#define ARROW_HEIGHT_TO_WIDTH 3.0

static void draw_edge(ReplayGraphView *self,
                      ReplayGraphViewEdge *edge,
                      guint node_index,
                      guint group_index,
                      guint edge_index,
                      guint group_size,
                      gboolean invert)
{
  ReplayGraphViewPrivate *priv;
  float dx, dy, dz, d, dzy;
  float anglezy, anglezyx;
  gboolean highlight = FALSE;
  float tail_offset = 0.0, head_offset = 0.0;
  ReplayGraphViewNode *tail;
  ReplayGraphViewNode *head;
  GLfloat color[4];

  priv = self->priv;

  tail = edge->tail;
  head = edge->head;

  dx = (head ? head->sx : 0) - tail->sx;
  dy = (head ? head->sy : 0) - tail->sy;
  dz = (head ? head->sz : 0) - tail->sz;

  dzy = sqrtf((dz * dz) + (dy * dy));
  d = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
  anglezy = atan2f(dy, dz);
  anglezy *= 180.0 / M_PI;
  anglezyx = atan2f(dx, dzy);
  anglezyx *= 180.0 / M_PI;

  /* save lighting state */
  glPushAttrib(GL_LIGHTING_BIT);

  /* move to tail of node, and rotate around so z-axis is in
   * direction of connection - ie. we are looking down z-axis */
  glPushMatrix();
  glTranslatef(tail->sx, tail->sy, tail->sz);

  glRotatef(anglezy, -1.0f, 0.0f, 0.0f);
  glRotatef(anglezyx, 0.0f, 1.0f, 0.0f);

  /* now only draw edge between radius's of the two nodes */
  d -= tail->radius;
  if (head)
  {
    d -= head->radius;
  }

  /* if this is part of a group of edges, we need to offset it from the other
   * edges in the group - space evenly around a circle at half the radius from
   * the centre of the tail node */
  if (group_size > 1 && head)
  {
    /* since we have rotated so the vector between tail and head is along the z
     * axis we can simply calculate the points on the circle as cos & sin (x is
     * like -z and y is still y) */
    float angle = (invert ? -1 : 1) * (2*M_PI * edge_index / group_size);
    float odx, ody, odz, odzy, oanglezy, oanglezyx;
    glTranslatef(tail->radius / 2 * cosf(angle),
                 -tail->radius / 2 * sinf(angle),
                 0.0f);

    /* now we need to take into account that the head radius is different, since
     * we don't want just parallel lines for the edges - so rotate to look down
     * the vector between current position and distance d along the z axis, and
     * offset by the radius on y & x */
    odz = d;
    /* need to do negative since we have just done translation above which has
     * moved us in the wrong direction overall */
    ody = -(head->radius - tail->radius) / 2 * sinf(angle);
    odx = (head->radius - tail->radius) / 2 * cosf(angle);
    odzy = sqrtf((odz * odz) + (ody * ody));
    oanglezy = atan2f(ody, odz);
    oanglezy *= 180.0 / M_PI;
    oanglezyx = atan2f(odx, odzy);
    oanglezyx *= 180.0 / M_PI;
    glRotatef(oanglezy, -1.0f, 0.0f, 0.0f);
    glRotatef(oanglezyx, 0.0f, 1.0f, 0.0f);

    /* need to account for this movement out from center of node - boundary
       of node is now less than tail->radius - project y and x points onto
       sphere with radius tail->radius to get z and translate d by this
       much */
    tail_offset = tail->radius -
      sqrtf((tail->radius * tail->radius) -
            ((tail->radius / 2 * sinf(angle)) *
             (tail->radius / 2 * sinf(angle))) -
            ((tail->radius / 2 * cosf(angle)) *
             (tail->radius / 2 * cosf(angle))));

    head_offset = head->radius -
      sqrtf((head->radius * head->radius) -
            ((head->radius / 2 * sinf(angle)) *
             (head->radius / 2 * sinf(angle))) -
            ((head->radius / 2 * cosf(angle)) *
             (head->radius / 2 * cosf(angle))));
  }
  d += tail_offset;
  d += head_offset;

  /* set light depheading on if there are some selected nodes and if we are
   * one of the selected ones or hovered one or not */
  if (!priv->moving &&
      (priv->selecting ||
       g_hash_table_size(priv->selected_nodes) > 0 ||
       g_hash_table_size(priv->picked_nodes) > 0 ||
       priv->hovered_node ||
       priv->hovered_edge) &&
      head &&
      g_hash_table_lookup(priv->selected_nodes, tail->name) != tail &&
      g_hash_table_lookup(priv->selected_nodes, head->name) != head &&
      g_hash_table_lookup(priv->picked_nodes, tail->name) != tail &&
      g_hash_table_lookup(priv->picked_nodes, head->name) != head &&
      priv->hovered_node != tail &&
      priv->hovered_node != head &&
      priv->hovered_edge != edge)
  {
    /* not a selected / hovered edge so ignore me */
    glDisable(GL_LIGHT0);
  }

  /* always highlight edges which point to NULL or if event */
  if (head == NULL || edge->event_label)
  {
    highlight = TRUE;
  }

  edge_get_color(edge, color);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
  glMaterialfv(GL_FRONT, GL_EMISSION,
               (highlight ? highlight_emission : no_emission));

  /* push names to be able to look up edge if hovering */
  glPushName(node_index);
  glPushName(group_index);
  glPushName(edge_index);

  /* special case: link an node to itself - but don't draw self edges for group
   * nodes */
  if (tail == head)
  {
    if (!tail->group)
    {
      glPushMatrix();
      glTranslatef(0, 0, tail->radius - tail_offset);
      /* enable normalization so that when we scale (to increase size of an
       * edge) normals get re-normalized too */
      glPushAttrib(GL_TRANSFORM_BIT);
      /* since we scale by same amount in all axes, can use rescale normal
       * rather than full GL_NORMALIZE */
      glEnable(GL_RESCALE_NORMAL);
      glScalef(tail->radius, tail->radius, tail->radius);
      glCallList(priv->self_edge_display_list);
      glPopAttrib();
      glPopMatrix();
    }
  }
  else
  {
    gdouble weight;

    weight = edge_get_weight(edge);
    /* move to boundary of sphere */
    glTranslatef(0, 0, tail->radius - tail_offset);
    glPushMatrix();
    /* enable normalization so that when we scale (to increase size of an
     * edge) normals get re-normalized too */
    glPushAttrib(GL_TRANSFORM_BIT);
    glEnable(GL_NORMALIZE);
    if (edge->directed)
    {
      glScalef(weight, weight, d - EDGE_WIDTH * ARROW_MULTIPLIER);
    }
    else
    {
      glScalef(weight, weight, d);
    }
    glCallList(priv->edge_body_display_list);
    glPopAttrib();
    glPopMatrix();

    if (edge->directed)
    {
      glPushMatrix();
      glTranslatef(0, 0, d - EDGE_WIDTH * ARROW_MULTIPLIER);
      glCallList(priv->edge_head_display_list);
      glPopMatrix();
    }
  }

  glPopName();
  glPopName();
  glPopName();
  glPopMatrix();
  glPopAttrib();
}

static gboolean draw_node_dark(ReplayGraphView *self,
                               ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv;

  priv = self->priv;

  return (!priv->moving &&
          (priv->selecting ||
           g_hash_table_size(priv->selected_nodes) > 0 ||
           g_hash_table_size(priv->picked_nodes) > 0 ||
           priv->hovered_node ||
           priv->hovered_edge)
          &&
          g_hash_table_lookup(priv->selected_nodes, node->name) != node &&
          g_hash_table_lookup(priv->picked_nodes, node->name) != node &&
          priv->hovered_node != node &&
          (!priv->hovered_edge || (priv->hovered_edge->tail != node &&
                                   priv->hovered_edge->head != node)));
}

static void vector_mult_matrix(GLfloat x, GLfloat y, GLfloat z,
                               GLdouble matrix[16],
                               GLfloat result[3])
{
  result[0] = x * matrix[0] + y * matrix[1] + z * matrix[2];
  result[1] = x * matrix[4] + y * matrix[5] + z * matrix[6];
  result[2] = x * matrix[8] + y * matrix[9] + z * matrix[10];
}

static void draw_active_node_glows(ReplayGraphView *self,
                                   ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv;

  priv = self->priv;

  /* only draw non-child nodes */
  g_assert(node == get_canonical_node(node));
  g_assert(node->activities != NULL);

  if (!draw_node_dark(self, node))
  {
    GList *list;
    for (list = node->activities; list != NULL; list = list->next)
    {
      GLfloat point[3];
      GLfloat radius;
      NodeActivity *activity;
      GLfloat color[4];

      activity = (NodeActivity *)list->data;

      radius = 1.25 * node->radius;
      radius = radius * (1.0 + (2.0 * node_activity_get_level(activity)));

      glPushMatrix();
      glTranslatef(node->sx, node->sy, node->sz);

      glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT |
                   GL_TEXTURE_BIT);

      /* allow 2d stuff to blend over 3d */
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);

      /* make depth buffer read only when drawing transparent texture */
      glDepthMask(GL_FALSE);

      /* disable lighting for texture */
      glDisable(GL_LIGHTING);

      /* enable 2d texture */
      glEnable(GL_TEXTURE_2D);

      /* blend glow texture with node colour for blend environemnt color */
      node_activity_get_color(activity, color);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
      glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);

      /* map texture in scene */
      glBindTexture(GL_TEXTURE_2D, priv->active_node_glow_texture);

      glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
      glBegin(GL_QUADS);

      glTexCoord2d(0.0f, 0.0f);
      vector_mult_matrix(-radius, radius, 0.0,
                         priv->rotation, point);
      glVertex3fv(point);

      glTexCoord2f(0.0f, 1.0f);
      vector_mult_matrix(-radius, -radius, 0.0,
                         priv->rotation, point);
      glVertex3fv(point);

      glTexCoord2f(1.0f, 1.0f);
      vector_mult_matrix(radius, -radius, 0.0,
                         priv->rotation, point);
      glVertex3fv(point);

      glTexCoord2f(1.0f, 0.0f);
      vector_mult_matrix(radius, radius, 0.0,
                         priv->rotation, point);
      glVertex3fv(point);

      glEnd();
      glPopAttrib();
      glPopMatrix();
    }
  }
}

/* Draw a single node */
static void draw_node(ReplayGraphView *self,
                      ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv;

  priv = self->priv;

  /* only draw non-child nodes */
  g_assert(node == get_canonical_node(node));

  glPushMatrix();
  glTranslatef(node->sx, node->sy, node->sz);

  glPushAttrib(GL_LIGHTING_BIT);

  /* set light depending on if there are some selected nodes and if we are
   * one of the selected ones or hovered one */
  if (draw_node_dark(self, node))
  {
    glDisable(GL_LIGHT0);
  }

  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, node->color);
  glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
  glPushName(node->index);
  /* enable normalization so that when we scale (to increase size of an
   * edge) normals get re-normalized too */
  glPushAttrib(GL_TRANSFORM_BIT);
  /* since we scale by same amount in all axes, can use rescale normal
   * rather than full GL_NORMALIZE */
  glEnable(GL_RESCALE_NORMAL);
  glScalef(node->radius, node->radius, node->radius);
  glCallList((node->group ? priv->group_display_list :
              priv->node_display_list));
  glPopAttrib();
  glPopName();
  glPopMatrix();
  /* reset lights */
  glPopAttrib();
}

static void draw_active_node(ReplayGraphView *self,
                             ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv;
  GLfloat color[4];

  priv = self->priv;

  /* only draw non-child nodes */
  g_assert(node == get_canonical_node(node));
  g_assert(node->activities != NULL);

  glPushAttrib(GL_LIGHTING_BIT);
  glPushMatrix();
  glTranslatef(node->sx, node->sy, node->sz);

  /* set light depending on if there are some selected nodes and if we are
   * one of the selected ones or hovered one */
  if (draw_node_dark(self, node))
  {
    glDisable(GL_LIGHT0);
  }
  /* TODO: figure out a better way of choosing which color to use... for now
   * pick the first one */
  node_activity_get_color(((NodeActivity *)node->activities->data), color);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
  glMaterialfv(GL_FRONT, GL_EMISSION, highlight_emission);
  glPushName(node->index);
  /* enable normalization so that when we scale (to increase size of an
   * edge) normals get re-normalized too */
  glPushAttrib(GL_TRANSFORM_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  /* since we scale by same amount in all axes, can use rescale normal rather
   * than full GL_NORMALIZE */
  glEnable(GL_RESCALE_NORMAL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  /* scale by 1.25 of radius */
  glScalef(node->radius * 1.25, node->radius * 1.25, node->radius * 1.25);
  glCallList((node->group ? priv->group_display_list :
              priv->node_display_list));
  glPopAttrib();
  glPopName();
  glPopMatrix();
  /* reset lights */
  glPopAttrib();
}

/* Draw the edges for an node */
static void draw_node_edges(ReplayGraphView *self,
                            ReplayGraphViewNode *node,
                            GHashTable *edges_table)
{
  ReplayGraphViewPrivate *priv;
  GLuint i = 0;

  priv = self->priv;
  /* we should only be drawing nodes which don't have parents */
  g_assert(node == get_canonical_node(node));

  /* draw connecting edges */
  for (i = 0; i < node->edges->len; i++)
  {
    GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, i);
    guint j;

    if (g_hash_table_lookup(edges_table, edges))
    {
      /* already done this array */
      continue;
    }
    g_hash_table_insert(edges_table, edges, edges);

    for (j = 0; j < edges->len; j++)
    {
      ReplayGraphViewEdge *edge =
        (ReplayGraphViewEdge *)g_ptr_array_index(edges, j);
      /* if node is not tail, need to invert the index of this edge so that it
       * gets drawn at correct angle compared to other edges in this group */
      gboolean invert = (node != edge->tail);
      ReplayGraphViewNode *other = (node == edge->tail ?
                                 edge->head : edge->tail);

      /* only draw edge if other node is not hidden */
      if (!other || !node_is_in_array(other, priv->hidden_nodes))
      {
        draw_edge(self, edge, node->index,
                  i, j, edges->len, invert);
      }
    }
  }

}


/* Resets view of scene to point of interest - must be in GL mode */
static void replay_graph_view_reset_gl_view(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* reset to zero  */
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /* move everything away from zero point so camera appears to be not at zero */
  glTranslatef(-priv->x, -priv->y, -priv->z);

  /* do rotation */
  glMultMatrixd(priv->rotation);

  /* now translate from point of interest so it (point of interest)
   * appears to be where the camera is looking to */
  glTranslatef(-(*(priv->pIx)), -(*(priv->pIy)), -(*(priv->pIz)));
}

static void free_cairo_context(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv = self->priv;

  if (priv->cr)
  {
    g_object_unref(priv->layout);
    cairo_destroy(priv->cr);
    priv->cr = NULL;
  }

  if (priv->surface_data)
  {
    g_free(priv->surface_data);
    priv->surface_data = NULL;
  }

}

static void allocate_cairo_context(ReplayGraphView *self)
{
  GtkWidget *widget;
  ReplayGraphViewPrivate *priv;
  int width, height, stride;
  cairo_surface_t *surf;
  GtkStyleContext *style;
  const PangoFontDescription *desc;

  priv = self->priv;
  widget = GTK_WIDGET(self);

  width = gtk_widget_get_allocated_width(widget);
  height = gtk_widget_get_allocated_height(widget);

  free_cairo_context(self);

  stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
  priv->surface_data = g_malloc(stride * height);
  surf = cairo_image_surface_create_for_data(priv->surface_data,
                                             CAIRO_FORMAT_ARGB32,
                                             width,
                                             height,
                                             stride);
  /* priv->cr now has reference on surf so we can drop our reference */
  priv->cr = cairo_create(surf);
  cairo_surface_destroy(surf);
  /* set clip region */
  cairo_rectangle(priv->cr, 0.0, 0.0, width, height);
  cairo_clip(priv->cr);

  /* and create a pango layout to use for drawing to the context */
  priv->layout = pango_cairo_create_layout(priv->cr);
  pango_layout_set_width(priv->layout, width*PANGO_SCALE);
  pango_layout_set_height(priv->layout, height*PANGO_SCALE);
  pango_layout_set_wrap(priv->layout, PANGO_WRAP_WORD);
  style = gtk_widget_get_style_context(widget);
  desc = gtk_style_context_get_font(style, gtk_widget_get_state_flags(widget));
  pango_layout_set_font_description(priv->layout, desc);
  pango_cairo_context_set_font_options(pango_layout_get_context(priv->layout),
                                       gdk_screen_get_font_options(gdk_screen_get_default()));
  pango_cairo_context_set_resolution(pango_layout_get_context(priv->layout),
                                     gdk_screen_get_resolution(gdk_screen_get_default()));
}


/*
 * Realize signal handler - should initialise the OpenGL scene including
 * background colours, lighting etc
 */
static void replay_graph_view_realize(GtkWidget *widget)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(widget));
  self = REPLAY_GRAPH_VIEW(widget);

  priv = self->priv;

  /* call parent class realize first */
  GTK_WIDGET_CLASS(replay_graph_view_parent_class)->realize(widget);

  /* make our parent container get automatically redrawn when we change
   * allocation */
  gtk_container_set_reallocate_redraws(GTK_CONTAINER(gtk_widget_get_parent(widget)),
                                       TRUE);
  /* setup event mask to receive events */
  gtk_widget_add_events(GTK_WIDGET(self),
                        GDK_KEY_PRESS_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_SCROLL_MASK |
                        GDK_VISIBILITY_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK);

  if (priv->gl_config)
  {
    if (gtk_widget_begin_gl(widget))
    {
      GLUquadric *quadric;
      cairo_surface_t *glow_surf;

      /* set the clear color */
      glClearColor(0.0, 0.0, 0.0, 0.0);

      /* call back faces */
      glEnable(GL_CULL_FACE);

      /* for cairo texture */
      glEnable(GL_DEPTH_TEST);

      /* enable lighting */
      glEnable(GL_LIGHTING);

      /* use light0 as standard light - this gets disabled when we are drawing
         non-selected stuff so they are shown in the dark*/
      glEnable(GL_LIGHT0);

      /* multisampling (apparently not yet fully implemented in gtkglext but
       * enable by default for future implementations */
      glEnable(GL_MULTISAMPLE);

      /* initialise rotation matrix */
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      /* reset modelview */
      glLoadIdentity();
      /* now get new resultant no rotation matrix */
      glGetDoublev(GL_MODELVIEW_MATRIX, priv->rotation);
      /* undo back to previous */
      glPopMatrix();

      /* create hud texture */
      glGenTextures(1, &priv->hud_texture);

      /* create node glow texture */
      glGenTextures(1, &priv->active_node_glow_texture);

      /* load glow image from a png using cairo */
      glow_surf = cairo_image_surface_create_from_png(REPLAY_TEXTURES_DIR "/active-node-glow.png");
      if (cairo_surface_status(glow_surf) != CAIRO_STATUS_SUCCESS)
      {
        g_error("Error loading image %s: %s",
                REPLAY_TEXTURES_DIR "/node-glow.png",
                cairo_status_to_string(cairo_surface_status(glow_surf)));
      }

      glBindTexture(GL_TEXTURE_2D, priv->active_node_glow_texture);
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA,
                   cairo_image_surface_get_width(glow_surf),
                   cairo_image_surface_get_height(glow_surf),
                   0,
                   GL_BGRA_EXT,
                   GL_UNSIGNED_BYTE,
                   cairo_image_surface_get_data(glow_surf));

      /* cairo surface is not needed anymore as texture now contains the data
       * in its own format */
      cairo_surface_destroy(glow_surf);

      /* set magnify / minify to linear for best appearance */
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      /* create a quadric to generate display list contents */
      quadric = gluNewQuadric();

      /* create display list for nodes */
      gluQuadricDrawStyle(quadric, GLU_FILL);
      gluQuadricNormals(quadric, GLU_SMOOTH);

      priv->node_display_list = glGenLists(1);
      glNewList(priv->node_display_list, GL_COMPILE);
      gluSphere(quadric, 1.0, NUM_SPHERE_SLICES, NUM_SPHERE_STACKS);
      glEndList();

      /* create display list for self_edges */
      priv->self_edge_display_list = glGenLists(1);
      glNewList(priv->self_edge_display_list, GL_COMPILE);
      gluSphere(quadric, SELF_EDGE_RADIUS, NUM_SPHERE_SLICES, NUM_SPHERE_STACKS);
      glEndList();

      /* create display list for edge_bodys */
      priv->edge_body_display_list = glGenLists(1);
      glNewList(priv->edge_body_display_list, GL_COMPILE);
      gluCylinder(quadric, 2.0 * EDGE_WIDTH, 2.0 * EDGE_WIDTH, 1.0, NUM_SPHERE_SLICES, NUM_SPHERE_STACKS);
      glEndList();

      /* create display list for edge_heads */
      priv->edge_head_display_list = glGenLists(1);
      glNewList(priv->edge_head_display_list, GL_COMPILE);
      gluCylinder(quadric, EDGE_WIDTH * ARROW_MULTIPLIER / ARROW_HEIGHT_TO_WIDTH, 0.0, EDGE_WIDTH * ARROW_MULTIPLIER, NUM_SPHERE_SLICES, NUM_SPHERE_STACKS);
      /* change orientation for base of head */
      gluQuadricOrientation(quadric, GLU_INSIDE);
      gluDisk(quadric, 0.0, EDGE_WIDTH * ARROW_MULTIPLIER / ARROW_HEIGHT_TO_WIDTH, NUM_SPHERE_SLICES, 1.0);
      glEndList();

      /* create display list for group nodes */
      priv->group_display_list = glGenLists(1);
      glNewList(priv->group_display_list, GL_COMPILE);
      /* use wireframe style */
      gluQuadricDrawStyle(quadric, GLU_LINE);
      gluSphere(quadric, 1.0, NUM_SPHERE_SLICES, NUM_SPHERE_STACKS);
      glEndList();

      gluDeleteQuadric(quadric);

      gtk_widget_end_gl(widget, FALSE);
    }
  }
}

/*
 * Unrealize signal handler - unrealize the OpenGL window - release anything
 * such as textures and the like here in OpenGL
 */
static void replay_graph_view_unrealize(GtkWidget *widget)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(widget));

  priv = REPLAY_GRAPH_VIEW(widget)->priv;

  if (priv->gl_config)
  {
    /* free display lists */
    glDeleteLists(priv->edge_head_display_list, 1);
    glDeleteLists(priv->edge_body_display_list, 1);
    glDeleteLists(priv->self_edge_display_list, 1);
    glDeleteLists(priv->group_display_list, 1);
    glDeleteLists(priv->node_display_list, 1);
    free_cairo_context(REPLAY_GRAPH_VIEW(widget));
    glDeleteTextures(1, &(priv->active_node_glow_texture));
    glDeleteTextures(1, &(priv->hud_texture));
  }
  GTK_WIDGET_CLASS(replay_graph_view_parent_class)->unrealize(widget);
}

/*
 * Configure event signal handler - generally just used to resize OpenGL
 * viewport based on widget allocation
 */
static gboolean replay_graph_view_configure_event(GtkWidget *widget,
                                               GdkEventConfigure *configure)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);
  self = REPLAY_GRAPH_VIEW(widget);

  priv = self->priv;

  if (priv->gl_config)
  {
    if (gtk_widget_begin_gl(widget))
    {
      float width = (float)gtk_widget_get_allocated_width(widget);
      float height = (float)gtk_widget_get_allocated_height(widget);
      float ratio = width / height;

      /* set viewport to entire window */
      glViewport(0, 0, width, height);

      /* reset coord system before modifying */
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();

      /* set clipping volume */
      gluPerspective(priv->fovy, ratio, priv->z_near, priv->z_far);

      /* reset back to model view */
      glMatrixMode(GL_MODELVIEW);

      /* reestablish view */
      replay_graph_view_reset_gl_view(self);

      gtk_widget_end_gl(widget, FALSE);

      /* reallocate cairo stuff */
      allocate_cairo_context(self);

      ret = TRUE;
    }
  }
  return ret;
}


static void replay_graph_view_draw_nodes_or_edges(ReplayGraphView *self,
                                               gboolean edges)
{
  ReplayGraphViewPrivate *priv;
  unsigned int i;
  GHashTable *edges_table = NULL;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (edges)
  {
    edges_table = g_hash_table_new(g_direct_hash,
                                   g_direct_equal);
  }
  /* Draw visible nodes */
  for (i = 0; i < priv->visible_nodes->len; i++)
  {
    ReplayGraphViewNode *node = g_ptr_array_index(priv->visible_nodes, i);
    if (edges)
    {
      draw_node_edges(self, node, edges_table);
    }
    else
    {
      draw_node(self, node);
    }
  }
  if (edges)
  {
    g_hash_table_destroy(edges_table);
  }
  else
  {
    /* now draw active overlay's for nodes */
    GList *list;
    for (list = priv->active_nodes; list != NULL; list = list->next)
    {
      ReplayGraphViewNode *node = (ReplayGraphViewNode *)list->data;
      draw_active_node(self, node);
    }
  }
}

static gboolean
strv_find(const gchar **strv, const gchar *word)
{
  gboolean found = FALSE;
  guint i;
  for (i = 0; !found && strv[i] != NULL; i++) {
    found = !g_strcmp0(word, strv[i]) ? TRUE : FALSE;
  }
  return found;
}

static gchar *
props_to_string(GVariant *props, const gchar *header, const gchar *ignore[])
{
  GVariantIter iter;
  gchar *key;
  GVariant *value;
  gchar *text = NULL;

  g_variant_iter_init(&iter, props);
  while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
  {
    if (strv_find(ignore, key)) {
      continue;
    }
    /* start text with a copy of header if this is the first property we are
       listing */
    if (!text && header)
    {
      text = g_strdup(header);
    }
    text = replay_strconcat_and_free(text, g_strdup("\n"));
    text = replay_strconcat_and_free(text, g_strdup_printf("%s: ", key));
    text = replay_strconcat_and_free(text, g_variant_print(value, FALSE));
  }
  return text;
}

static void replay_graph_view_draw_all_markups(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node;
  unsigned int i;
  GVariant *props;
  GList *list;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* draw label for hovered edge */
  if (priv->hovered_edge)
  {
    ReplayGraphViewEdge *edge;
    gchar *props_text = NULL;
    gchar *text = NULL;
    gchar *label = NULL;
    float x, y, z;
    ReplayGraphViewNode *tail, *head;
    const gchar *ignore[] = { "label", "color", "weight", NULL };

    edge = priv->hovered_edge;

    /* don't just use edge->tail and edge->head since these may be
     * groups, instead may need to actually look-up nodes based on id's */
    tail = edge->tail;
    head = edge->head;
    if (tail->group)
    {
      tail = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                        edge->tail_name);
      g_assert(!tail->group);
    }
    if (head && head->group)
    {
      head = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                        edge->head_name);
      g_assert(!head->group);
    }

    props_get_string(edge->props, "label", &label);
    text = g_strdup_printf("%s", label ? label : edge->name);
    g_free(label);
    props = replay_property_store_get_props(edge->props);

    props_text = props_to_string(props, "\n\nProperties: \n-----------",
                                 ignore);

    if (props_text)
    {
      text = replay_strconcat_and_free(text, props_text);
    }

    if (edge->hover_info)
    {
      gchar *old_text = text;
      text = g_strjoin("\n\n", old_text, edge->hover_info, NULL);
      g_free(old_text);
    }

    /* edge hover info is positioned halfway along edge */
    x = ((edge->head ? edge->head->sx : 0.0) + edge->tail->sx) / 2;
    y = ((edge->head ? edge->head->sy : 0.0) + edge->tail->sy) / 2;
    z = ((edge->head ? edge->head->sz : 0.0) + edge->tail->sz) / 2;

    /* hover info is shown on hud */
    draw_markup_to_hud(self, text, x, y, z);
    g_free(text);
  }

  /* draw label for hovered node if not moving */
  if (!priv->moving && priv->hovered_node)
  {
    GtkTreeIter tree_iter;
    gchar *text = NULL;
    gchar *props_text = NULL;
    const gchar *ignore[] = { "label", "color", NULL };

    node = priv->hovered_node;
    text = g_strdup_printf("%s", node->label);

    /* show node properties */
    replay_node_tree_get_iter_for_id(priv->node_tree,
                                  &tree_iter,
                                  priv->hovered_node->name);
    props = replay_node_tree_get_props(priv->node_tree, &tree_iter);
    props_text = props_to_string(props, "\n\nProperties: \n-----------",
                                 ignore);

    if (props_text)
    {
      text = replay_strconcat_and_free(text, props_text);
    }

    /* add space if have activities */
    if (node->activities)
    {
      text = replay_strconcat_and_free(text, g_strdup("\n"
                                                   "\n"
                                                   "Activities\n"
                                                   "----------"));
    }
    for (list = g_list_first(node->activities);
         list != NULL;
         list = g_list_next(list))
    {
      NodeActivity *activity = (NodeActivity *)list->data;
      GVariantIter iter;
      gchar *key;
      GVariant *value;

      text = replay_strconcat_and_free(text,
                                    g_strdup_printf("\n"
                                                    "%s\n"
                                                    "---------",
                                                    activity->name));

      g_variant_iter_init(&iter, props);
      while (g_variant_iter_loop(&iter, "{sv}", &key, &value))
      {
        text = replay_strconcat_and_free(text, g_strdup("\n"));
        text = replay_strconcat_and_free(text, g_strdup_printf("%s: ", key));
        text = replay_strconcat_and_free(text, g_variant_print(value, FALSE));
      }
    }

    /* print children names as well */
    if (node->group && node->num_children > 0)
    {
      ReplayGraphViewNode *child;
      list = g_list_first(node->children);
      child = (ReplayGraphViewNode *)list->data;

      text = replay_strconcat_and_free(text,
                                    g_strdup("\n\nChildren: "));
      for (list = g_list_first(node->children);
           list != NULL;
           list = g_list_next(list))
      {
        child = (ReplayGraphViewNode *)list->data;
        text = replay_strconcat_and_free(text,
                                      g_strdup_printf("\n"
                                                      "          %s%s",
                                                      (child->label ?
                                                       child->label :
                                                       ""),
                                                      (child->group ?
                                                       " (GROUP)" :
                                                       "")));
      }
    }
    /* hovered info is shown on hud */
    draw_markup_to_hud(self, text, node->sx, node->sy, node->sz);
    g_free(text);
  }

  /* now label all events - if node is picked or selected this takes
   * precedence */
  /* do labelled nodes */
  for (list = priv->labelled_nodes;
       list != NULL;
       list = list->next)
  {
    node = (ReplayGraphViewNode *)list->data;
    g_assert(node->event_label);

    /* make sure node is not hovered and is visible */
    if (node_is_in_array(node, priv->visible_nodes) &&
        node != priv->hovered_node)
    {
      float dx, dy, dz;
      /* offset label by radius of node along z axis (transformed by
       * current rotation) ie. [0, 0, r] x priv->rotation */
      dx = priv->rotation[2] * node->radius;
      dy = priv->rotation[6] * node->radius;
      dz = priv->rotation[10] * node->radius;
      draw_markup_gl(self,
                     node->event_label,
                     node->sx + dx,
                     node->sy + dy,
                     node->sz + dz);
    }
  }

  /* do labelled edges */
  for (list = priv->labelled_edges;
       list != NULL;
       list = list->next)
  {
    ReplayGraphViewEdge *edge = (ReplayGraphViewEdge *)list->data;
    float x, y, z;
    g_assert(edge->event_label);

    /* only label if node and head are visible */
    if (node_is_in_array(edge->tail, priv->visible_nodes) &&
        (!edge->head ||
         node_is_in_array(edge->head, priv->visible_nodes)))
    {
      /* edge event label is at position along arrow */
      x = (((edge->head ? edge->head->sx : 0.0) * edge->position) + (edge->tail->sx * (1.0 - edge->position)));
      y = (((edge->head ? edge->head->sy : 0.0) * edge->position) + (edge->tail->sy * (1.0 - edge->position)));
      z = (((edge->head ? edge->head->sz : 0.0) * edge->position) + (edge->tail->sz * (1.0 - edge->position)));
      draw_markup_gl(self, edge->event_label, x, y, z);
    }
  }

  /* now do standard label for nodes and edges if needed - ie we are in a label
   * mode which requires them to be labelled or we have some picked / selected
   * nodes */
  if (priv->label_nodes ||
      priv->label_edges ||
      g_hash_table_size(priv->picked_nodes) > 0 ||
      g_hash_table_size(priv->selected_nodes) > 0)
  {
    /* loop through all visible nodes */
    for (i = 0; i < priv->visible_nodes->len; i++)
    {
      node = g_ptr_array_index(priv->visible_nodes, i);

      /* draw node label if we are in correct mode OR it is selected or
       * picked, and it is not hovered node and it doesn't have an event label
       * since in both those cases we have already drawn a label for it */
      if (((priv->label_nodes ||
           g_hash_table_lookup(priv->picked_nodes, node->name) == node ||
           g_hash_table_lookup(priv->selected_nodes, node->name) == node) &&
          ((node != priv->hovered_node || priv->moving) &&
           !node->event_label)) &&
          node->label)
      {
        float dx, dy, dz;

        /* offset label by radius of node along z axis (transformed by
         * current rotation) ie. [0, 0, r] x priv->rotation */
        dx = priv->rotation[2] * node->radius;
        dy = priv->rotation[6] * node->radius;
        dz = priv->rotation[10] * node->radius;
        draw_markup_gl(self,
                       node->label,
                       node->sx + dx,
                       node->sy + dy,
                       node->sz + dz);
      }

      if (priv->label_edges)
      {
        guint j;
        for (j = 0; j < node->edges->len; j++)
        {
          guint k;
          GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, j);
          for (k = 0; k < edges->len; k++)
          {
            ReplayGraphViewEdge *edge = (ReplayGraphViewEdge *)
              g_ptr_array_index(edges, k);
            gchar *label = NULL;
            float x, y, z;

            props_get_string(edge->props, "label", &label);
            if (label)
            {
              /* edge label is half way along edge */
              x = ((edge->head ? edge->head->sx : 0.0) + edge->tail->sx) / 2;
              y = ((edge->head ? edge->head->sy : 0.0) + edge->tail->sy) / 2;
              z = ((edge->head ? edge->head->sz : 0.0) + edge->tail->sz) / 2;
              draw_markup_gl(self, label, x, y, z);
              g_free(label);
            }
          }
        }
      }
    }
  }
}

static void replay_graph_view_draw_hud(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  gdouble width, height;
  GtkWidget *widget;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  widget = GTK_WIDGET(self);
  priv = self->priv;

  /* only draw if we've got some data */
  if (priv->event_store && priv->node_tree)
  {
    GtkStyleContext *style;
    const PangoFontDescription *desc;
    int text_width, text_height;
    gchar *text, *big_text;
    gint iter_index = 0;

    style = gtk_widget_get_style_context(widget);
    desc = gtk_style_context_get_font(style,
                                      gtk_widget_get_state_flags(widget));
    width = (gdouble)gtk_widget_get_allocated_width(GTK_WIDGET(self));
    height = (gdouble)gtk_widget_get_allocated_height(GTK_WIDGET(self));

    cairo_save(priv->cr);

    cairo_set_line_width(priv->cr, 0.5);

    /* display select rectangle */
    if (priv->selecting)
    {
      GdkRGBA select_color;
      gtk_style_context_get_background_color(style,
                                             GTK_STATE_FLAG_SELECTED,
                                             &select_color);
      /* fill using half lightness of select color (does ALPHA / 2.0) */
      cairo_set_source_rgba(priv->cr,
                            select_color.red,
                            select_color.green,
                            select_color.blue,
                            0.5);
      cairo_rectangle(priv->cr,
                      MIN(priv->x1, priv->x2),
                      MIN(priv->y1, priv->y2),
                      fabs(priv->x2 - priv->x1),
                      fabs(priv->y2 - priv->y1));

      cairo_fill_preserve(priv->cr);
      cairo_set_source_rgb(priv->cr,
                           select_color.red,
                           select_color.green,
                           select_color.blue);
      cairo_stroke(priv->cr);
    }

    /* show any recent drawing error where it can't be missed */
    if (priv->gl_error != GL_NO_ERROR)
    {
      PangoAlignment align;
      text = g_markup_printf_escaped("<big><b>OpenGL Error: #%d %s\n\n"
                                     "Please report this error to %s</b></big>",
                                     priv->gl_error, gluErrorString(priv->gl_error),
                                     PACKAGE_BUGREPORT);
      pango_layout_set_markup(priv->layout, text, -1);
      /* set to centered alignment */
      align = pango_layout_get_alignment(priv->layout);
      pango_layout_set_alignment(priv->layout, PANGO_ALIGN_CENTER);
      g_free(text);
      pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);
      cairo_move_to(priv->cr,
                    (width - text_width) / 2,
                    (height - text_height) / 2);
      cairo_set_source_rgb(priv->cr, 1.0, 0.0, 0.0);
      pango_cairo_show_layout(priv->cr, priv->layout);
      pango_layout_set_alignment(priv->layout, align);
    }

    if (priv->debug)
    {
      text = g_strdup_printf("Render time: %fms\n"
                             "FPS        : %f\n"
                             "z_near     : %f\n"
                             "z_far      : %f\n"
                             "min_cam_d  : %f\n"
                             "max_cam_d  : %f\n"
                             "max_node_d : %f\n"
                             "fovy       : %f\n"
                             "x          : %f\n"
                             "y          : %f\n"
                             "z          : %f\n"
                             "# nodes    : %d\n"
                             "# edges    : %d\n"
                             "2D         : %s\n",
                             priv->render_ms, priv->fps,
                             priv->z_near, priv->z_far,
                             priv->min_camera_distance,
                             priv->max_camera_distance,
                             priv->max_node_distance, priv->fovy,
                             priv->x, priv->y, priv->z,
                             g_hash_table_size(priv->node_table),
                             g_hash_table_size(priv->edge_table),
                             priv->two_dimensional ? "TRUE" : "FALSE");
      pango_layout_set_markup(priv->layout, text, -1);
      g_free(text);
      cairo_move_to(priv->cr, 0.0, 0.0);
      cairo_set_source_rgba(priv->cr, 1.0, 1.0, 1.0, 1.0);
      pango_cairo_show_layout(priv->cr, priv->layout);
    }

    if (priv->show_hud)
    {
      if (!priv->debug)
      {
        pango_layout_set_font_description(priv->layout, desc);
        pango_layout_set_markup(priv->layout, (priv->two_dimensional ?
                                               "<big>2D</big>" :
                                               "<big>3D</big>"),
                                -1);
        pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);
        cairo_move_to(priv->cr, 1.0, 1.0);
        cairo_set_source_rgb(priv->cr, 1.0, 1.0, 1.0);
        pango_cairo_show_layout(priv->cr, priv->layout);
      }

      /* display rendering fps in red, bold */
      text = g_strdup_printf("<span weight='bold'>%4.1f FPS</span>",
                             priv->fps);
      pango_layout_set_markup(priv->layout, text, -1);
      g_free(text);
      pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);
      cairo_move_to(priv->cr, width - text_width,
                    height - text_height);

      /* show fps in red but set alpha channel as 0 when we match desired frame
       * rate, and increase opacity to 100% when we are at half this to indicate
       * that it's time to get a faster GPU / we need to optimise our drawing
       * routines */
      cairo_set_source_rgba(priv->cr, 1.0, 0.0, 0.0,
                            CLAMP(2 * (1.0 - (priv->fps / priv->max_fps)),
                                  0.0, 1.0));
      pango_cairo_show_layout(priv->cr, priv->layout);

      /* display wall time */
      if (priv->iter)
      {
        ReplayEvent *event = replay_event_store_get_event(priv->event_store,
                                                          priv->iter);
        gint64 timestamp = replay_event_get_timestamp(event);

        if (!priv->absolute_time) {
                timestamp -= replay_event_store_get_start_timestamp(priv->event_store);
        }
        text = replay_timestamp_to_string(timestamp,
                                          priv->absolute_time);
        g_object_unref(event);
        big_text = g_strdup_printf("<big>%s</big>", text);
        g_free(text);
        text = big_text;
        pango_layout_set_font_description(priv->layout, desc);
        pango_layout_set_markup(priv->layout, text, -1);
        g_free(text);
        pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);
        cairo_move_to(priv->cr, width - text_width, 0.0);
        cairo_set_source_rgb(priv->cr, 1.0, 1.0, 1.0);
        pango_cairo_show_layout(priv->cr, priv->layout);
      }

      /* get progress along event list */
      if (priv->iter)
      {
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                                    priv->iter);
        iter_index = gtk_tree_path_get_indices(path)[0];
        gtk_tree_path_free(path);
      }

      /* display some stats */
      text = g_strdup_printf("%d of %d nodes visible in current state [%d / %d events]",
                             priv->visible_nodes->len,
                             priv->visible_nodes->len + priv->hidden_nodes->len,
                             iter_index + 1,
                             gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->event_store), NULL));
      pango_layout_set_markup(priv->layout, text, -1);
      g_free(text);

      pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);

      cairo_move_to(priv->cr, 0.0, height - text_height);
      cairo_set_source_rgb(priv->cr, 1.0, 1.0, 1.0);
      pango_cairo_show_layout(priv->cr, priv->layout);
    }

    /* if processing events overlay a message to this effect on the view */
    if (priv->processing_id)
    {
      GtkTreePath *path;
      gint *start, *end, *i;
      gdouble progress[2];

      /* darken overall background */
      cairo_rectangle(priv->cr, 0, 0, width, height);
      cairo_set_source_rgba(priv->cr, 0.0, 0.0, 0.0, 0.5);
      cairo_fill(priv->cr);

      /* display processing message */
      text = g_strdup_printf("<big><b>%s</b></big>",
                             _("Processing events…"));
      pango_layout_set_markup(priv->layout, text, -1);
      g_free(text);

      pango_layout_get_pixel_size(priv->layout, &text_width, &text_height);

      cairo_move_to(priv->cr,
                    (width - text_width) / 2,
                    (height - text_height) / 2);
      cairo_set_source_rgb(priv->cr, 1.0, 1.0, 1.0);
      pango_cairo_show_layout(priv->cr, priv->layout);

      /* draw a two-level progress bar */
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                     priv->iter);
      start = gtk_tree_path_get_indices(priv->processing_start);
      end = gtk_tree_path_get_indices(priv->processing_end);
      i = gtk_tree_path_get_indices(path);

      progress[0] = (end[0] == start[0] && end[0] == i[0] ?  1.0 :
                     (fabs((gdouble)(i[0] - start[0])) /
                      fabs((gdouble)(end[0] - start[0]))));
      progress[1] = (end[1] == start[1] && end[1] == i[1] ?  1.0 :
                     (fabs((gdouble)(i[1] - start[1])) /
                      fabs((gdouble)(end[1] - start[1]))));
      /* make progress bar 1/5th width of view and 20 pixels high */
      cairo_rectangle(priv->cr,
                      0.40 * width,
                      (height - text_height) / 2 + text_height + 10.0,
                      progress[0] * 0.2 * width,
                      10.0);
      cairo_fill(priv->cr);
      cairo_rectangle(priv->cr,
                      0.40 * width,
                      (height - text_height) / 2 + text_height + 20.0,
                      progress[1] * 0.2 * width,
                      10.0);
      cairo_fill(priv->cr);
      cairo_rectangle(priv->cr,
                      0.40 * width,
                      (height - text_height) / 2 + text_height + 10.0,
                      0.2 * width,
                      20.0);
      cairo_stroke_preserve(priv->cr);
      cairo_set_source_rgba(priv->cr, 1.0, 1.0, 1.0, 0.1);
      cairo_fill(priv->cr);
      gtk_tree_path_free(path);
    }
    cairo_restore(priv->cr);
  }
}

static void replay_graph_view_draw_all(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node;
  unsigned int i;
  GHashTable *edges_table;
  GList *list;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  edges_table = g_hash_table_new(g_direct_hash,
                                 g_direct_equal);

  for (i = 0; i < priv->visible_nodes->len; i++)
  {
    node = g_ptr_array_index(priv->visible_nodes, i);
    draw_node_edges(self, node, edges_table);
    draw_node(self, node);
  }
  g_hash_table_destroy(edges_table);

  /* now draw active overlay's for nodes */
  for (list = priv->active_nodes; list != NULL; list = list->next)
  {
    node = (ReplayGraphViewNode *)list->data;
    if (!node_is_in_array(node, priv->hidden_nodes))
    {
      draw_active_node(self, node);
      draw_active_node_glows(self, node);
    }
  }

  replay_graph_view_draw_all_markups(self);
  replay_graph_view_draw_hud(self);
}


/* draw event signal handler - do a full draw of the entire scene */
static gboolean replay_graph_view_draw(GtkWidget *widget,
                                    cairo_t *cr)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean handled = FALSE;
  gint width, height;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);
  self = REPLAY_GRAPH_VIEW(widget);

  priv = self->priv;
  width = gtk_widget_get_allocated_width(GTK_WIDGET(self));
  height = gtk_widget_get_allocated_height(GTK_WIDGET(self));

  /* keep moving average of how many fps we are achieving */
  if (!priv->fps_timer)
  {
    /* create timer and reset fps data - this will start timer running too */
    priv->fps_timer = g_timer_new();
    priv->fps = 0.0;
  }
  else
  {
    gdouble fps = 1.0 / g_timer_elapsed(priv->fps_timer, NULL);
    priv->fps = ((0.8 * priv->fps) + (0.2 * fps));
    /* restart timing till next render timeout */
    g_timer_start(priv->fps_timer);
  }

  if (priv->gl_config)
  {
    if (gtk_widget_begin_gl(widget))
    {
      cairo_operator_t op;
      double x[2], y[2];
      GdkGLDrawable *gl_drawable;

      if (priv->debug)
      {
        if (priv->debug_timer == NULL)
        {
          priv->debug_timer = g_timer_new();
        }
        g_timer_start(priv->debug_timer);
      }

      /* reset viewing volume if needed */
      if (priv->reset_viewing_volume)
      {
        reset_viewing_volume(self);
        /* make sure z is valid */
        priv->z = CLAMP(priv->z,
                        priv->min_camera_distance,
                        priv->max_camera_distance);
      }
      priv->reset_viewing_volume = FALSE;

      glClearDepth(1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      replay_graph_view_reset_gl_view(self);

#if 0
      /* draw axes at point of interest - can be useful for scene debugging  */
      glDisable(GL_LIGHTING);
      glPushMatrix();
      glColor3f(1.0, 1.0, 1.0);
      gdk_gl_draw_cube(FALSE, 2*priv->max_node_distance);
      gdk_gl_draw_sphere(FALSE,
                         sqrtf(3 * (priv->max_node_distance * priv->max_node_distance)),
                         20, 20);
      glTranslatef(*(priv->pIx), *(priv->pIy), *(priv->pIz));
      glBegin(GL_LINES);
      glVertex3f(0, 0, 0);
      glVertex3f(priv->z / 2, 0, 0);
      glVertex3f(0, 0, 0);
      glVertex3f(0, priv->z / 2, 0);
      glVertex3f(0, 0, 0);
      glVertex3f(0, 0, priv->z / 2);
      glEnd();
      glEnable(GL_LIGHTING);
      glPopMatrix();
#endif
      /* use scissor test to only draw newly exposed area */
      glPushAttrib(GL_SCISSOR_BIT);
      glEnable(GL_SCISSOR_TEST);
      cairo_clip_extents(cr, &x[0], &y[0], &x[1], &y[1]);
      glScissor(x[0], y[0], x[1] - x[0], y[1] - y[0]);

      /* clear surface */
      op = cairo_get_operator(priv->cr);
      cairo_set_operator(priv->cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba(priv->cr, 0.0f, 0.0f, 0.0f, 0.0f);
      cairo_paint(priv->cr);
      cairo_set_operator(priv->cr, op);

      /* prepare cairo surface for rendering - set double buffered */
      cairo_push_group(priv->cr);

      replay_graph_view_draw_all(self);

      /* draw the buffered contents */
      cairo_pop_group_to_source(priv->cr);
      cairo_paint(priv->cr);

      glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT |
                   GL_TEXTURE_BIT);

      /* overlay 2d hud over 3d scene */
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      /* make depth buffer read only when drawing transparent nodes (such as
       * the hud which has a transparent background) */
      glDepthMask(GL_FALSE);

      /* disable lighting for 2d */
      glDisable(GL_LIGHTING);

      /* set up new projection matrix space for mapping the cairo surface */
      glMatrixMode(GL_PROJECTION);
      /* save current projection matrix */
      glPushMatrix();

      /* reset projection matrix */
      glLoadIdentity();
      glOrtho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

      /* now switch back to modelview to draw scene */
      glMatrixMode(GL_MODELVIEW);

      /* save current modelview matrix */
      glPushMatrix();

      /* reset modelview matrix */
      glLoadIdentity();

      /* replace underlying fragment colors with those from texture - and since
       * we are blending with GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA will get
       * expected overlayed effect */
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

      /* map texture in scene */
      glEnable(GL_TEXTURE_RECTANGLE_ARB);
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, priv->hud_texture);
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                   0,
                   GL_RGBA,
                   width,
                   height,
                   0,
                   GL_BGRA_EXT,
                   GL_UNSIGNED_BYTE,
                   priv->surface_data);

      glBegin(GL_QUADS);

      glTexCoord2d(0.0f, 0.0f);
      glVertex2f(0.0f, 1.0f);

      glTexCoord2f(0.0f, (GLfloat)height);
      glVertex2f(0.0f, 0.0f);

      glTexCoord2f((GLfloat)width, (GLfloat)height);
      glVertex2f(1.0f, 0.0f);

      glTexCoord2f((GLfloat)width, 0.0f);
      glVertex2f(1.0f, 1.0f);

      glEnd();

      /* switch to projection mode to turn it off */
      glMatrixMode(GL_PROJECTION);
      /* pop changes to projection matrix for hud drawing() */
      glPopMatrix();
      /* switch back to 3d to continue 3d drawing stuff */
      glMatrixMode(GL_MODELVIEW);
      /* pop changes to modelview matrix */
      glPopMatrix();

      /* reenable lighting and blending attribs */
      glPopAttrib();

      /* turn off scissor test */
      glPopAttrib();

      /* Swap buffers manually so we can check for errors before ending the gl
       * code */
      gl_drawable = GDK_GL_DRAWABLE(gtk_widget_get_gl_window(widget));
      if (gdk_gl_drawable_is_double_buffered(gl_drawable))
      {
        gdk_gl_drawable_swap_buffers(gl_drawable);
      }
      else
      {
        glFlush();
      }

      /* check current opengl error status so can be reported later */
      priv->gl_error = glGetError();

      gtk_widget_end_gl(widget, FALSE);

      if (priv->debug)
      {
        gdouble ms = g_timer_elapsed(priv->debug_timer, NULL) * 1000.0;
        priv->render_ms = ((0.8 * priv->render_ms) + (0.2 * ms));
      }

      handled = TRUE;
    }
  }
  if (!handled)
  {
    /* an OpenGL error - tell user */
    PangoLayout *layout;
    gint text_width, text_height;
    GtkStyleContext *style;
    GtkStateFlags flags;
    GdkRGBA bg_color, text_color;
    const PangoFontDescription *desc;

    style = gtk_widget_get_style_context(widget);
    flags = gtk_widget_get_state_flags(widget);
    gtk_style_context_get_background_color(style, flags, &bg_color);
    gtk_style_context_get_color(style, flags, &text_color);
    desc = gtk_style_context_get_font(style, flags);

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_set_source_rgb(cr,
                         bg_color.red,
                         bg_color.green,
                         bg_color.blue);
    cairo_fill(cr);

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout,
                          "ERROR: OpenGL support does not appear to be enabled.\n"
                          "Unable to draw Node Edge State Graph.", -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    cairo_move_to(cr,
                  (width / 2) - (text_width / 2),
                  (height / 2) - (text_height / 2));
    cairo_set_source_rgb(cr,
                         text_color.red,
                         text_color.green,
                         text_color.blue);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
  }
  return handled;
}

static gboolean replay_graph_view_run_animation(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), ret);
  priv = self->priv;

  /* only add animation timeout if we have gl edge and we have nodes to
   * move and we are not already running, and we are not selecting  */
  if (priv->gl_config != NULL && priv->visible_nodes->len > 0 &&
      priv->render_timeout_id == 0 &&
      priv->layout_source == NULL &&
      !priv->selecting)
  {
    /* use default idle priority so we don't override normal GTK / GLib events
     * etc when rendering - this will in turn rate limit when we can't keep up
     * at priv->max_fps */
    priv->render_timeout_id = gdk_threads_add_timeout_full(G_PRIORITY_LOW,
                                                           1000.0 / priv->max_fps,
                                                           replay_graph_view_render_ratelimit,
                                                           self,
                                                           NULL);
    priv->layout_source = g_timeout_source_new(LAYOUT_INTERVAL_MS);
    g_source_set_callback(priv->layout_source,
                          (GSourceFunc)replay_graph_view_update_node_positions,
                          self,
                          NULL);
    g_source_attach(priv->layout_source, priv->layout_context);

    ret = TRUE;
  }
  /* make a single call to render scene regardless of if this started animation
   * or not */
  replay_graph_view_render_ratelimit(self);
  return ret;
}

static gboolean replay_graph_view_stop_animation(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), ret);
  priv = self->priv;

  if (priv->render_timeout_id != 0)
  {
    g_assert(priv->layout_source != NULL);
    g_source_destroy(priv->layout_source);
    g_source_unref(priv->layout_source);
    priv->layout_source = NULL;

    g_source_remove(priv->render_timeout_id);
    priv->render_timeout_id = 0;

    /* force one more render so we show most up-to-date state */
    replay_graph_view_render_graph(self);

    /* make sure above call to render_graph() hasn't pushed fps over priv->max_fps
     * since we never want to appear to exceed priv->max_fps */
    priv->fps = MIN(priv->max_fps, priv->fps);
    ret = TRUE;
  }
  return ret;
}


static void add_activity(ReplayGraphViewNode *node, NodeActivity *activity)
{
  while (node != NULL)
  {
    node->activities = g_list_append(node->activities, activity);
    node = node->parent;
  }
}

static void remove_activity(ReplayGraphViewNode *node, NodeActivity *activity)
{
  while (node != NULL)
  {
    node->activities = g_list_remove(node->activities, activity);
    node = node->parent;
  }
}

static gint node_activity_cmp(NodeActivity *act1,
                              const gchar *name)
{
  return g_strcmp0(act1->name, name);
}

static void remove_activity_node_with_name(ReplayGraphView *self,
                                           const gchar *node_name,
                                           const gchar *name)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node;

  priv = self->priv;

  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, node_name);
  if (node != NULL)
  {
    GList *list = g_list_find_custom(node->activities,
                                     name,
                                     (GCompareFunc)node_activity_cmp);

    if (list)
    {
      NodeActivity *activity = (NodeActivity *)list->data;
      remove_activity(node, activity);
      g_object_unref(activity->props);

      g_slice_free(NodeActivity, activity);

      /* now see if we need to remove canonical node from active nodes list */
      node = get_canonical_node(node);
      if (node->activities == NULL)
      {
        priv->active_nodes = g_list_remove(priv->active_nodes, node);
      }
    }
    else
    {
      g_warning("Couldn't find activty %s on node %s",
                name, node_name);
    }
  }
}

static void add_activity_node_with_name(ReplayGraphView *self,
                                        const gchar *node_name,
                                        const gchar *name,
                                        GVariant *props)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node;

  priv = self->priv;

  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, node_name);
  if (node != NULL)
  {
    NodeActivity *activity = g_slice_new0(NodeActivity);
    activity->name = g_strdup(name);
    activity->props = replay_property_store_new();
    replay_property_store_push(activity->props, props);
    add_activity(node, activity);

    /* add canonical node to active nodes list if we now have only 1 activity
     * in its list */
    node = get_canonical_node(node);
    if (g_list_length(node->activities) == 1)
    {
      priv->active_nodes = g_list_prepend(priv->active_nodes, node);
    }
  }
}

static GPtrArray *find_edges_array(GPtrArray *array,
                                   ReplayGraphViewNode *a,
                                   ReplayGraphViewNode *b)
{
  guint i;
  GPtrArray *edges = NULL;

  for (i = 0; i < array->len; i++)
  {
    ReplayGraphViewEdge *edge;
    edges = (GPtrArray *)g_ptr_array_index(array, i);
    g_assert(edges);
    /* should only have arrays with edges in them */
    g_assert(edges->len > 0);
    edge = (ReplayGraphViewEdge *)g_ptr_array_index(edges, 0);
    if ((edge->tail == a && edge->head == b) ||
        (edge->head == a && edge->tail == b))
    {
      break;
    }
    edges = NULL;
  }
  return edges;
}

static gboolean edge_is_in_array(ReplayGraphViewEdge *edge,
                                 GPtrArray *array)
{
  return (edge->index < array->len ?
          ((ReplayGraphViewEdge *)g_ptr_array_index(array, edge->index) ==
           edge) :
          FALSE);
}

static void add_edge_to_array(ReplayGraphViewEdge *edge,
                              GPtrArray *array)
{
  g_assert(!edge_is_in_array(edge, array));
  edge->index = array->len;
  g_ptr_array_add(array, edge);
  g_assert(edge_is_in_array(edge, array));
}

static void remove_edge_from_array(ReplayGraphViewEdge *edge,
                                   GPtrArray *array)
{
  guint i;
  g_assert(edge_is_in_array(edge, array));
  g_ptr_array_remove_index(array, edge->index);
  /* now update indexes of other remaining elements in array since we have
   * removed the element before them in the array */
  for (i = edge->index; i < array->len; i++)
  {
    edge = (ReplayGraphViewEdge *)g_ptr_array_index(array, i);
    edge->index--;
    g_assert(edge->index == i);
  }
}

static gdouble calculate_edges_weight(ReplayGraphViewNode *node)
{
  gdouble edges_weight = 0.0f;
  guint i;

  for (i = 0; i < node->edges->len; i++)
  {
    guint j;
    GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, i);

    for (j = 0; j < edges->len; j++)
    {
      ReplayGraphViewEdge *edge = (ReplayGraphViewEdge *)
        g_ptr_array_index(edges, j);

      /* if the node is a group, then it contains all its children's edges -
       * but edges which are intra-children (and hence would normally have 2
       * entries (one for each of the pair of nodes) get aggregated as single
       * entries - so we need to count them twice */
      if (node->group)
      {
        /* count twice if tail and head are node BUT names are different */
        if (edge->tail == edge->head &&
            g_ascii_strcasecmp(edge->tail_name, edge->head_name))
        {
          edges_weight += edge_get_weight(edge);
        }
      }
      edges_weight += edge_get_weight(edge);
    }
  }
  return edges_weight;
}

static void node_update_mass(ReplayGraphViewNode *node)
{
  /* update and aggregate mass of all children */
  GList *list;

  /* reset mass then add on mass of all children, plus all edges */
  node->mass = NODE_MASS;

  for (list = node->children; list != NULL; list = g_list_next(list))
  {
    ReplayGraphViewNode *child = (ReplayGraphViewNode *)list->data;
    node_update_mass(child);
    node->mass += child->mass;
  }
  /* node gets half the mass of its edges */
  node->mass += calculate_edges_weight(node) / 2.0;
  /* now calculate radius */
  node->radius = 2*powf(0.75*M_1_PI*node->mass, 1.0/3.0);
}

static void node_add_edge(ReplayGraphViewNode *node,
                          ReplayGraphViewEdge *edge)
{
  GPtrArray *edges;
  g_assert(edge->tail == node);

  /* create edges array if needed */
  if ((edges = find_edges_array(node->edges, node, edge->head)) == NULL)
  {
    edges = g_ptr_array_new();
    g_ptr_array_add(node->edges, edges);
    /* also insert array into head's edges if is not same node */
    if (edge->head && edge->head != node)
    {
      g_assert(find_edges_array(edge->head->edges, node, edge->head) == NULL);
      g_ptr_array_add(edge->head->edges, edges);
    }
  }
  /* don't add 2 lots of same edge to an array */
  if (!edge_is_in_array(edge, edges))
  {
    add_edge_to_array(edge, edges);

    /* update mass / size of nodes */
    node_update_mass(edge->tail);
    if (edge->head && edge->head != edge->tail)
    {
      node_update_mass(edge->head);
    }
  }
}

static void node_remove_edge(ReplayGraphViewNode *node,
                             ReplayGraphViewEdge *edge)
{
  GPtrArray *edges;
  gboolean ret;

  g_assert(edge->tail == node);

  /* find edges array */
  edges = find_edges_array(node->edges, node, edge->head);
  g_assert(edges != NULL);
  /* remove the edge */
  remove_edge_from_array(edge, edges);
  if (edges->len == 0)
  {
    /* delete edges array when none left */
    ret = g_ptr_array_remove(node->edges, edges);
    g_assert(ret);
    /* remove array from edge head as well - unless head is this same node */
    if (edge->head && edge->head != node)
    {
      ret = g_ptr_array_remove(edge->head->edges, edges);
      g_assert(ret);
    }
    g_ptr_array_free(edges, TRUE);
  }
  /* update mass / size of nodes */
  node_update_mass(edge->tail);
  if (edge->head && edge->head != edge->tail)
  {
    node_update_mass(edge->head);
  }
}

static void add_node_to_array(ReplayGraphViewNode *node,
                              GPtrArray *array)
{
  g_assert(!node_is_in_array(node, array));
  node->index = array->len;
  g_ptr_array_add(array, node);
  g_assert(node_is_in_array(node, array));
}

static void remove_node_from_array(ReplayGraphViewNode *node,
                                   GPtrArray *array)
{
  guint i;
  g_assert(node_is_in_array(node, array));
  g_ptr_array_remove_index(array, node->index);
  /* now update indexes of other remaining elements in array since we have
   * removed the element before them in the array */
  for (i = node->index; i < array->len; i++)
  {
    node = (ReplayGraphViewNode *)g_ptr_array_index(array, i);
    node->index--;
    g_assert(node->index == i);
  }
}

static void add_node_to_visible_array(ReplayGraphView *self,
                                      ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv = self->priv;

  add_node_to_array(node, priv->visible_nodes);
}

static void remove_node_from_visible_array(ReplayGraphView *self,
                                           ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv = self->priv;

  remove_node_from_array(node, priv->visible_nodes);
}

static void add_node_to_hidden_array(ReplayGraphView *self,
                                     ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv = self->priv;

  add_node_to_array(node, priv->hidden_nodes);
}

static void remove_node_from_hidden_array(ReplayGraphView *self,
                                          ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv = self->priv;

  remove_node_from_array(node, priv->hidden_nodes);
}

static void free_edge(ReplayGraphView *self,
                      ReplayGraphViewEdge *edge)
{
  ReplayGraphViewPrivate *priv = self->priv;

  /* remove edge if is in dead edges list */
  priv->dead_edges = g_list_remove(priv->dead_edges, edge);
  if (edge->event_label)
  {
    g_free(edge->event_label);
    edge->event_label = NULL;
    priv->labelled_edges = g_list_remove(priv->labelled_edges, edge);
  }
  if (edge->hover_info)
  {
    g_free(edge->hover_info);
    edge->hover_info = NULL;
  }
  g_hash_table_remove(priv->edge_table, edge->name);
  g_object_unref(edge->props);
  g_free(edge->name);
  g_free(edge->tail_name);
  g_free(edge->head_name);
  g_free(edge);
}

static gboolean node_has_name(ReplayGraphViewNode *node,
                              const gchar *name)
{
  gboolean has_name = FALSE;

  /* try node itself, otherwise try all of its children */
  has_name = (g_ascii_strcasecmp(node->name, name) == 0);

  if (!has_name)
  {
    GList *list;
    for (list = node->children;
         list != NULL && !has_name;
         list = g_list_next(list))
    {
      ReplayGraphViewNode *child = (ReplayGraphViewNode *)list->data;
      has_name = node_has_name(child, name);
    }
  }
  return has_name;
}


/*
 * Transfers edges from node to new - pass new as NULL to free edges - if
 * match_new is true, then only those edges which actually belong to
 * (ie. have tail_name) or point to (have head_name) a child of new
 * will be transferred
 */
static void transfer_edges(ReplayGraphView *self,
                           ReplayGraphViewNode *node,
                           ReplayGraphViewNode *new,
                           gboolean match_new)
{
  ReplayGraphViewPrivate *priv;
  guint i;

  g_assert(!match_new || new != NULL);

  priv = self->priv;

  for (i = 0; i < node->edges->len; i++)
  {
    guint j;
    GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, i);

    for (j = 0; j < edges->len; j++)
    {
      /* move each edge which has us as tail */
      ReplayGraphViewEdge *edge =
        (ReplayGraphViewEdge *)g_ptr_array_index(edges, j);

      if (edge->tail == node)
      {
        /* if match_children, then only transfer edges which have id of
         * tail_name (ie are really owned by) new otherwise do all */
        if (!match_new || node_has_name(new, edge->tail_name))
        {
          gboolean freed_array = FALSE;

          if (edges->len == 1)
          {
            freed_array = TRUE;
          }
          node_remove_edge(node, edge);

          /* keep same index since we just removed edge */
          j--;

          /* give edge to new tail or free it if no new tail */
          if (new)
          {
            edge->tail = new;
            node_add_edge(new, edge);
          }
          else
          {
            /* free edge */
            free_edge(self, edge);
          }
          if (freed_array)
          {
            /* keep same index since we just removed an array from the overall
             * edges array for this node */
            i--;
            break;
          }
          /* we have handled edge so don't try handling as head - if head is also
           * this same node, we will have re-added it to the end of the end of
           * the edges array and so should process it again later */
          continue;
        }
      }

      if (edge->head == node)
      {
        /* if match_new, then only transfer edges which have head_name of
         * new->id, otherwise do all */
        if (!match_new || node_has_name(new, edge->head_name))
        {
          gboolean freed_array = FALSE;

          if (edges->len == 1)
          {
            freed_array = TRUE;
          }
          node_remove_edge(edge->tail, edge);
          /* just removed edge - keep same index */
          j--;

          /* point edge head to new head */
          edge->head = new;
          node_add_edge(edge->tail, edge);

          if (edge->head == NULL)
          {
            /* also add to dead edges list */
            priv->dead_edges = g_list_prepend(priv->dead_edges, edge);
            /* set hover info to show which node the edge originally pointed to
             * since it is now dead */
            if (edge->hover_info)
            {
              g_free(edge->hover_info);
            }
            edge->hover_info = g_strdup_printf("Original target: %s (%s)\n",
                                               node->label ? node->label : "",
                                               node->name ? node->name : "");
          }
          if (freed_array)
          {
            /* keep same index since we just removed an array from the overall
             * edges array for this node */
            i--;
            break;
          }
        }
      }
    }
  }
}

static void remove_activity_from_node(NodeActivity *activity,
                                      ReplayGraphViewNode *node)
{
  remove_activity(node, activity);
}

static void add_activity_to_node(NodeActivity *activity,
                                 ReplayGraphViewNode *node)
{
  add_activity(node, activity);
}

/*
 * Unsets any existing parent of node (and will inturn actually remove the
 * parent node entirely if it now has no children) and resets parent to be
 * parent
 */
static void reparent_node(ReplayGraphView *self,
                          ReplayGraphViewNode *node,
                          ReplayGraphViewNode *parent)
{
  ReplayGraphViewPrivate *priv = self->priv;

  /* remove any existing parent */
  if (node->parent)
  {
    ReplayGraphViewNode *_parent = node->parent;
    g_assert(g_list_find(_parent->children, node) != NULL);

    /* if an node has a parent, then any edges it had get moved to the
     * canonical parent so transfer childs edges from the canonical parent's edge
     * list - tell transfer edges to only transfer those which match one of our
     * children */
    transfer_edges(self, get_canonical_node(node), node, TRUE);

    if (!node->hidden)
    {
      /* move node to visible list since it is now not a descendant of a
       * group */
      remove_node_from_hidden_array(self, node);
      add_node_to_visible_array(self, node);
    }

    g_list_foreach(node->activities, (GFunc)remove_activity_from_node,
                   _parent);
    _parent->children = g_list_remove(_parent->children,
                                      node);
    _parent->num_children--;
    node->parent = NULL;

    /* if this is the last child remove parent */
    if (_parent->num_children == 0)
    {
      remove_node(self, _parent);
    }
  }

  if (parent)
  {
    /* move childs edges to new canonical parent of node  */
    transfer_edges(self, node, get_canonical_node(parent), FALSE);

    parent->children = g_list_prepend(parent->children, node);
    parent->num_children++;
    node->parent = parent;

    g_list_foreach(node->activities, (GFunc)add_activity_to_node,
                   parent);

    /* move parent a bit towards node it now contains - if this is the only
     * node it contains, set parent to node's location */
    if (parent->num_children == 1)
    {
      parent->sx = node->sx;
      parent->sy = node->sy;
      parent->sz = node->sz;
    }
    else
    {
      parent->sx += (node->sx - parent->sx) / parent->num_children;
      parent->sy += (node->sy - parent->sy) / parent->num_children;
      parent->sz += (node->sz - parent->sz) / parent->num_children;
    }

    /* always move to hidden list */
    if (!node->hidden)
    {
      remove_node_from_visible_array(self, node);
      add_node_to_hidden_array(self, node);
    }
    g_assert(node_is_in_array(node, priv->hidden_nodes));
  }
}

/* removes the node and its associated parent if we are the last child */
static void remove_node(ReplayGraphView *self, ReplayGraphViewNode *node)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  /* if we have children, remove them - removing the last one will cause us to
   * inturn get removed, so don't access node pointer after calling
   * remove_node on last child */
  if (node->group)
  {
    while (node->children != NULL)
    {
      gboolean ret = (node->num_children == 1);
      remove_node(self,
                  (ReplayGraphViewNode *)(g_list_first(node->children)->data));
      if (ret)
      {
        return;
      }
    }
    /* at this point we should have no edges left if we are a group */
    g_assert(node->edges->len == 0);
  }

  /* remove parent - this will remove the parent node if we are last child
   * too */
  reparent_node(self, node, NULL);

  if (node->event_label)
  {
    g_free(node->event_label);
    node->event_label = NULL;
    priv->labelled_nodes = g_list_remove(priv->labelled_nodes, node);
  }

  /* remove node from list */
  if (node_is_in_array(node, priv->visible_nodes))
  {
    remove_node_from_visible_array(self, node);
  }
  else if (node_is_in_array(node, priv->hidden_nodes))
  {
    remove_node_from_hidden_array(self, node);
  }
  else
  {
    g_assert_not_reached();
  }

  g_hash_table_remove(priv->node_table, node->name);
  /* for each array of edges we share with another node, remove all the edges
   * which we hold from the array, and set the head node (which was us) to
   * NULL for all remaining edges in the array, then add the contents of the
   * array to the array of edges which point to NULL for the other node */
  transfer_edges(self, node, NULL, FALSE);

  /* now free actual list itself */
  g_ptr_array_free(node->edges, TRUE);
  if (node->name)
  {
    g_free(node->name);
  }
  if (node->label)
  {
    g_free(node->label);
  }

  /* if we are looking at node, reset view to 0, 0, 0 */
  if (priv->pIx == &(node->sx))
  {
    g_assert(priv->pIy == &(node->sy) && priv->pIz == &(node->sz));
    priv->pIx = &zero;
    priv->pIy = &zero;
    priv->pIz = &zero;
  }
  g_free(node);

}

static void stop_idle_processing(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->processing_id)
  {
    g_source_remove(priv->processing_id);
    priv->processing_id = 0;
    gtk_tree_path_free(priv->processing_start);
    gtk_tree_path_free(priv->processing_end);
    g_signal_emit(self, signals[PROCESSING], 0, FALSE);
  }
}

void replay_graph_view_unset_data(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  stop_idle_processing(self);

  replay_graph_view_stop_animation(self);

  reset_view_parameters(self);

  g_list_free(priv->active_nodes);
  priv->active_nodes = NULL;

  /* free all nodes and hence edges */
  while (priv->visible_nodes->len > 0)
  {
    remove_node(self,
                (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes, 0));
  }
  while (priv->hidden_nodes->len > 0)
  {
    remove_node(self,
                (ReplayGraphViewNode *)g_ptr_array_index(priv->hidden_nodes, 0));
  }
  g_assert(priv->visible_nodes->len == 0 &&
           priv->hidden_nodes->len == 0);

  if (priv->iter)
  {
    gtk_tree_iter_free(priv->iter);
    priv->iter = NULL;
  }

  if (priv->event_store)
  {
    /* remove old event list */
    g_signal_handler_disconnect(priv->event_store,
                                priv->event_inserted_id);
    g_signal_handler_disconnect(priv->event_store,
                                priv->current_changed_id);
    g_object_unref(priv->event_store);
    priv->event_store = NULL;
  }

  if (priv->node_tree)
  {
    /* remove old node_tree */
    g_signal_handler_disconnect(priv->node_tree,
                                priv->row_inserted_id);
    g_signal_handler_disconnect(priv->node_tree,
                                priv->row_changed_id);
    g_object_unref(priv->node_tree);
    priv->node_tree = NULL;
  }

  if (priv->fps_timer)
  {
    g_timer_destroy(priv->fps_timer);
    priv->fps_timer = NULL;
  }

  if (priv->search_completion)
  {
    gtk_entry_set_completion(GTK_ENTRY(priv->search_entry),
                             NULL);
    g_object_unref(priv->visible_nodes_model);
    g_object_unref(priv->search_completion);
    priv->search_completion = NULL;
  }

  if (priv->debug_timer)
  {
    g_timer_destroy(priv->debug_timer);
    priv->debug_timer = NULL;
  }

  /* schedule a redraw to clear widget surface if it exists */
  if (gtk_widget_get_window(GTK_WIDGET(self)))
  {
    replay_graph_view_render_ratelimit(self);
  }
}

/*
 * Dispose signal handler - should remove any refs which we might have other
 * nodes
 */
static void replay_graph_view_dispose(GObject *node)
{
  ReplayGraphView *self;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(node));
  self = REPLAY_GRAPH_VIEW(node);

  replay_graph_view_unset_data(self);

  /* call parent dispose */
  G_OBJECT_CLASS(replay_graph_view_parent_class)->dispose(node);
}

/* Finalize signal handler - should cleanup etc */
static void replay_graph_view_finalize(GObject *node)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(node));
  self = REPLAY_GRAPH_VIEW(node);

  priv = self->priv;

  g_assert(priv->layout_source == NULL);
  g_main_loop_quit(priv->layout_loop);
  g_thread_join(priv->layout_thread);
  g_mutex_clear(&priv->lock);

  g_hash_table_destroy(priv->node_table);
  g_hash_table_destroy(priv->selected_nodes);
  g_hash_table_destroy(priv->picked_nodes);
  g_ptr_array_free(priv->visible_nodes, TRUE);
  g_ptr_array_free(priv->hidden_nodes, TRUE);

  /* should destroy both window and search entry */
  gtk_widget_destroy(priv->search_window);

  if (priv->font_settings)
  {
    g_object_unref(priv->font_settings);
  }
  if (priv->settings)
  {
    g_object_unref(priv->settings);
  }
  if (priv->gl_config)
  {
    free_cairo_context(self);
    g_object_unref(priv->gl_config);
  }
  g_object_unref(priv->select_actions);
  g_object_unref(priv->view_actions);
  /* call parent finalize */
  G_OBJECT_CLASS(replay_graph_view_parent_class)->finalize(node);
}
/*
 * Handle the map-event - called when widget is mapped onto the display - start
 * rendering timeout
 */
static gboolean replay_graph_view_map_event(GtkWidget *widget,
                                         GdkEventAny *event)
{
  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);
  replay_graph_view_run_animation(REPLAY_GRAPH_VIEW(widget));

  return TRUE;
}


/*
 * Handle the unmap-event - called when widget is unmapped from the display -
 * stop rendering timeout
 */
static gboolean replay_graph_view_unmap_event(GtkWidget *widget,
                                           GdkEventAny *event)
{
  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);
  replay_graph_view_stop_animation(REPLAY_GRAPH_VIEW(widget));
  return TRUE;
}

/*
 * Handle the visibility-notify event - called when widget is obscured or
 * reshown - start and stop rendering timeout
 */
static gboolean replay_graph_view_visibility_notify_event(GtkWidget *widget,
                                                       GdkEventVisibility *event)
{
  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);

  if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
  {
    replay_graph_view_stop_animation(REPLAY_GRAPH_VIEW(widget));
  }
  else
  {
    replay_graph_view_run_animation(REPLAY_GRAPH_VIEW(widget));
  }
  return TRUE;
}

/**
 * process_hits:
 * @self: The ReplayGraphView we are processing for
 * @hits: the number of hits to process
 * @buffer: the buffer containing the opengl hit data
 * @hover: Whether we are selecting nodes as a result or just marking a single node as hover
 * @multiple: When hover is false, are we selecting a single or multiple nodes - if select is FALSE, this param is ignored
 * @recenter: When hover is false and multiple is false, recenter the view on the single selected node, otherwise recenter back to 0,0,0
 *
 * Return: TRUE if got hits, FALSE otherwise
 *
 * Processes hits contained within buffer for self.
 */
static gboolean process_hits(ReplayGraphView *self,
                             int hits,
                             GLuint buffer[],
                             gboolean hover,
                             gboolean multiple,
                             gboolean recenter)
{
  int i;
  GLuint names, *ptr, minZ, *ptrNames = NULL, numberOfNames = 0;
  ReplayGraphViewNode *node = NULL;
  ReplayGraphViewPrivate *priv = self->priv;
  gboolean ret = FALSE;

  ptr = (GLuint *)buffer;
  minZ = ~0x0;

  for (i = 0; i < hits; i++)
  {
    /* names */
    names = *ptr;
    /* skip over names */
    ptr++;

    /* select all when multiple - we could have multiple names for the same
     * hit */
    if (!hover && multiple)
    {
      /* skip over z1 and z2 */
      ptrNames = ptr + 2;
      numberOfNames = names;

      g_assert(numberOfNames <= 2);
      /* if there are 2 names, we've got a edge so ignore this hit, otherwise if
       * we have 1 name it is an node and so add to selection */
      if (numberOfNames == 1)
      {
        node = (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes,
                                                     *ptrNames);
        g_assert(node != NULL);
        /* mark node as picked if not already selected */
        if (!g_hash_table_lookup(priv->selected_nodes, node->name))
        {
          g_hash_table_insert(priv->picked_nodes, node->name, node);
        }
        ret = TRUE;
      }
    }
    /* otherwise find one with minimum z for hover */
    else
    {
      /* minZ index of hit */
      if (*ptr < minZ)
      {
        numberOfNames = names;
        minZ = *ptr;

        /* get names of this record */
        ptrNames = ptr + 2;
      }
    }

    /* go to next record - number of names of this record plus 2 (z
     * min and z max for the record) */
    ptr += names + 2;
  }

  /* return a single selection if it exists */
  if (!hover)
  {
    if (!multiple)
    {
      if (numberOfNames > 0)
      {
        /* could be 3 names if have selected a edge - ignore since we are
         * selecting and we only select nodes, not edges */
        GLuint obj_name = *ptrNames;
        g_assert(numberOfNames == 3 || numberOfNames == 1);
        if (numberOfNames == 1)
        {
          node = (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes,
                                                       obj_name);
          g_assert(node != NULL);
          /* mark node as picked if not already selected - otherwise remove
           * from selected nodes to unselect it */
          if (!g_hash_table_lookup(priv->selected_nodes, node->name))
          {
            g_hash_table_insert(priv->picked_nodes, node->name, node);
          }
          else
          {
            g_hash_table_remove(priv->selected_nodes, node->name);
          }

          if (recenter)
          {
            priv->pIx = &(node->sx);
            priv->pIy = &(node->sy);
            priv->pIz = &(node->sz);
            priv->x = 0.0;
            priv->y = 0.0;
          }
          ret = TRUE;
        }
      }
    }
  }
  else
  {
    if (numberOfNames > 0 && ptrNames != NULL)
    {
      GLuint name = *ptrNames;
      g_assert(numberOfNames == 1 || numberOfNames == 3);
      node = (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes,
                                                   name);
      if (node)
      {
        if (numberOfNames == 3)
        {
          GLuint edge_index, array_index;
          GPtrArray *edges;
          ReplayGraphViewEdge *edge;

          ptrNames++;
          array_index = *ptrNames;
          ptrNames++;
          edge_index = *ptrNames;

          edges = (GPtrArray *)g_ptr_array_index(node->edges, array_index);
          edge = (ReplayGraphViewEdge *)g_ptr_array_index(edges,
                                                       edge_index);
          priv->hovered_edge = edge;
        }
        else
        {
          /* mark node as hover */
          priv->hovered_node = node;
        }
        ret = TRUE;
      }
    }
  }
  return ret;
}

/**
 * pick_and_select:
 * @self: The ReplayGraphView we are processing for
 * @x: x-coord of center of pick region
 * @y: y-coord of center of pick region
 * @w: width of pick region
 * @h: height of pick region
 * @hover: Whether we are selecting nodes as a result or just marking a
 * single node as hover
 * @multiple: When select is true, are we selecting a single or multiple
 * nodes - if select is FALSE, this param is ignored
 * @recenter: When select is true and multiple is false, recenter the view on the single selected node, otherwise recenter back to 0,0,0
 * @edges: Pick edges instead of nodes
 *
 * Returns: TRUE if selected / hovered nodes, FALSE if nothing
 *
 * Pick region for self and select / hover nodes / edges from results
 */
static gboolean pick_and_select(ReplayGraphView *self,
                                float x, float y,
                                float w, float h,
                                gboolean hover,
                                gboolean multiple,
                                gboolean recenter,
                                gboolean edges)
{
  ReplayGraphViewPrivate *priv;
  GtkWidget *widget = GTK_WIDGET(self);
  gboolean ret = FALSE;

  g_return_val_if_fail(w != 0.0 && h != 0.0, FALSE);

  priv = self->priv;

  /* start OpenGL calls */
  if (priv->gl_config &&
      gtk_widget_begin_gl(widget))
  {
    float width, height, ratio;
    int hits = -1;
    GLuint *pick_buffer = NULL;
    GLuint buffer_size = 512; /* gets doubled before first use so is 1024 */
    GLint viewport[4];

    /* get current viewport for picking matrix dimensions */
    glGetIntegerv(GL_VIEWPORT, viewport);
    width = (float)gtk_widget_get_allocated_width(widget);
    height = (float)gtk_widget_get_allocated_height(widget);
    ratio = width / height;

    /* picking can return an error if buffer is not big enough so keep trying if
       we get an error */
    while (hits < 0)
    {
      /* double buffer size */
      buffer_size *= 2;
      pick_buffer = g_realloc(pick_buffer,
                              buffer_size * sizeof(pick_buffer[0]));
      /* setup for picking results */
      glSelectBuffer(buffer_size, pick_buffer);

      /* init name stack for selections */
      glInitNames();

      /* start in selection mode */
      glRenderMode(GL_SELECT);

      glMatrixMode(GL_PROJECTION);
      /* push current projection matrix */
      glPushMatrix();
      glLoadIdentity();

      /* set pick matrix */
      gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y),
                    (GLdouble)w, (GLdouble)h, viewport);

      /* set viewport to entire window */
      glViewport(0, 0, width, height);

      gluPerspective(priv->fovy, ratio, priv->z_near, priv->z_far);

      /* switch back to modelview to draw */
      glMatrixMode(GL_MODELVIEW);

      /* redisplay only graph since we can't pick stuff from the hud */
      replay_graph_view_draw_nodes_or_edges(self, edges);

      /* pop projection changes */
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      /* flush changes */
      glFlush();

      glMatrixMode(GL_MODELVIEW);
      /* get number of hits in select mode */
      hits = glRenderMode(GL_RENDER);
    }
    /* only process if we didn't get an error above */
    if (hits > 0)
    {
      ret = process_hits(self, hits, pick_buffer, hover, multiple, recenter);
    }
    g_free(pick_buffer);
    gtk_widget_end_gl(widget, FALSE);
  }
  return ret;
}

static void point_to_vector(GtkWidget *widget,
                            GLdouble x, GLdouble y,
                            GLdouble v[3])
{
  GLdouble d_xy, d_inv;
  GLdouble width, height;

  /* project event x, y onto a hemi-sphere centered within total width and
   * height */
  width = (GLdouble)gtk_widget_get_allocated_width(widget);
  height = (GLdouble)gtk_widget_get_allocated_height(widget);

  v[0] = ((2.0 * x) - width) / width;
  v[1] = (height - (2.0 * y)) / height;
  d_xy = sqrt((v[0] * v[0]) + (v[1] * v[1]));
  v[2] = cos((M_PI / 2.0) * ((d_xy < 1.0) ? d_xy : 1.0));
  d_inv = 1.0 / sqrt((v[0] * v[0]) +
                     (v[1] * v[1]) +
                     (v[2] * v[2]));

  v[0] *= d_inv;
  v[1] *= d_inv;
  v[2] *= d_inv;
}

static gboolean replay_graph_view_motion_notify_event(GtkWidget *widget,
                                                   GdkEventMotion *event)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), handled);

  self = REPLAY_GRAPH_VIEW(widget);
  priv = self->priv;

  /* ignore event if we don't have gl support */
  if (priv->gl_config)
  {
    gtk_widget_begin_gl(widget);
    /* handle selecting first */
    if (priv->selecting)
    {
      float dx, dy;

      priv->x2 = event->x;
      priv->y2 = event->y;

      dx = priv->x2 - priv->x1;
      dy = priv->y2 - priv->y1;

      /* doesn't make sense to pick from a region with zero width or height */
      if (dx != 0 && dy != 0)
      {
        g_hash_table_remove_all(priv->picked_nodes);
        /* pick nodes to try and select them */
        pick_and_select(self,
                        priv->x1 + (dx / 2.0), /* center of region */
                        priv->y1 + (dy / 2.0), /* center of region */
                        fabs(dx), /* width of region */
                        fabs(dy), /* height of region */
                        FALSE, /* select not hover */
                        TRUE, /* select multiple nodes */
                        FALSE,/* ignored since multiple is TRUE */
                        FALSE); /* don't try and select edges */

        /* always manually redraw when selecting since we stop animation during
         * this time */
        handled = TRUE;
      }
    }

    if (priv->panning)
    {
      /* pan by difference */
      float dx, dy;

      priv->x2 = (float)event->x;
      priv->y2 = (float)event->y;

      dx = priv->x2 - priv->x1;
      dy = priv->y2 - priv->y1;

      priv->x1 = priv->x2;
      priv->y1 = priv->y2;

      priv->x -= dx * priv->z / priv->max_camera_distance;
      priv->y += dy * priv->z / priv->max_camera_distance;
      handled = TRUE;
    }

    /* now handle rotating as well */
    if (priv->rotating && !priv->two_dimensional)
    {
      GLdouble cur_pos[3];
      GLdouble angle, dx, dy, dz;
      GLdouble axis[3];

      /* offset event by start position so we can drag a horizontal line that
       * doesn't have to pass through the centre to rotate in xz plane etc */
      priv->x2 = event->x;
      priv->y2 = event->y;
      priv->x2 -= (priv->x1 - ((GLdouble)gtk_widget_get_allocated_width(widget) / 2.0));
      priv->y2 -= (priv->y1 - ((GLdouble)gtk_widget_get_allocated_height(widget) / 2.0));
      point_to_vector(widget, priv->x2, priv->y2, cur_pos);

      priv->x1 = event->x;
      priv->y1 = event->y;

      /* get the angle to rotate */
      dx = cur_pos[0] - priv->start_pos[0];
      dy = cur_pos[1] - priv->start_pos[1];
      dz = cur_pos[2] - priv->start_pos[2];
      angle = 180.0 * sqrt(dx * dx + dy * dy + dz * dz);

      axis[0] = ((priv->start_pos[1] * cur_pos[2]) -
                 (priv->start_pos[2] * cur_pos[1]));
      axis[1] = ((priv->start_pos[2] * cur_pos[0]) -
                 (priv->start_pos[0] * cur_pos[2]));
      axis[2] = ((priv->start_pos[0] * cur_pos[1]) -
                 (priv->start_pos[1] * cur_pos[0]));

      /* now modify modelview matrix */
      glPushMatrix();
      /* reset modelview */
      glLoadIdentity();
      /* do new rotation */
      glRotated(angle, axis[0], axis[1], axis[2]);
      /* apply existing rotation */
      glMultMatrixd(priv->rotation);
      /* now get new resultant rotation */
      glGetDoublev(GL_MODELVIEW_MATRIX, priv->rotation);
      /* undo back to previous */
      glPopMatrix();

      /* make sure we render window */
      handled = TRUE;
    }

    if (priv->moving)
    {
      /* we need to convert the projected coordinates to world coords */
      GLint viewport[4];
      GLdouble modelview[16];
      GLdouble projection[16];
      GLdouble objx1, objy1, objz1;
      GLdouble objx2, objy2, objz2;
      GLdouble dx, dy, dz;
      GLdouble invy;
      GHashTableIter iter;
      gpointer key, value;

      priv->x2 = (float)event->x;
      priv->y2 = (float)event->y;

      /* get matrices to use gluUnProject */
      glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
      glGetDoublev(GL_PROJECTION_MATRIX, projection);
      glGetIntegerv(GL_VIEWPORT, viewport);


      /* convert start and end location to world coords */
      /* y positions need to be inverted */
      invy = viewport[3] - priv->y1 - 1;
      gluUnProject(priv->x1, invy, 1.0, modelview, projection, viewport,
                   &objx1, &objy1, &objz1);
      invy = viewport[3] - priv->y2 - 1;
      gluUnProject(priv->x2, invy, 1.0, modelview, projection, viewport,
                   &objx2, &objy2, &objz2);

      dx = objx2 - objx1;
      dy = objy2 - objy1;
      dz = objz2 - objz1;

      priv->x1 = priv->x2;
      priv->y1 = priv->y2;

      /* move all selected nodes */
      g_hash_table_iter_init(&iter, priv->selected_nodes);
      while (g_hash_table_iter_next(&iter, &key, &value))
      {
        ReplayGraphViewNode *node = (ReplayGraphViewNode *)value;
        node->sx += dx * priv->z / priv->max_camera_distance;
        node->sy += dy * priv->z / priv->max_camera_distance;
        node->sz += dz * priv->z / priv->max_camera_distance;
      }

      handled = TRUE;
    }

    /* only try and do hover info if we haven't handled this event already */
    if (!handled)
    {
      guint modifiers;
      gboolean pick_edges = FALSE;
      ReplayGraphViewNode *old_hovered_node = priv->hovered_node;
      ReplayGraphViewEdge *old_hovered_edge = priv->hovered_edge;

      /* always reset hovered node / edge - if we found something under the
       * mouse cursor then we should be able to find it by picking and choosing
       * - otherwise we don't have any hovered node / edge */
      priv->hovered_node = NULL;
      priv->hovered_edge = NULL;

      /* try and pick an node within the 5x5 region of the mouse pointer -
       * if we get one, then we set it to display extra info by setting it as
       * the hovered_node - make sure we only set handled if we changed the
       * hovered node */
      g_hash_table_remove_all(priv->picked_nodes);

      /* only try and do hover info for edges if CTRL is held down */
      modifiers = gtk_accelerator_get_default_mod_mask();
      pick_edges = (event->state & modifiers) == GDK_CONTROL_MASK;

      pick_and_select(self,
                      (double)event->x, (double)event->y,
                      5.0, 5.0,
                      TRUE, /* hover - so display extra info */
                      FALSE, /* select single nodes - ignored as a
                              * param anyway since hover is TRUE */
                      FALSE,  /* ignored since hover is TRUE */
                      pick_edges); /* pick edges */
      handled = (priv->hovered_node != old_hovered_node ||
                 priv->hovered_edge != old_hovered_edge);
    }
  }
  gtk_widget_end_gl(widget, FALSE);

  /* if handled, then we updated node info, so we need to redraw */
  if (handled)
  {
    replay_graph_view_render_ratelimit(self);
  }

  /* call to get future events after this time - see docs for
   * GDK_POINTER_MOTION_HINT_MASK - use instead of gdk_window_get_pointer() to
   * request further motion notifies, because it also works for extension events
   * where motion notifies are provided for devices other than the core
   * pointer */
  gdk_event_request_motions(event);

  return handled;
}

static gboolean remove_to_selected_table(gpointer key,
                                         gpointer value,
                                         gpointer user_data)
{
  GHashTable *selected_nodes = (GHashTable *)(user_data);

  g_hash_table_insert(selected_nodes, key, value);
  /* return TRUE to make sure is removed from picked table */
  return TRUE;
}

static gboolean replay_graph_view_button_release_event(GtkWidget *widget,
                                                    GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), handled);

  priv = REPLAY_GRAPH_VIEW(widget)->priv;

  if (priv->selecting)
  {
    priv->selecting = FALSE;
    /* re-animate */
    replay_graph_view_run_animation(REPLAY_GRAPH_VIEW(widget));
    priv->x2 = (float)event->x;
    priv->y2 = (float)event->y;
    handled = TRUE;
  }
  else if (priv->moving)
  {
    priv->moving = FALSE;
    /* if we hovered an node, remove it from selected table */
    if (priv->hovered_node)
    {
      g_hash_table_remove(priv->selected_nodes, priv->hovered_node->name);
      /* and unset hovered node since is unlikely to still be hovered (ie have
       * the mouse directly over it */
    }
    priv->hovered_node = NULL;
    handled = TRUE;
  }
  else if (priv->rotating)
  {
    priv->rotating = FALSE;
    handled = TRUE;
  }
  else if (priv->panning)
  {
    priv->panning = FALSE;
    handled = TRUE;
  }
  /* move from picked table to selected table */
  g_hash_table_foreach_remove(priv->picked_nodes,
                              remove_to_selected_table,
                              priv->selected_nodes);
  return handled;
}

static void
handle_2button_press_event(ReplayGraphView *self,
                           GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv;
  guint modifiers;

  priv = self->priv;
  modifiers = gtk_accelerator_get_default_mod_mask();

  /* if double click with control, pan back to 0, 0 */
  if ((event->state & modifiers) == GDK_CONTROL_MASK)
  {
    priv->x = 0;
    priv->y = 0;
  }
  else
  {
    /* for double clicks, recenter on node if one is under cursor,
     * otherwise recenter on 0,0,0 */
    if (!pick_and_select(self,
                         (double)event->x, (double)event->y,
                         5.0, 5.0,
                         FALSE,  /* select */
                         FALSE,  /* select single nodes */
                         TRUE,   /* recenter */
                         FALSE)) /* don't pick edges */
    {
      /* no node picked so reset to zero */
      priv->pIx = &zero;
      priv->pIy = &zero;
      priv->pIz = &zero;
    }
  }
}

static void
start_moving_hovered_node(ReplayGraphView *self,
                          GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv = self->priv;

  /* unset hovered_node if is already in selected */
  if (g_hash_table_lookup(priv->selected_nodes,
                          priv->hovered_node->name))
  {
    priv->hovered_node = NULL;
  }
  else
  {
    g_hash_table_insert(priv->selected_nodes,
                        priv->hovered_node->name,
                        priv->hovered_node);
  }
  priv->moving = TRUE;
  priv->x1 = (float)event->x;
  priv->y1 = (float)event->y;
  priv->x2 = (float)event->x;
  priv->y2 = (float)event->y;
}

static void
start_selecting(ReplayGraphView *self,
                GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv = self->priv;

  /* try and select a single node from a 5x5 region at the current
   * cursor position */
  if (!pick_and_select(self,
                       (double)event->x, (double)event->y,
                       5.0, 5.0,
                       FALSE, /* select not hover */
                       FALSE, /* select single nodes */
                       FALSE, /* don't recenter view */
                       FALSE)) /* don't pick edges */
  {
    unselect_all_nodes(self);
  }
  /* stop animation */
  replay_graph_view_stop_animation(self);
  priv->selecting = TRUE;
  priv->x1 = (float)event->x;
  priv->y1 = (float)event->y;
  priv->x2 = (float)event->x;
  priv->y2 = (float)event->y;
}

static void
start_panning(ReplayGraphView *self,
              GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv = self->priv;

  priv->panning = TRUE;

  /* save this start event so we can offset pans using it */
  priv->x1 = (float)event->x;
  priv->y1 = (float)event->y;
}

static void
start_rotating(ReplayGraphView *self,
               GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv = self->priv;
  GtkWidget *widget = GTK_WIDGET(self);

  priv->rotating = TRUE;

  /* save this start event so we can offset drags using it */
  priv->x1 = (float)event->x;
  priv->y1 = (float)event->y;

  /* assume we start though at the centre of the screen since this way
   * you can drag in a horizontal line from anywhere on the screen to
   * make it rotate in xy plane and vice-versa for vertical line -
   * doesn't have to pass through middle of the graph */
  point_to_vector(widget,
                  (GLdouble)gtk_widget_get_allocated_width(widget) / 2.0,
                  (GLdouble)gtk_widget_get_allocated_height(widget) / 2.0,
                  priv->start_pos);
}

static void
handle_1button_press_event(ReplayGraphView *self,
                           GdkEventButton *event)
{
  ReplayGraphViewPrivate *priv;
  guint modifiers;

  priv = self->priv;

  modifiers = gtk_accelerator_get_default_mod_mask();

  /* button 1 with control is the same as button 2 to emulate for trackpads
   * etc */
  if ((event->button == 1 &&
       (event->state & modifiers) & GDK_CONTROL_MASK) ||
      event->button == 2)
  {
    if ((event->state & modifiers) & GDK_SHIFT_MASK)
    {
      start_panning(self, event);
    }
    else
    {
      start_rotating(self, event);
    }
  }
  else if (event->button == 1)
  {
    /* if we have a hovered node, user has clicked over an node, so is
     * trying to select a single node - add to selected nodes hash
     * table and set moving */
    if (priv->hovered_node)
    {
      start_moving_hovered_node(self, event);
    }
    else
    {
      start_selecting(self, event);
    }
  }
  else if (event->button == 3)
  {
    do_popup_menu(self, event);
  }
}

static gboolean replay_graph_view_button_press_event(GtkWidget *widget,
                                                  GdkEventButton *event)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), handled);

  self = REPLAY_GRAPH_VIEW(widget);
  priv = self->priv;

  if (priv->gl_config)
  {
    /* grab focus */
    gtk_widget_grab_focus(widget);

    if (priv->visible_nodes->len > 0)
    {
      if (event->type == GDK_2BUTTON_PRESS)
      {
        handle_2button_press_event(self, event);
      }
      else if (event->type == GDK_BUTTON_PRESS)
      {
        handle_1button_press_event(self, event);
      }
    }
  }
  return TRUE;
}

static gboolean replay_graph_view_scroll_event(GtkWidget *widget,
                                            GdkEventScroll *event)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);

  self = REPLAY_GRAPH_VIEW(widget);
  priv = self->priv;

  if (priv->gl_config != NULL && priv->visible_nodes->len > 0)
  {
    if (event->direction == GDK_SCROLL_DOWN)
    {
      /* zoom in */
      priv->z = MIN(priv->z * 1.1f, priv->max_camera_distance);
      handled = TRUE;
    }

    if (event->direction == GDK_SCROLL_UP)
    {
      /* zoom out */
      priv->z = MAX(priv->z / 1.1f, priv->min_camera_distance);
      handled = TRUE;
    }

    /* schedule a redraw if needed */
    if (handled)
    {
      replay_graph_view_render_ratelimit(self);
    }
  }
  return handled;
}

static void select_children(ReplayGraphViewNode *node,
                            ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  guint i;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));
  priv = self->priv;

  for (i = 0; i < node->edges->len; i++)
  {
    GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, i);
    guint j;

    for (j = 0; j < edges->len; j++)
    {
      ReplayGraphViewEdge *edge =
        (ReplayGraphViewEdge *)g_ptr_array_index(edges, j);
      if (edge->tail == node && edge->head)
      {
        g_hash_table_insert(priv->selected_nodes, edge->head->name, edge->head);
      }
    }
  }
}


static void expand_selection(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  priv = self->priv;

  if (g_hash_table_size(priv->selected_nodes) > 0)
  {
    /* get the single nodes and add their edges to selected nodes */
    GList *list = g_hash_table_get_values(priv->selected_nodes);

    g_list_foreach(list,
                   (GFunc)select_children,
                   self);
    g_list_free(list);
  }
  if (g_hash_table_size(priv->picked_nodes) > 0)
  {
    /* get the single nodes and add their edges to selected nodes */
    GList *list = g_hash_table_get_values(priv->picked_nodes);

    g_list_foreach(list,
                   (GFunc)select_children,
                   self);
    g_list_free(list);
  }
}

static void select_parents(ReplayGraphViewNode *node,
                           ReplayGraphView *self)
{
  guint i;
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));
  priv = self->priv;

  for (i = 0; i < node->edges->len; i++)
  {
    GPtrArray *edges = (GPtrArray *)g_ptr_array_index(node->edges, i);
    guint j;

    for (j = 0; j < edges->len; j++)
    {
      ReplayGraphViewEdge *edge =
        (ReplayGraphViewEdge *)g_ptr_array_index(edges, j);
      if (edge->head == node)
      {
        g_hash_table_insert(priv->selected_nodes, edge->tail->name, edge->tail);
      }
    }
  }
}

static void contract_selection(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;

  priv = self->priv;

  if (g_hash_table_size(priv->selected_nodes) > 0)
  {
    /* get the single nodes and add the reverse edges to selected nodes */
    GList *list = g_hash_table_get_values(priv->selected_nodes);

    g_list_foreach(list,
                   (GFunc)select_parents,
                   self);
  }
}

static gboolean replay_graph_view_key_press_event(GtkWidget *widget,
                                               GdkEventKey *event)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean render = FALSE;
  GdkRectangle rect;
  gint x, y;
  guint modifiers;
  gboolean handled = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(widget), FALSE);

  self = REPLAY_GRAPH_VIEW(widget);
  priv = self->priv;

  /* only do something if we have a visible graph */
  if (priv->visible_nodes->len > 0)
  {
    modifiers = gtk_accelerator_get_default_mod_mask();
    /* handle Ctrl-S for search or '/' */
    if (((event->state & modifiers) == GDK_CONTROL_MASK &&
         (gdk_keyval_to_lower(event->keyval) == 's')) ||
        (!(event->state & modifiers) && event->keyval == '/'))
    {
      /* remove any existing timeout, and re-add a timeout to hide search window
       * after 5 seconds of inactivity */
      if (priv->searching_id)
      {
        g_source_remove(priv->searching_id);
      }
      priv->searching_id = g_timeout_add_seconds(5,
                                                 hide_search_window,
                                                 self);

      gtk_widget_grab_focus(priv->search_entry);
      gtk_window_present_with_time(GTK_WINDOW(priv->search_window),
                                   event->time);

      /* move after presenting so size is allocated already */
      gdk_window_get_frame_extents(gtk_widget_get_window(priv->search_window), &rect);
      gdk_window_get_origin(gtk_widget_get_window(widget), &x, &y);
      gtk_window_move(GTK_WINDOW(priv->search_window),
                      x + gtk_widget_get_allocated_width(widget) - rect.width,
                      y + gtk_widget_get_allocated_height(widget));
      handled = TRUE;
    }
    else if (!(event->state & modifiers))
    {
      switch (gdk_keyval_to_lower(event->keyval))
      {
        case 'd': /* fall thru */
          priv->debug = !priv->debug;
          render = TRUE;
          break;

        case 'i': /* fall thru */
          invert_selection(self);
          render = TRUE;
          break;

        case GDK_KEY_Left:
          priv->x -= DELTA_PAN;
          render = TRUE;
          break;

        case GDK_KEY_Right:
          priv->x += DELTA_PAN;
          render = TRUE;
          break;

        case GDK_KEY_Up:
          priv->y += DELTA_PAN;
          render = TRUE;
          break;

        case GDK_KEY_Down:
          priv->y -= DELTA_PAN;
          render = TRUE;
          break;

        case GDK_KEY_KP_Add:
          expand_selection(self);
          render = TRUE;
          break;

        case GDK_KEY_KP_Subtract:
          contract_selection(self);
          render = TRUE;
          break;

        default:
          break;
      }
    }
  }

  /* schedule a redraw if needed */
  if (render)
  {
    replay_graph_view_render_ratelimit(self);
  }
  return render ? render : handled;
}

/* Called to render scene to animate it */
static void replay_graph_view_render_graph(ReplayGraphView *self)
{
  gtk_widget_queue_draw(GTK_WIDGET(self));
}

static gboolean replay_graph_view_render_ratelimit(gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);

  self = REPLAY_GRAPH_VIEW(data);
  priv = self->priv;

  /* we only render if we have gone for priv->max_fps time slice without rendering a
   * frame */
  if (!priv->fps_timer ||
      (priv->max_fps * g_timer_elapsed(priv->fps_timer, NULL) >= 1.0))
  {
    replay_graph_view_render_graph(self);
  }

  /* keep timeout running */
  return TRUE;
}

/* creates the given node and its corresponding parents as needed */
static ReplayGraphViewNode *add_node_with_name(ReplayGraphView *self,
                                            const gchar *name,
                                            GError **error)
{
  ReplayGraphViewPrivate *priv;
  GtkTreePath *path;
  ReplayGraphViewNode *node = NULL;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), node);

  priv = self->priv;

  g_assert((ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, name) == NULL);

  /* if node exists in global node list then add it */
  path = replay_node_tree_get_tree_path(priv->node_tree, name);
  if (path != NULL)
  {
    gboolean hidden;
    gchar *color;

    node = (ReplayGraphViewNode *)g_malloc0(sizeof(*node));
    node->edges = g_ptr_array_new();
    gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->node_tree), &(node->iter),
                            path);

    gtk_tree_model_get(GTK_TREE_MODEL(priv->node_tree),
                       &(node->iter),
                       REPLAY_NODE_TREE_COL_GROUP, &(node->group),
                       REPLAY_NODE_TREE_COL_COLOR, &color,
                       REPLAY_NODE_TREE_COL_ID, &(node->name),
                       REPLAY_NODE_TREE_COL_LABEL, &(node->label),
                       REPLAY_NODE_TREE_COL_HIDDEN, &hidden,
                       -1);

    parse_color_to_flv(color, node->color);
    g_free(color);

    node->sx = g_random_double_range(-1.0, 1.0) * priv->max_node_distance;
    node->sy = g_random_double_range(-1.0, 1.0) * priv->max_node_distance;
    node->sz = (priv->two_dimensional ? 0.0 :
                g_random_double_range(-1.0, 1.0) * priv->max_node_distance);
    node->hidden = hidden;
    node_update_mass(node);
    g_hash_table_insert(priv->node_table, node->name, node);
    if (node->hidden)
    {
      add_node_to_hidden_array(self, node);
    }
    else
    {
      add_node_to_visible_array(self, node);
    }

    /* create parents if this row already has them and they don't exist */
    if (gtk_tree_path_get_depth(path) > 1)
    {
      GtkTreeIter parent_iter;
      GtkTreePath *parent_path;
      gchar *parent_name;
      ReplayGraphViewNode *parent = NULL;

      parent_path = gtk_tree_path_copy(path);
      gtk_tree_path_up(parent_path);
      gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->node_tree),
                              &parent_iter,
                              parent_path);
      gtk_tree_path_free(parent_path);
      gtk_tree_model_get(GTK_TREE_MODEL(priv->node_tree),
                         &parent_iter,
                         REPLAY_NODE_TREE_COL_ID, &parent_name,
                         -1);
      /* get parent - if it doesn't exist, create it */
      parent = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                       parent_name);
      if (parent == NULL)
      {
        parent = add_node_with_name(self, parent_name, NULL);
      }
      g_free(parent_name);
      /* parent now exists, so set us as child */
      reparent_node(self, node, parent);
      g_assert(parent != NULL);
      g_assert(parent->group);
      g_assert(node->parent == parent);
      g_assert(g_list_find(node->parent->children, node) != NULL);
    }
    g_assert(node->children == NULL);
    gtk_tree_path_free(path);

    /* if running backwards, check dead edge list for any which point to this
     * node */
    if (!priv->forward)
    {
      GList *list;
      for (list = priv->dead_edges; list != NULL; /* advance manually */)
      {
        /* save next incase we remove this element from list */
        GList *next = g_list_next(list);
        ReplayGraphViewEdge *edge = (ReplayGraphViewEdge *)list->data;
        g_assert(edge->head == NULL);

        if (edge->head_name &&
            g_ascii_strcasecmp(edge->head_name, node->name) == 0)
        {
          priv->dead_edges = g_list_remove(priv->dead_edges, edge);
          /* remove and re-add edge with new head */
          node_remove_edge(edge->tail, edge);
          edge->head = node;
          node_add_edge(edge->tail, edge);
        }
        list = next;
      }
    }
  }
  else
  {
    g_set_error(error, REPLAY_GRAPH_VIEW_ERROR, INVALID_NODE_ERROR,
                "%s: %d: %s: got add_node for an node which doesn't exist in the global node list: [%s]\n",
                __FILE__, __LINE__, __func__, name);
  }
  return node;
}

static gboolean remove_node_with_name(ReplayGraphView *self,
                                      const gchar *name,
                                      GError **error)
{
  ReplayGraphViewNode *node;
  ReplayGraphViewPrivate *priv;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), FALSE);

  priv = self->priv;

  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, name);
  if (node != NULL)
  {
    remove_node(self, node);
    ret = TRUE;
  }
  else
  {
    g_set_error(error, REPLAY_GRAPH_VIEW_ERROR, INVALID_NODE_ERROR,
                "%s: %d: %s: could not find node to remove\n",
                __FILE__, __LINE__, __FUNCTION__);
  }
  return ret;
}

static void edge_set_properties(ReplayGraphViewEdge *edge,
                                GVariant *props)
{
  replay_property_store_push(edge->props, props);
  node_update_mass(edge->tail);
  if (edge->head && edge->head != edge->tail)
  {
    node_update_mass(edge->head);
  }
}

static void edge_unset_properties(ReplayGraphViewEdge *edge,
                                  GVariant *props)
{
  replay_property_store_pop(edge->props, props);
  node_update_mass(edge->tail);
  if (edge->head && edge->head != edge->tail)
  {
    node_update_mass(edge->head);
  }
}

static gboolean add_edge(ReplayGraphView *self,
                         gboolean directed,
                         const gchar *edge_name,
                         const gchar *tail_name,
                         const gchar *head_name,
                         GVariant *props,
                         GError **error)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node;
  ReplayGraphViewEdge *edge;
  GtkTreePath *path;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), FALSE);

  priv = self->priv;

  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                 tail_name);
  if (node)
  {
    /* always add to canonical node */
    node = get_canonical_node(node);
    edge = g_malloc0(sizeof(*edge));
    edge->props = replay_property_store_new();
    edge->directed = directed;
    edge->name = g_strdup(edge_name);
    edge->tail_name = g_strdup(tail_name);
    edge->head_name = g_strdup(head_name);
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                   priv->iter);
    edge->tail = node;
    gtk_tree_path_free(path);
    if (edge->head_name)
    {
      edge->head = ((ReplayGraphViewNode *)
                    g_hash_table_lookup(priv->node_table, edge->head_name));
      if (edge->head)
      {
        /* always use canonical node */
        edge->head = get_canonical_node(edge->head);
        /* success */
        ret = TRUE;
      }
    }
    /* if no head_name specified or we couldn't find a head node then this can
     * only happen when running backwards */
    if (!edge->head)
    {
      /* we couldn't match it up to an existing node it is probably a dead edge
         - ie points to an node which doesn't exist - this can be the case only
         if we're running backwards */
      if (!priv->forward && edge->head_name)
      {
        priv->dead_edges = g_list_prepend(priv->dead_edges, edge);
        ret = TRUE;
      }
      else
      {
        g_set_error(error, REPLAY_GRAPH_VIEW_ERROR, INVALID_EDGE_ERROR,
                    "%s: %d: %s: could not find head node '%s' for this edge\n",
                    __FILE__, __LINE__, __FUNCTION__, edge->head_name);
      }
    }
    g_hash_table_insert(priv->edge_table, edge->name, edge);

    /* set props at end */
    edge_set_properties(edge, props);

    node_add_edge(node, edge);
  }
  else
  {
    g_set_error(error, REPLAY_GRAPH_VIEW_ERROR, INVALID_NODE_ERROR,
                "%s: %d: %s: could not find source node for this edge\n",
                __FILE__, __LINE__, __FUNCTION__);
  }
  g_assert(ret || error);
  return ret;
}

static gboolean remove_edge(ReplayGraphView *self,
                            gboolean directed,
                            const gchar *edge_name,
                            const gchar *tail_name,
                            const gchar *head_name,
                            GError **error)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewEdge *edge;
  ReplayGraphViewNode *node;
  gboolean ret = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(self), FALSE);

  priv = self->priv;

  edge = g_hash_table_lookup(priv->edge_table, edge_name);
  if (edge)
  {
    node = ((ReplayGraphViewNode *)
            g_hash_table_lookup(priv->node_table, tail_name));
    /* always use canonical node */
    node = get_canonical_node(node);
    node_remove_edge(node, edge);
    free_edge(self, edge);
    ret = TRUE;
  }
  else
  {
    g_set_error(error, REPLAY_GRAPH_VIEW_ERROR, INVALID_EDGE_ERROR,
                "%s: %d: %s: could not find edge to remove\n",
                __FILE__, __LINE__, __FUNCTION__);
  }
  return ret;
}

static void free_edge_event_label(gpointer list_data, gpointer data)
{
  ReplayGraphViewEdge *edge = (ReplayGraphViewEdge *)list_data;
  g_assert(edge->event_label != NULL);

  g_free(edge->event_label);
  edge->event_label = NULL;
}

static void free_node_event_label(gpointer list_data, gpointer data)
{
  ReplayGraphViewNode *node = (ReplayGraphViewNode *)list_data;
  g_assert(node->event_label != NULL);

  g_free(node->event_label);
  node->event_label = NULL;
}

static gchar *
get_message_description(ReplayMsgSendEvent *event)
{
  gchar *description = NULL;

  g_variant_lookup(replay_msg_send_event_get_props(event),
                   "description", "s", &description);
  return description;
}


static void label_from_event(ReplayGraphView *self, ReplayEvent *event)
{
  ReplayGraphViewPrivate *priv;
  ReplayGraphViewNode *node = NULL;
  ReplayGraphViewNode *parent = NULL;
  const gchar *edge_name = NULL, *message = NULL;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (REPLAY_IS_MSG_SEND_EVENT(event)) {
    ReplayMsgSendEvent *msg_send = REPLAY_MSG_SEND_EVENT(event);
    node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                   replay_msg_send_event_get_node(msg_send));
    edge_name = replay_msg_send_event_get_edge(msg_send);
    message = get_message_description(msg_send);
  } else if (REPLAY_IS_MSG_RECV_EVENT(event)) {
    ReplayMsgRecvEvent *msg_recv = REPLAY_MSG_RECV_EVENT(event);
    ReplayMsgSendEvent *msg_send = replay_msg_recv_event_get_send_event(msg_recv);
    node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                   replay_msg_recv_event_get_node(msg_recv));
    edge_name = replay_msg_send_event_get_edge(msg_send);
    message = get_message_description(msg_send);
  } else {
    goto out;
  }

  if (node && message)
  {
    ReplayGraphViewEdge *edge;
    parent = get_canonical_node(node);

    if (edge_name)
    {
      edge = g_hash_table_lookup(priv->edge_table,
                                 edge_name);
      if (edge)
      {
        /* replace any existing label */
        if (edge->event_label)
        {
          g_free(edge->event_label);
        }
        else
        {
          priv->labelled_edges = g_list_prepend(priv->labelled_edges,
                                                edge);
        }
        edge->event_label = g_strdup(message);
        /* place close to sender */
        edge->position = (edge->tail == parent ? 0.25 : 0.75);
      }

      /* label sending node as well */
      if (node->label)
      {
        /* replace any existing label */
        if (parent->event_label)
        {
          g_free(parent->event_label);
        }
        else
        {
          priv->labelled_nodes = g_list_prepend(priv->labelled_nodes,
                                                parent);
        }
        parent->event_label = g_strdup(node->label);
      }
    }
    else
    {
      /* no edge specified - just label node with msg label */
      /* replace any existing label */
      if (parent->event_label)
      {
        g_free(parent->event_label);
      }
      else
      {
        priv->labelled_nodes = g_list_prepend(priv->labelled_nodes,
                                              parent);
      }
      parent->event_label = g_strdup(message);
    }
  }
  out:
  return;
}

static void set_node_properties(ReplayGraphView *self,
                                const gchar *id,
                                GVariant *props)
{
  ReplayGraphViewPrivate *priv;
  GtkTreeIter iter;
  gboolean ret;

  priv = self->priv;

  ret = replay_node_tree_get_iter_for_id(priv->node_tree, &iter, id);
  if (ret)
  {
    /* set properties in node tree and respond to change */
    replay_node_tree_push_props(priv->node_tree, &iter, props);
  }
}

static void unset_node_properties(ReplayGraphView *self,
                                  const gchar *id,
                                  GVariant *props)
{
  ReplayGraphViewPrivate *priv;
  GtkTreeIter iter;
  gboolean ret;

  priv = self->priv;

  ret = replay_node_tree_get_iter_for_id(priv->node_tree, &iter, id);
  if (ret)
  {
    /* set properties in node tree and respond to change */
    replay_node_tree_pop_props(priv->node_tree, &iter, props);
  }
}

static void set_edge_properties(ReplayGraphView *self,
                                const gchar *name,
                                GVariant *props)
{
  ReplayGraphViewEdge *edge;

  edge = (ReplayGraphViewEdge *)g_hash_table_lookup(self->priv->edge_table, name);

  if (edge)
  {
    edge_set_properties(edge, props);
  }
}

static void unset_edge_properties(ReplayGraphView *self,
                                const gchar *name,
                                GVariant *props)
{
  ReplayGraphViewEdge *edge;

  edge = (ReplayGraphViewEdge *)g_hash_table_lookup(self->priv->edge_table, name);

  if (edge)
  {
    edge_unset_properties(edge, props);
  }
}

static void display_error_for_event(ReplayGraphView *self,
                                    GError *error,
                                    ReplayEvent *event)
{
  ReplayGraphViewPrivate *priv;
  GtkWidget *dialog;
  gchar *markup;
  gchar *string;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  g_assert(priv->iter != NULL);

  dialog = gtk_message_dialog_new(NULL,
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  "Error processing event");

  string = replay_event_to_string(event,
                                  (priv->absolute_time ? 0 :
                                   replay_event_store_get_start_timestamp(priv->event_store)));
  markup = g_strdup_printf("%s\n"
                           "%s",
                           error ? error->message : "No error message",
                           string);

  gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
                                             "%s", markup);
  g_free(markup);
  g_free(string);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

/* since we are going backwards we need to make it look like we just processed
 * the previous event - hence we need to know its event so we can label things
 * as though we just processed the previous event - and thus we need prev_iter
 * to point to the element in event_store before the current one */
static void process_event_backward(ReplayGraphView *self,
                                   GtkTreeIter *prev_iter)
{
  ReplayGraphViewPrivate *priv;
  ReplayEvent *event = NULL;
  gboolean ret = FALSE;
  GError *error = NULL;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;
  g_assert(!priv->forward);

  event = replay_event_store_get_event(priv->event_store, priv->iter);

  g_mutex_lock(&priv->lock);

  if (REPLAY_IS_NODE_CREATE_EVENT(event)) {
    ret = remove_node_with_name(self,
                                replay_node_create_event_get_id(REPLAY_NODE_CREATE_EVENT(event)),
                                &error);
  } else if (REPLAY_IS_NODE_DELETE_EVENT(event)) {
    ret = (add_node_with_name(self,
                              replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event)),
                              &error) != NULL);
  } else if (REPLAY_IS_EDGE_CREATE_EVENT(event)) {
    ReplayEdgeCreateEvent *edge_create = REPLAY_EDGE_CREATE_EVENT(event);
    ret = remove_edge(self,
                      replay_edge_create_event_get_directed(edge_create),
                      replay_edge_create_event_get_id(edge_create),
                      replay_edge_create_event_get_tail(edge_create),
                      replay_edge_create_event_get_head(edge_create),
                      &error);
  } else if (REPLAY_IS_EDGE_DELETE_EVENT(event)) {
    ReplayEdgeCreateEvent *edge_create = replay_edge_delete_event_get_create_event(REPLAY_EDGE_DELETE_EVENT(event));
    ret = add_edge(self,
                   replay_edge_create_event_get_directed(edge_create),
                   replay_edge_create_event_get_id(edge_create),
                   replay_edge_create_event_get_tail(edge_create),
                   replay_edge_create_event_get_head(edge_create),
                   replay_edge_create_event_get_props(edge_create),
                   &error);
  } else if (REPLAY_IS_ACTIVITY_START_EVENT(event)) {
    ReplayActivityStartEvent *act_start = REPLAY_ACTIVITY_START_EVENT(event);
    remove_activity_node_with_name(self,
                                   replay_activity_start_event_get_node(act_start),
                                   replay_activity_start_event_get_id(act_start));
    ret = TRUE;
  } else if (REPLAY_IS_ACTIVITY_END_EVENT(event)) {
    ReplayActivityStartEvent *act_start = replay_activity_end_event_get_start_event(REPLAY_ACTIVITY_END_EVENT(event));
    add_activity_node_with_name(self,
                                replay_activity_start_event_get_node(act_start),
                                replay_activity_start_event_get_id(act_start),
                                replay_activity_start_event_get_props(act_start));
    ret = TRUE;
  } else if (REPLAY_IS_NODE_PROPS_EVENT(event)) {
    /* set properties in node tree and will respond to change */
    ReplayNodePropsEvent *node_props = REPLAY_NODE_PROPS_EVENT(event);
    unset_node_properties(self,
                          replay_node_props_event_get_id(node_props),
                          replay_node_props_event_get_props(node_props));
    ret = TRUE;
  } else if (REPLAY_IS_EDGE_PROPS_EVENT(event)) {
    ReplayEdgePropsEvent *edge_props = REPLAY_EDGE_PROPS_EVENT(event);
    unset_edge_properties(self,
                          replay_edge_props_event_get_id(edge_props),
                          replay_edge_props_event_get_props(edge_props));
    ret = TRUE;
  } else if (REPLAY_IS_MSG_SEND_EVENT(event) ||
             REPLAY_IS_MSG_RECV_EVENT(event)) {
    /* don't set label - will use prev events instead */
    ret = TRUE;
  } else {
    ret = TRUE;
  }
  if (!ret)
  {
    display_error_for_event(self, error, event);
    g_clear_error(&error);
  }
  g_mutex_unlock(&priv->lock);
  g_object_unref(event);

  /* since we are going backwards we need to label things as though we just
   * processed the previous event - use prev_event instead of event */
  event = replay_event_store_get_event(priv->event_store, prev_iter);
  if (REPLAY_IS_MSG_SEND_EVENT(event) ||
      REPLAY_IS_MSG_RECV_EVENT(event))
  {
    label_from_event(self, event);
  }
  g_object_unref(event);
}

static void process_event_forward(ReplayGraphView *self)
{
  ReplayGraphViewPrivate *priv;
  gboolean ret = FALSE;
  GError *error = NULL;
  ReplayEvent *event = NULL;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  g_assert(priv->forward);

  event = replay_event_store_get_event(priv->event_store, priv->iter);

  g_mutex_lock(&priv->lock);

  if (REPLAY_IS_NODE_CREATE_EVENT(event)) {
    ReplayNodeCreateEvent *node_create = REPLAY_NODE_CREATE_EVENT(event);
    ret = (add_node_with_name(self,
                              replay_node_create_event_get_id(node_create),
                              &error) != NULL);
    set_node_properties(self, replay_node_create_event_get_id(node_create),
                        replay_node_create_event_get_props(node_create));
  } else if (REPLAY_IS_NODE_DELETE_EVENT(event)) {
    ret = remove_node_with_name(self,
                                replay_node_delete_event_get_id(REPLAY_NODE_DELETE_EVENT(event)),
                                &error);
  } else if (REPLAY_IS_EDGE_CREATE_EVENT(event)) {
    ReplayEdgeCreateEvent *edge_create = REPLAY_EDGE_CREATE_EVENT(event);
    ret = add_edge(self,
                   replay_edge_create_event_get_directed(edge_create),
                   replay_edge_create_event_get_id(edge_create),
                   replay_edge_create_event_get_tail(edge_create),
                   replay_edge_create_event_get_head(edge_create),
                   replay_edge_create_event_get_props(edge_create),
                   &error);
  } else if (REPLAY_IS_EDGE_DELETE_EVENT(event)) {
    ReplayEdgeCreateEvent *edge_create = replay_edge_delete_event_get_create_event(REPLAY_EDGE_DELETE_EVENT(event));
    ret = remove_edge(self,
                      replay_edge_create_event_get_directed(edge_create),
                      replay_edge_create_event_get_id(edge_create),
                      replay_edge_create_event_get_tail(edge_create),
                      replay_edge_create_event_get_head(edge_create),
                      &error);
  } else if (REPLAY_IS_MSG_SEND_EVENT(event) ||
             REPLAY_IS_MSG_RECV_EVENT(event)) {
    label_from_event(self, event);
    ret = TRUE;
  } else if (REPLAY_IS_ACTIVITY_START_EVENT(event)) {
    ReplayActivityStartEvent *act_start = REPLAY_ACTIVITY_START_EVENT(event);
    add_activity_node_with_name(self,
                                replay_activity_start_event_get_node(act_start),
                                replay_activity_start_event_get_id(act_start),
                                replay_activity_start_event_get_props(act_start));
    ret = TRUE;
  } else if (REPLAY_IS_ACTIVITY_END_EVENT(event)) {
    ReplayActivityStartEvent *act_start = replay_activity_end_event_get_start_event(REPLAY_ACTIVITY_END_EVENT(event));
    remove_activity_node_with_name(self,
                                   replay_activity_start_event_get_node(act_start),
                                   replay_activity_start_event_get_id(act_start));
    ret = TRUE;
  } else if (REPLAY_IS_NODE_PROPS_EVENT(event)) {
    ReplayNodePropsEvent *node_props = REPLAY_NODE_PROPS_EVENT(event);
    set_node_properties(self,
                        replay_node_props_event_get_id(node_props),
                        replay_node_props_event_get_props(node_props));
    ret = TRUE;
  } else if (REPLAY_IS_EDGE_PROPS_EVENT(event)) {
    ReplayEdgePropsEvent *edge_props = REPLAY_EDGE_PROPS_EVENT(event);
    set_edge_properties(self,
                        replay_edge_props_event_get_id(edge_props),
                        replay_edge_props_event_get_props(edge_props));
    ret = TRUE;
  } else {
    ret = FALSE;
  }
  if (!ret)
  {
    display_error_for_event(self, error, event);
    g_clear_error(&error);
  }
  g_mutex_unlock(&priv->lock);
  g_object_unref(event);
}

/* should be called from an idle callback to process events in batches until we
 * reach current in event store to make sure we don't block main loop for too
 * long */
static gboolean _process_events(gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  GtkTreeIter iter;
  GTimer *timer;
  gboolean ret = FALSE;
  gboolean current;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);

  self = REPLAY_GRAPH_VIEW(data);
  priv = self->priv;

  if (priv->iter == NULL)
  {
    GtkTreePath *path;
    GtkTreeIter first;

    /* get first event in event store */
    path = gtk_tree_path_new_from_string("0:0");
    ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                  &first, path);
    gtk_tree_path_free(path);
    g_assert(ret);

    /* set iter from this if we haven't got it yet */
    priv->iter = gtk_tree_iter_copy(&first);
    priv->forward = TRUE;
    process_event_forward(self);
  }

  current = replay_event_store_get_current(priv->event_store, &iter);

  timer = g_timer_new();
  /* don't block main loop for more than 20ms */
  while (current && g_timer_elapsed(timer, NULL) < 0.20)
  {
    GtkTreePath *old_path, *new_path;
    int i;
    old_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                       priv->iter);
    new_path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                       &iter);

    /* if new_path has a different index than old_path then clear labels */
    if (gtk_tree_path_get_indices(old_path)[0] !=
        gtk_tree_path_get_indices(new_path)[0])
    {
      /* free any existing labelled edges or nodes */
      g_list_foreach(priv->labelled_edges, free_edge_event_label, NULL);
      g_list_free(priv->labelled_edges);
      priv->labelled_edges = NULL;

      g_list_foreach(priv->labelled_nodes, free_node_event_label, NULL);
      g_list_free(priv->labelled_nodes);
      priv->labelled_nodes = NULL;
    }

    i = gtk_tree_path_compare(new_path, old_path);
    if (i == -1)
    {
      GtkTreeIter prev_iter;
      priv->forward = FALSE;
      /* new_path comes before old_path - we have gone backwards get the
       * previous one so we can pass to process_event_backwards */
      ret = gtk_tree_path_prev(old_path);
      if (ret)
      {
        /* reset iter */
        ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                      &prev_iter,
                                      old_path);
      }
      if (!ret)
      {
        /* there is no event as this level - go last event at prev level */
        int n;
        GtkTreeIter parent;
        gtk_tree_path_up(old_path);
        gtk_tree_path_prev(old_path);
        ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                      &parent,
                                      old_path);
        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->event_store),
                                           &parent);
        gtk_tree_path_append_index(old_path, n - 1);
        ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                      &prev_iter,
                                      old_path);
      }
      g_assert(ret);
      /* process the current event backwards first */
      process_event_backward(self, &prev_iter);
      /* now set priv->iter */
      gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                              priv->iter,
                              old_path);
      gtk_tree_path_free(old_path);
      gtk_tree_path_free(new_path);
      /* call idle callback again since not yet at current */
      ret = TRUE;
    }
    else if (i == 0)
    {
      gtk_tree_path_free(old_path);
      gtk_tree_path_free(new_path);
      /* stop processing since have reached current */
      ret = FALSE;
      priv->processing_id = 0;
      gtk_tree_path_free(priv->processing_start);
      gtk_tree_path_free(priv->processing_end);
      g_signal_emit(self, signals[PROCESSING], 0, FALSE);
      break;
    }
    else
    {
      priv->forward = TRUE;
      /* old path comes before new path - go forwards to next event */
      gtk_tree_path_next(old_path);
      /* reset iter */
      ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                    priv->iter,
                                    old_path);
      if (!ret)
      {
        /* there is no event as this level - go to next level */
        gtk_tree_path_up(old_path);
        gtk_tree_path_next(old_path);
        gtk_tree_path_down(old_path);
        ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->event_store),
                                      priv->iter,
                                      old_path);
      }
      g_assert(ret);
      process_event_forward(self);
      gtk_tree_path_free(old_path);
      gtk_tree_path_free(new_path);
      /* call idle callback again since not yet at current */
      ret = TRUE;
    }
  }

  /* if we are finished resume animation - but only if we have something worth
   * seeing */
  if (!ret && priv->visible_nodes->len > 0)
  {
    replay_graph_view_run_animation(self);
  }
  else
  {
    replay_graph_view_stop_animation(self);
    /* do a single update to show that we are processing */
    replay_graph_view_render_graph(self);
  }
  g_timer_destroy(timer);

  return ret;
}

static void replay_graph_view_process_events(ReplayGraphView *self,
                                          GtkTreeIter *iter)
{
  ReplayGraphViewPrivate *priv;
  gboolean ret;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));
  g_return_if_fail(iter != NULL);

  priv = self->priv;

  stop_idle_processing(self);

  /* get start and end indices so we can track progress */
  if (priv->iter)
  {
    priv->processing_start = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                                     priv->iter);
  }
  else
  {
    priv->processing_start = gtk_tree_path_new_from_string("0:0");
  }
  priv->processing_end = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->event_store),
                                                 iter);

  /* process some events directly now whilst we are still doing layout */
  g_signal_emit(self, signals[PROCESSING], 0, TRUE);
  ret = _process_events(self);

  /* if still have more events set up idle callback to process events */
  if (ret)
  {
    /* stop animating while processing events since is unneeded CPU load */
    replay_graph_view_stop_animation(self);

    priv->processing_id = gdk_threads_add_idle(_process_events,
                                               self);
  }
}


GtkWidget *replay_graph_view_new(void)
{
  return GTK_WIDGET(g_object_new(REPLAY_TYPE_GRAPH_VIEW, NULL));
}


/* Respond to row_inserted signals in the ReplayNodeTree */
static void row_inserted_cb(GtkTreeModel *node_tree,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gchar *name = NULL;
  gboolean render = FALSE;
  ReplayGraphViewNode *node = NULL;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(data));
  self = REPLAY_GRAPH_VIEW(data);

  priv = self->priv;

  g_return_if_fail(REPLAY_IS_NODE_TREE(node_tree) &&
                   priv->node_tree == REPLAY_NODE_TREE(node_tree));

  gtk_tree_model_get(node_tree,
                     iter,
                     REPLAY_NODE_TREE_COL_ID, &name,
                     -1);

  /* update existing information for an node */
  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, name);
  g_free(name);
  if (node != NULL)
  {
    ReplayGraphViewNode *parent = NULL;
    GtkTreePath *parent_path;

    /* if this row has a parent, set it */
    parent_path = gtk_tree_path_copy(path);
    if (gtk_tree_path_up(parent_path) &&
        gtk_tree_path_get_depth(parent_path) > 0)
    {
      gchar *parent_name;
      GtkTreeIter parent_iter;

      if (!gtk_tree_model_get_iter(node_tree, &parent_iter, parent_path))
      {
        g_assert_not_reached();
      }

      gtk_tree_model_get(node_tree,
                         &parent_iter,
                         REPLAY_NODE_TREE_COL_ID, &parent_name,
                         -1);
      /* create parent if doesn't already exist */
      parent = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table,
                                                       parent_name);
      if (parent == NULL)
      {
        g_mutex_lock(&priv->lock);
        parent = add_node_with_name(self, parent_name, NULL);
        g_mutex_unlock(&priv->lock);
      }
      g_assert(parent != NULL);
      g_assert(parent->group);
      g_free(parent_name);
    }
    /* update parent and child relationships */
    g_mutex_lock(&priv->lock);
    reparent_node(self, node, parent);
    g_mutex_unlock(&priv->lock);
    gtk_tree_path_free(parent_path);

    render = TRUE;
  }

  /* schedule a redraw if needed */
  if (render)
  {
    replay_graph_view_render_ratelimit(self);
  }
}

/* respond to changes in rows */
static void row_changed_cb(GtkTreeModel *node_tree,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gboolean hidden;
  gchar *name = NULL, *color = NULL, *label;
  ReplayGraphViewNode *node;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(data));
  self = REPLAY_GRAPH_VIEW(data);

  priv = self->priv;

  gtk_tree_model_get(node_tree,
                     iter,
                     REPLAY_NODE_TREE_COL_ID, &name,
                     REPLAY_NODE_TREE_COL_LABEL, &label,
                     REPLAY_NODE_TREE_COL_COLOR, &color,
                     REPLAY_NODE_TREE_COL_HIDDEN, &hidden,
                     -1);

  /* set hidden property */
  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, name);
  g_free(name);
  if (node != NULL)
  {
    if (node->hidden != hidden)
    {
      /* node->parent overrides node->hidden property - if node->parent
       * != NULL then node is always hidden as far as we are concerned */
      if (node->parent == NULL)
      {
        g_mutex_lock(&priv->lock);
        /* move from hidden to visible array or vice-versa */
        if (hidden)
        {
          /* move to hidden list */
          remove_node_from_visible_array(self, node);
          add_node_to_hidden_array(self, node);
        }
        else
        {
          /* move to visible list */
          remove_node_from_hidden_array(self, node);
          add_node_to_visible_array(self, node);
        }
        g_mutex_unlock(&priv->lock);
      }
      node->hidden = hidden;
    }

    g_free(node->label);
    node->label = label;
    parse_color_to_flv(color, node->color);
  }
  g_free(color);
}

/* used to implement the GtkTreeModelFilter which does completion for the search
 * entry - only show unique rows */
static gboolean node_tree_row_visible_func(GtkTreeModel *model,
                                           GtkTreeIter *iter,
                                           gpointer data)
{
  ReplayGraphView *self;
  ReplayGraphViewPrivate *priv;
  gchar *name = NULL;
  ReplayGraphViewNode *node;
  gboolean visible = FALSE;

  g_return_val_if_fail(REPLAY_IS_GRAPH_VIEW(data), FALSE);
  self = REPLAY_GRAPH_VIEW(data);

  priv = self->priv;

  g_return_val_if_fail(GTK_TREE_MODEL(priv->node_tree) == model, FALSE);

  gtk_tree_model_get(model, iter,
                     REPLAY_NODE_TREE_COL_ID, &name,
                     -1);

  node = (ReplayGraphViewNode *)g_hash_table_lookup(priv->node_table, name);
  g_free(name);
  if (node)
  {
    visible = node_is_in_array(node, priv->visible_nodes);
    if (visible)
    {
      /* see if we are a unique label */
      guint i;
      gboolean match = FALSE;

      /* only search rest of visible nodes since GtkTreeModelFilter will
       * already have checked the ones which come before us */
      for (i = node->index + 1; i < priv->visible_nodes->len && !match; i++)
      {
        ReplayGraphViewNode *node2 =
          (ReplayGraphViewNode *)g_ptr_array_index(priv->visible_nodes, i);
        match = (g_ascii_strcasecmp(node->label, node2->label) == 0);
      }
      visible = !match;
    }
  }
  return visible;
}

void replay_graph_view_set_data(ReplayGraphView *self,
                             ReplayEventStore *event_store,
                             ReplayNodeTree *node_tree)
{
  ReplayGraphViewPrivate *priv;
  GtkTreeIter iter;
  gboolean current;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  replay_graph_view_unset_data(self);

  priv->node_tree = g_object_ref(node_tree);
  priv->row_inserted_id = g_signal_connect(priv->node_tree,
                                           "row-inserted",
                                           G_CALLBACK(row_inserted_cb),
                                           self);
  priv->row_changed_id = g_signal_connect(priv->node_tree,
                                          "row-changed",
                                          G_CALLBACK(row_changed_cb),
                                          self);

  priv->event_store = g_object_ref(event_store);
  priv->event_inserted_id = g_signal_connect_swapped(priv->event_store,
                                                     "row-inserted",
                                                     G_CALLBACK(replay_graph_view_render_ratelimit),
                                                     self);
  priv->current_changed_id = g_signal_connect_swapped(priv->event_store,
                                                      "current-changed",
                                                      G_CALLBACK(replay_graph_view_process_events),
                                                      self);

  /* use a filtered version of node tree where we only have visible rows,
   * using path as completion */
  priv->search_completion = gtk_entry_completion_new();
  priv->visible_nodes_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->node_tree),
                                                        NULL);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(priv->visible_nodes_model),
                                         node_tree_row_visible_func,
                                         self,
                                         NULL);

  gtk_entry_completion_set_model(priv->search_completion,
                                 priv->visible_nodes_model);
  gtk_entry_completion_set_text_column(priv->search_completion,
                                       REPLAY_NODE_TREE_COL_LABEL);
  gtk_entry_set_completion(GTK_ENTRY(priv->search_entry),
                           priv->search_completion);

  /* process initial event - this will also set priv->iter for us */
  current = replay_event_store_get_current(priv->event_store, &iter);
  if (current)
  {
    replay_graph_view_process_events(self, &iter);
  }

  /* do a single draw to refresh screen - don't run yet - wait till we've
   * actually got something to display */
  replay_graph_view_render_graph(self);
}

void replay_graph_view_set_label_nodes(ReplayGraphView *self,
                                    gboolean label_nodes)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->label_nodes != label_nodes)
  {
    priv->label_nodes = label_nodes;
  }
}

void replay_graph_view_set_label_edges(ReplayGraphView *self,
                                    gboolean label_edges)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->label_edges != label_edges)
  {
    priv->label_edges = label_edges;
  }
}

void replay_graph_view_set_absolute_time(ReplayGraphView *self,
                                      gboolean absolute_time)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->absolute_time != absolute_time)
  {
    priv->absolute_time = absolute_time;
    replay_graph_view_render_ratelimit(self);
  }
}

void replay_graph_view_set_show_hud(ReplayGraphView *self,
                                 gboolean show_hud)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->show_hud != show_hud)
  {
    priv->show_hud = show_hud;
    replay_graph_view_render_ratelimit(self);
  }
}

void replay_graph_view_set_two_dimensional(ReplayGraphView *self,
                                        gboolean two_dimensional)
{
  ReplayGraphViewPrivate *priv;

  g_return_if_fail(REPLAY_IS_GRAPH_VIEW(self));

  priv = self->priv;

  if (priv->two_dimensional != two_dimensional)
  {
    priv->two_dimensional = two_dimensional;
    if (priv->two_dimensional)
    {
      /* reset rotation matrix */
      memset(priv->rotation, 0.0, sizeof(priv->rotation));
      priv->rotation[0] = 1.0;
      priv->rotation[5] = 1.0;
      priv->rotation[10] = 1.0;
      priv->rotation[15] = 1.0;
    }
    replay_graph_view_render_ratelimit(self);
  }
}
