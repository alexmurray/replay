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

#include <string.h>
#include <replay/replay-interval-list.h>
#include <replay/replay-timestamp.h>
#include "replay-marshallers.h"

#define REPLAY_INTERVAL_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), REPLAY_TYPE_INTERVAL_LIST, ReplayIntervalListPrivate))

/* signals we emit */
enum
{
  INTERVAL_INSERTED = 0,
  INTERVAL_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* function definitions */
static void replay_interval_list_class_init(ReplayIntervalListClass *klass);

static void replay_interval_list_init(ReplayIntervalList *self);

static void replay_interval_list_finalize(GObject *object);

/* intervals of type NODE_EXISTS, NODE_ACTIVE and NODE_WAITING are expected to
 * span a given time, and so in this case if start and end are the same, these
 * are special, since for a given search, we need to return all those which
 * start / end before / after the interval period. So store all intervals which
 * aren't actually a real interval (ie have start == end) in a separate array
 * from those which have start != end so we can easily return these */
enum
{
  SAME = 0,
  DIFFERENT = 1,
};

struct _ReplayIntervalListPrivate
{
  GPtrArray *start_intervals[REPLAY_EVENT_INTERVAL_NUM_TYPES][2];
  GPtrArray *end_intervals[REPLAY_EVENT_INTERVAL_NUM_TYPES][2];

  /* make MT safe */
  GMutex mutex[REPLAY_EVENT_INTERVAL_NUM_TYPES][2];

  /* do we need sorting? */
  gboolean dirty[REPLAY_EVENT_INTERVAL_NUM_TYPES][2];

  /* the child event list which our intervals refer to */
  ReplayEventStore *event_store;
};

/* define ReplayIntervalList as inheriting from GObject */
G_DEFINE_TYPE(ReplayIntervalList, replay_interval_list, G_TYPE_OBJECT);

struct _ReplayEventInterval
{
  /* type of interval */
  ReplayEventIntervalType type;

  /* the GtkTreeIter's for this interval */
  GtkTreeIter start;
  gint64 start_usecs;
  GtkTreeIter end;
  gint64 end_usecs;
  GMutex mutex;
  int refcount;
};

static void replay_interval_list_class_init(ReplayIntervalListClass *klass)
{
  GObjectClass *obj_class;

  obj_class = G_OBJECT_CLASS(klass);

  /* override parent methods */
  obj_class->finalize = replay_interval_list_finalize;

  /* install class signals */
  signals[INTERVAL_INSERTED] = g_signal_new(
                                            "interval-inserted",
                                            G_OBJECT_CLASS_TYPE(obj_class),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET(ReplayIntervalListClass, interval_inserted),
                                            NULL, NULL,
                                            g_cclosure_marshal_VOID__POINTER,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_POINTER);

  signals[INTERVAL_CHANGED] = g_signal_new(
                                           "interval-changed",
                                           G_OBJECT_CLASS_TYPE(obj_class),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET(ReplayIntervalListClass, interval_changed),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__POINTER,
                                           G_TYPE_NONE,
                                           1,
                                           G_TYPE_POINTER);

  /* add private data */
  g_type_class_add_private(obj_class, sizeof(ReplayIntervalListPrivate));
}

static void replay_interval_list_init(ReplayIntervalList *self)
{
  ReplayIntervalListPrivate *priv;
  ReplayEventIntervalType type;

  priv = REPLAY_INTERVAL_LIST_GET_PRIVATE(self);

  self->priv = priv;

  for (type = REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS;
       type < REPLAY_EVENT_INTERVAL_NUM_TYPES;
       type++)
  {
    int i;
    for (i = 0; i < 2; i++)
    {
      priv->start_intervals[type][i] = g_ptr_array_new();
      priv->end_intervals[type][i] = g_ptr_array_new();
      g_mutex_init(&priv->mutex[type][i]);
    }
  }
}

static void replay_interval_list_finalize(GObject *object)
{
  ReplayIntervalList *self;
  ReplayIntervalListPrivate *priv;
  ReplayEventIntervalType type;

  g_return_if_fail(REPLAY_IS_INTERVAL_LIST(object));

  self = REPLAY_INTERVAL_LIST(object);
  priv = self->priv;

  for (type = REPLAY_EVENT_INTERVAL_TYPE_NODE_EXISTS;
       type < REPLAY_EVENT_INTERVAL_NUM_TYPES;
       type++)
  {
    int i;
    for (i = 0; i < 2; i++)
    {
      g_mutex_lock(&priv->mutex[type][i]);
      g_ptr_array_foreach(priv->start_intervals[type][i],
                          (GFunc)replay_event_interval_unref,
                          NULL);
      g_ptr_array_free(priv->start_intervals[type][i], TRUE);

      g_ptr_array_foreach(priv->end_intervals[type][i],
                          (GFunc)replay_event_interval_unref,
                          NULL);
      g_ptr_array_free(priv->end_intervals[type][i], TRUE);
      g_mutex_unlock(&priv->mutex[type][i]);
      g_mutex_clear(&priv->mutex[type][i]);
    }
  }
  g_object_unref(priv->event_store);

  /* call parent finalize */
  G_OBJECT_CLASS(replay_interval_list_parent_class)->finalize(object);
}

ReplayIntervalList *replay_interval_list_new(ReplayEventStore *event_store)
{
  ReplayIntervalList *self = g_object_new(REPLAY_TYPE_INTERVAL_LIST, NULL);
  ReplayIntervalListPrivate *priv = self->priv;

  priv->event_store = g_object_ref(event_store);
  return self;
}

/**
 * replay_interval_list_get_event_store:
 * @self: A #ReplayIntervalList
 *
 * Returns: (transfer none): The event store associated with this interval list
 */
ReplayEventStore *replay_interval_list_get_event_store(ReplayIntervalList *self)
{
  g_assert(REPLAY_IS_INTERVAL_LIST(self));

  return self->priv->event_store;
}

static int compare_pintervals_by_start(gconstpointer a,
                                       gconstpointer b)
{
  ReplayEventInterval *a_i = *(ReplayEventInterval **)a;
  ReplayEventInterval *b_i = *(ReplayEventInterval **)b;
  gint64 diff;

  g_mutex_lock(&a_i->mutex);
  g_mutex_lock(&b_i->mutex);
  diff = a_i->start_usecs - b_i->start_usecs;
  g_mutex_unlock(&a_i->mutex);
  g_mutex_unlock(&b_i->mutex);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

static int compare_pintervals_by_end(gconstpointer a,
                                     gconstpointer b)
{
  ReplayEventInterval *a_i = *(ReplayEventInterval **)a;
  ReplayEventInterval *b_i = *(ReplayEventInterval **)b;
  gint64 diff;

  g_mutex_lock(&a_i->mutex);
  g_mutex_lock(&b_i->mutex);
  diff = a_i->end_usecs - b_i->end_usecs;
  g_mutex_unlock(&a_i->mutex);
  g_mutex_unlock(&b_i->mutex);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

static gboolean
iter_equal(const GtkTreeIter *iter1,
           const GtkTreeIter *iter2)
{
  return ((iter1->stamp == iter2->stamp &&
           iter1->user_data == iter2->user_data &&
           iter1->user_data2 == iter2->user_data2 &&
           iter1->user_data3 == iter2->user_data3));
}

void replay_interval_list_add_interval(ReplayIntervalList *self,
                                    ReplayEventInterval *interval)
{
  ReplayIntervalListPrivate *priv;
  guint array = SAME;

  g_assert(REPLAY_IS_INTERVAL_LIST(self));
  g_return_if_fail(REPLAY_IS_INTERVAL_LIST(self));
  g_return_if_fail(interval != NULL);
  g_return_if_fail(interval->start.user_data != NULL);
  g_return_if_fail(interval->end.user_data != NULL);
  g_return_if_fail(interval->type < REPLAY_EVENT_INTERVAL_NUM_TYPES);

  priv = self->priv;

  /* add to arrays - if start != end then add to array DIFFERENT */
  g_mutex_lock(&interval->mutex);
  if (!iter_equal(&interval->start, &interval->end) &&
      interval->start_usecs != interval->end_usecs)
  {
    array = DIFFERENT;
  }
  else
  {
    g_assert(interval->start_usecs == interval->end_usecs);
  }
  g_mutex_unlock(&interval->mutex);
  g_mutex_lock(&priv->mutex[interval->type][array]);
  g_ptr_array_add(priv->start_intervals[interval->type][array],
                  replay_event_interval_ref(interval));
  g_ptr_array_add(priv->end_intervals[interval->type][array],
                  replay_event_interval_ref(interval));
  /* we will need to sort before next lookup */
  priv->dirty[interval->type][array] = TRUE;
  g_mutex_unlock(&priv->mutex[interval->type][array]);

  g_signal_emit(self,
                signals[INTERVAL_INSERTED],
                0,
                interval);
}

void replay_interval_list_set_interval_end(ReplayIntervalList *self,
                                        ReplayEventInterval *interval,
                                        const GtkTreeIter *iter,
                                        gint64 usecs)
{
  ReplayIntervalListPrivate *priv;
  guint old_array = SAME, new_array = SAME;

  g_assert(REPLAY_IS_INTERVAL_LIST(self));
  g_return_if_fail(REPLAY_IS_INTERVAL_LIST(self));
  g_return_if_fail(interval != NULL);
  g_return_if_fail(interval->start.user_data != NULL);
  g_return_if_fail(interval->end.user_data != NULL);
  g_return_if_fail(interval->type < REPLAY_EVENT_INTERVAL_NUM_TYPES);

  priv = self->priv;

  /* add to arrays - if start != end then add to array 1 */
  g_mutex_lock(&interval->mutex);
  if (!iter_equal(&interval->start, &interval->end) &&
      interval->start_usecs != interval->end_usecs)
  {
    old_array = DIFFERENT;
  }
  if (!iter_equal(&interval->start, iter) && interval->start_usecs != usecs)
  {
    new_array = DIFFERENT;
  }
  g_mutex_unlock(&interval->mutex);

  /* if we have changed array, then need to move first */
  if (old_array != new_array)
  {
    gboolean ret;
    g_mutex_lock(&priv->mutex[interval->type][new_array]);
    g_mutex_lock(&priv->mutex[interval->type][old_array]);
    ret = g_ptr_array_remove(priv->start_intervals[interval->type][old_array],
                             interval);
    ret = g_ptr_array_remove(priv->end_intervals[interval->type][old_array],
                             interval);
    g_assert(ret);
    g_ptr_array_add(priv->start_intervals[interval->type][new_array],
                    interval);
    g_ptr_array_add(priv->end_intervals[interval->type][new_array],
                    interval);
    g_mutex_unlock(&priv->mutex[interval->type][old_array]);
  }

  g_mutex_lock(&interval->mutex);

  interval->end = *iter;
  interval->end_usecs = usecs;

  g_mutex_unlock(&interval->mutex);

  priv->dirty[interval->type][new_array] = TRUE;

  g_mutex_unlock(&priv->mutex[interval->type][new_array]);

  g_signal_emit(self,
                signals[INTERVAL_CHANGED],
                0,
                interval);
}

typedef enum
{
  COMPARE_START_TO_KEY,
  COMPARE_END_TO_KEY
} CompareType;

/*
 * returns 0 if start time of interval is equal to key, +1 if interval start is
 * greater than key, and -1 if is less than key
 */
static int compare_interval_start_to_key(ReplayEventInterval *interval,
                                         const gint64 *key)
{
  gint64 diff;

  g_mutex_lock(&interval->mutex);
  diff = interval->start_usecs - *key;
  g_mutex_unlock(&interval->mutex);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

/*
 * returns 0 if end time of interval is equal to key, +1 if interval end is
 * greater than key, and -1 if is less than key
 */
static int compare_interval_end_to_key(ReplayEventInterval *interval,
                                       const gint64 *key)
{
  gint64 diff;

  g_mutex_lock(&interval->mutex);
  diff = interval->end_usecs - *key;
  g_mutex_unlock(&interval->mutex);
  return (diff > 0 ? 1 : (diff < 0 ? -1 : 0));
}

/*
 * Binary searches the array using the given key and compare function - returns
 * the index of the matching element - if none is found, we return the last
 * checked element (which will be the closest one) in pclosest
 */
static gint replay_interval_list_search(ReplayEventInterval **array,
                                     gint array_len,
                                     gint64 key,
                                     CompareType compare,
                                     gint *pclosest)
{
  gint lower = 0;
  gint upper = array_len - 1;
  gint i = -1;
  gint middle = -1;

  while (1)
  {
    int ret;
    /* couldn't find element */
    if (lower > upper)
    {
      i = -1;
      *pclosest = middle;
      break;
    }
    /* avoid potential integer overflow when calculating middle for really
     * high values of upper and lower -
     * http://googleresearch.blogspot.com.au/2006/06/extra-extra-read-all-about-it-nearly.html */
    middle = ((guint)lower + (guint)upper) >> 1;
    if (compare == COMPARE_START_TO_KEY)
    {
      ret = compare_interval_start_to_key((array[middle]), &key);
    }
    else if (compare == COMPARE_END_TO_KEY)
    {
      ret = compare_interval_end_to_key((array[middle]), &key);
    }
    else
    {
      g_assert_not_reached();
    }
    if (ret < 0)
    {
      /* value at middle is less than key, so we move lower up to slot after
       * middle as everything before and including middle is not what we
       * want */
      lower = middle + 1;
    }
    else if (ret == 0)
    {
      /* middle has the index */
      i = middle;
      break;
    }
    else
    {
      /* value at middle is greater than key, so we move upper down to slot
       * before middle as everything after and including middle is not what we
       * want */
      upper = middle - 1;
    }
  }
  return i;
}

/**
 * replay_interval_list_lookup:
 * @self: The #ReplayIntervalList to search
 * @type: The type of intervals to search for
 * @start_usecs: The start time in usecs
 * @end_usecs: The end time in usecs
 * @n: Gets filled in with the number of intervals returned
 *
 * Internally we have 2 arrays for each interval type - one which orders the
 * ReplayEventIntervals in increasing order based on start time, and one which orders
 * them in increasing order based on end time. This way, to search for ALL
 * intervals in a given span (and which span the given span), we return the set
 * of intervals from the start time ordered list which have start time greater
 * than or equal to start of interval and start time less than or equal to the
 * end of the interval, AND the set of intervals from the end time ordered list
 * which have end time greater than or equal to start of interval and less than
 * or equal to the end of interval. We also want all intervals which span OVER
 * the given time span (ie. which start before the given start and end after the
 * given end). This is a little trickier, but not too hard - once we find the
 * first interval which starts just after the start, we get all the elements
 * before this one from the start ordered array, and sort them based upon END
 * time - we then add to our list those which have end time greater than the
 * search interval end time. Conversely we could do the same for the end time
 * array, so we can do either - so we pick the one of the two arrays which has
 * the least remaining elements to sort and then search. This works for the
 * intervals which have different start and end times - for those with same
 * start and end times, also need to return all intervals which start before the
 * end of the interval
 *
 * Returns: (element-type ReplayEventInterval) (transfer full): A #GList of
 * #ReplayEventInterval pointers contained between start and end
 */
GList *replay_interval_list_lookup(ReplayIntervalList *self,
                                ReplayEventIntervalType type,
                                gint64 start_usecs,
                                gint64 end_usecs,
                                guint *n)
{
  ReplayIntervalListPrivate *priv;
  gint i, closest;
  GList *matches = NULL;
  GHashTable *unique_table;
  gint start_len = 0, end_len = 0;
  guint array;
  guint num_intervals = 0;

  g_return_val_if_fail(type < REPLAY_EVENT_INTERVAL_NUM_TYPES, matches);
  g_return_val_if_fail(REPLAY_IS_INTERVAL_LIST(self), matches);

  priv = self->priv;

  /* make sure we only return unique intervals in the list */
  unique_table = g_hash_table_new(g_direct_hash,
                                  g_direct_equal);


  for (array = 0; array < 2; array++)
  {
    g_mutex_lock(&priv->mutex[type][array]);

    if (priv->dirty[type][array])
    {
      /* need to use the pintervals version of the compare functions since
         g_ptr_array_sort passes POINTERS to pointers within the array */
      g_ptr_array_sort(priv->start_intervals[type][array],
                       compare_pintervals_by_start);
      g_ptr_array_sort(priv->end_intervals[type][array],
                       compare_pintervals_by_end);
      priv->dirty[type][array] = FALSE;
    }

    /* do search on both SAME and DIFFERENT arrays if type is MESSAGE_PASS,
     * otherwise only do this search on DIFFERENT array */
    if (type == REPLAY_EVENT_INTERVAL_TYPE_MESSAGE_PASS ||
        array == DIFFERENT)
    {
      /* do binary search on each array to find match - if we can't match we get
       * back the lower and upper indexes of the last search - if we are
       * searching for */

      /* in this case find the closest element to the start time of the interval
       * - this is our start_index */
      i = replay_interval_list_search((ReplayEventInterval **)(priv->start_intervals[type][array]->pdata),
                                   priv->start_intervals[type][array]->len,
                                   start_usecs,
                                   COMPARE_START_TO_KEY,
                                   &closest);

      if (i == -1)
      {
        i = closest;
      }

      if (i != -1)
      {
        int start_index = i;
        /* now find last entry in start_intervals which starts before the end
         * time */
        int end_index = replay_interval_list_search((ReplayEventInterval **)(priv->start_intervals[type][array]->pdata),
                                                 priv->start_intervals[type][array]->len,
                                                 end_usecs,
                                                 COMPARE_START_TO_KEY,
                                                 &closest);
        if (end_index == -1)
        {
          end_index = closest;
        }
        g_assert(end_index != -1 && start_index <= end_index);
        /* add all intervals between these two indices */
        for (i = start_index; i <= end_index; i++)
        {
          ReplayEventInterval *interval =
            (ReplayEventInterval *)g_ptr_array_index(priv->start_intervals[type][array], i);
          if (!g_hash_table_lookup(unique_table, interval))
          {
            g_hash_table_insert(unique_table, interval, interval);
            matches = g_list_prepend(matches, replay_event_interval_ref(interval));
            num_intervals++;
          }
        }
        /* now we need to record the number of elements left *before* start_index
         * so we can later pick which list to search for remainder elements */
        start_len = start_index;
      }

      /* now find the closest element in end_intervals to the start time of the
       * interval - this is our start_index */
      i = replay_interval_list_search((ReplayEventInterval **)(priv->end_intervals[type][array]->pdata),
                                   priv->end_intervals[type][array]->len,
                                   start_usecs,
                                   COMPARE_END_TO_KEY,
                                   &closest);

      if (i == -1)
      {
        i = closest;
      }

      if (i != -1)
      {
        int start_index = i;
        /* now find last entry in end_intervals which ends before the end
         * time */
        int end_index = replay_interval_list_search((ReplayEventInterval **)(priv->end_intervals[type][array]->pdata),
                                                 priv->end_intervals[type][array]->len,
                                                 end_usecs,
                                                 COMPARE_END_TO_KEY,
                                                 &closest);
        if (end_index == -1)
        {
          end_index = closest;
        }
        g_assert(end_index != -1 && start_index <= end_index);
        /* add all intervals between these two indices */
        for (i = start_index; i <= end_index; i++)
        {
          ReplayEventInterval *interval =
            (ReplayEventInterval *)g_ptr_array_index(priv->end_intervals[type][array], i);
          if (!g_hash_table_lookup(unique_table, interval))
          {
            g_hash_table_insert(unique_table, interval, interval);
            matches = g_list_prepend(matches, replay_event_interval_ref(interval));
            num_intervals++;
          }
        }

        /* now we need to record the number of elements left *after* end_index
         * so we can later pick which list to search for remainder elements */
        end_len = priv->end_intervals[type][array]->len - 1 - end_index;
      }

      /* now we need to find the elements which come before the start of the
       * interval and after the end of the interval - ie. those in start_len which
       * have end time greater than end of interval or vice-versa  */
      if (start_len > 0 && start_len < end_len)
      {
        /* check those in start_len */
        g_assert(start_len < (gint)priv->start_intervals[type][array]->len);
        for (i = 0; i < start_len; i++)
        {
          ReplayEventInterval *interval = (ReplayEventInterval *)(priv->start_intervals[type][array]->pdata[i]);
          g_assert(compare_interval_start_to_key(interval, &start_usecs) <= 0);
          if (compare_interval_end_to_key(interval, &end_usecs) > 0)
          {
            if (!g_hash_table_lookup(unique_table, interval))
            {
              g_hash_table_insert(unique_table, interval, interval);
              matches = g_list_prepend(matches, replay_event_interval_ref(interval));
              num_intervals++;
            }
          }
        }
      }
      else if (end_len > 0)
      {
        /* check those in end_len */
        g_assert(end_len < (gint)priv->end_intervals[type][array]->len);
        for (i = priv->end_intervals[type][array]->len - end_len;
             i < (gint)priv->end_intervals[type][array]->len;
             i++)
        {
          ReplayEventInterval *interval = (ReplayEventInterval *)(priv->end_intervals[type][array]->pdata[i]);
          g_assert(compare_interval_end_to_key(interval, &end_usecs) >= 0);
          if (compare_interval_start_to_key(interval, &start_usecs) < 0)
          {
            if (!g_hash_table_lookup(unique_table, interval))
            {
              g_hash_table_insert(unique_table, interval, interval);
              matches = g_list_prepend(matches, replay_event_interval_ref(interval));
              num_intervals++;
            }
          }
        }
      }
    }
    else
    {
      /* have a SAME array for non MESSAGE_PASS type - in this case we have a
       * single event for an interval which is expected to span - but we don't
       * know if start defines the start or defines the end of the interval -
       * hence we have to return all intervals: TODO: when adding a new interval
       * should only specify start or end then see if we can handle them
       * better */
      for (i = 0; i < (gint)priv->start_intervals[type][array]->len; i++)
      {
        ReplayEventInterval *interval = (ReplayEventInterval *)(priv->start_intervals[type][array]->pdata[i]);
        if (!g_hash_table_lookup(unique_table, interval))
        {
          g_hash_table_insert(unique_table, interval, interval);
          matches = g_list_prepend(matches, replay_event_interval_ref(interval));
          num_intervals++;
        }
      }
    }
    g_mutex_unlock(&priv->mutex[type][array]);
  }

  g_hash_table_destroy(unique_table);
  if (n != NULL)
  {
    *n = num_intervals;
  }

  /* try to return in-order */
  matches = g_list_reverse(matches);
  return matches;
}

guint replay_interval_list_num_intervals(ReplayIntervalList *self,
                                      ReplayEventIntervalType type)
{
  ReplayIntervalListPrivate *priv;
  guint n = 0;

  g_return_val_if_fail(REPLAY_IS_INTERVAL_LIST(self), n);
  g_return_val_if_fail(type < REPLAY_EVENT_INTERVAL_NUM_TYPES, n);

  priv = self->priv;
  n = priv->start_intervals[type][SAME]->len +
    priv->start_intervals[type][DIFFERENT]->len;
  return n;
}

ReplayEventInterval *replay_event_interval_ref(ReplayEventInterval *interval)
{
  interval->refcount++;
  return interval;
}

#define REPLAY_TYPE_EVENT_INTERVAL			(replay_event_interval_get_type ())
#define REPLAY_EVENT_INTERVAL(obj)			((ReplayEventInterval *) (obj))

/**
 * replay_event_interval_get_type:
 *
 * Retrieves the #GType object which is associated with the #ReplayEventInterval
 * class.
 *
 * Return value: the GType associated with #ReplayEventInterval.
 **/
GType
replay_event_interval_get_type(void)
{
  static GType the_type = 0;

  if (G_UNLIKELY(!the_type))
  {
    the_type = g_boxed_type_register_static("ReplayEventInterval",
                                            (GBoxedCopyFunc)replay_event_interval_ref,
                                            (GBoxedFreeFunc)replay_event_interval_unref);
  }
  return the_type;
}

/**
 * replay_event_interval_new:
 * @type: The type of this interval
 * @start: An interator referencing the #ReplayEvent which starts this interval
 * @start_usecs: The time which the start event occurs
 * @end: An interator referencing the #ReplayEvent which ends this interval
 * @end_usecs: The time which the end event occurs
 *
 * Creates a new ReplayEventInterval with refcount of 1 - caller must call
 * unref_event_interval when done
 */
ReplayEventInterval *replay_event_interval_new(ReplayEventIntervalType type,
                                         const GtkTreeIter *start,
                                         gint64 start_usecs,
                                         const GtkTreeIter *end,
                                         gint64 end_usecs)
{
  ReplayEventInterval *interval = g_slice_new0(ReplayEventInterval);
  interval->type = type;
  interval->start = *start;
  interval->start_usecs = start_usecs;
  interval->end = *end;
  interval->end_usecs = end_usecs;
  g_mutex_init(&interval->mutex);
  return replay_event_interval_ref(interval);
}

void replay_event_interval_unref(ReplayEventInterval *interval)
{
  g_assert(interval->refcount > 0);
  interval->refcount--;
  if (interval->refcount == 0)
  {
    g_mutex_clear(&interval->mutex);
    g_slice_free(ReplayEventInterval, interval);
  }
}

ReplayEventIntervalType replay_event_interval_get_interval_type(ReplayEventInterval *interval)
{
  ReplayEventIntervalType type;

  g_mutex_lock(&interval->mutex);
  type = interval->type;
  g_mutex_unlock(&interval->mutex);

  return type;
}

void replay_event_interval_get_start(ReplayEventInterval *interval,
                                  GtkTreeIter *start,
                                  gint64 *start_usecs)
{
  g_mutex_lock(&interval->mutex);
  if (start)
  {
    *start = interval->start;
  }
  if (start_usecs)
  {
    *start_usecs = interval->start_usecs;
  }
  g_mutex_unlock(&interval->mutex);
}

void replay_event_interval_get_end(ReplayEventInterval *interval,
                                GtkTreeIter *end,
                                gint64 *end_usecs)
{
  g_mutex_lock(&interval->mutex);
  if (end)
  {
    *end = interval->end;
  }
  if (end_usecs)
  {
    *end_usecs = interval->end_usecs;
  }
  g_mutex_unlock(&interval->mutex);
}
