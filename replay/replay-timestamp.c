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
#include <stdio.h>
#include <glib.h>
#include <replay/replay-timestamp.h>
#include <replay/replay-string-utils.h>

static const gint64 multipliers[REPLAY_NUM_TIME_UNITS] = {
        (gint64)1, /* USECS in USEC */
        (gint64)1000, /* USECS in MSEC */
        (gint64)1000 * 1000, /* USECS in SEC */
        (gint64)60 * 1000 * 1000, /* USECS in MIN */
        (gint64)60 * 60 * 1000 * 1000, /* USECS in HR */
        (gint64)24 * 60 * 60 * 1000 * 1000 /* USECS in DAY */
};

/**
 * replay_timestamp_get_times:
 * @timestamp: A timestamp in microseconds since epoch
 * @times: (out) (allow-none): Filled with the various time components in @timestamp
 *
 * Breaks down @timestamp into it's various components, and returns the
 * maximum component as a return values
 *
 * Returns: The maximum time unit of @timestamp
 */
ReplayTimeUnit replay_timestamp_get_times(gint64 timestamp,
                                           gint64 *out)
{
        ReplayTimeUnit max = REPLAY_TIME_UNIT_MICROSECONDS;
        gint64 times[REPLAY_NUM_TIME_UNITS];
        gint i;

        /* start at top and work our way down */
        for (i = REPLAY_NUM_TIME_UNITS - 1; i >= 0; i--) {
                times[i] = ((timestamp - (timestamp % multipliers[i])) /
                            multipliers[i]);
                timestamp -= times[i] * multipliers[i];
                if (times[i] && max == REPLAY_TIME_UNIT_MICROSECONDS) {
                        max = i;
                }
        }

        if (out) {
                memcpy(out, times, sizeof(times));
        }
        return max;
}

/**
 * replay_timestamp_from_times:
 * @times: Filled with the various time components of a @timestamp
 *
 * Converts a series of timestamp components into a timestamp (as microseconds
 * since the epoch).
 * Returns: A timestamp as microseconds since the epoch
 */
gint64 replay_timestamp_from_times(gint64 *times)
{
        gint64 timestamp = 0;
        ReplayTimeUnit i;

        for (i = 0; i < REPLAY_NUM_TIME_UNITS; i++) {
                timestamp += times[i] * multipliers[i];
        }
        return timestamp;
}

static const gchar *units[REPLAY_NUM_TIME_UNITS] = {
        "us",
        "ms",
        "s",
        "m",
        "h",
        "d",
};

static const gchar *separators[REPLAY_NUM_TIME_UNITS] = {
        "",
        ".",
        ".",
        ":",
        ":",
        ":",
};

static const gint widths[REPLAY_NUM_TIME_UNITS] = {
        3,
        3,
        2,
        2,
        2,
        -1
};

static gchar *
relative_to_string(gint64 times[REPLAY_NUM_TIME_UNITS],
                   ReplayTimeUnit max)
{
        gchar *string;
        ReplayTimeUnit min;
        ReplayTimeUnit i;

        /* find minimum non-zero value in times */
        min = REPLAY_TIME_UNIT_MICROSECONDS;
        while (!times[min] && min < max) {
                min++;
        }
        /* print the first value without trailing separator */
        string = g_strdup_printf("%*" G_GINT64_FORMAT,
                                 widths[max], times[max]);
        /* then print the rest with separator */
        for (i = max - 1; i >= min && i <= max; i--) {
                gchar *this = g_strdup_printf("%s%0*" G_GINT64_FORMAT,
                                              separators[i + 1], widths[i],
                                              times[i]);
                string = replay_strconcat_and_free(string, this);
        }
        /* now add units */
        string = replay_strconcat_and_free(string,
                                           g_strdup(units[max]));
        return string;
}

static gchar *
absolute_to_string(gint64 times[REPLAY_NUM_TIME_UNITS],
                   ReplayTimeUnit max)
{
        gint64 timestamp = replay_timestamp_from_times(times);
        gint64 unix_secs = timestamp / 1000000;
        gint64 msecs = (timestamp % 1000000) / 1000;
        gint64 usecs = (timestamp % 1000000) % 1000;
        GDateTime *dt = g_date_time_new_from_unix_local(unix_secs);
        gchar *string = g_date_time_format(dt, "%c");

        if (msecs) {
                gchar *msecs_str = g_strdup_printf(" %03" G_GINT64_FORMAT "ms",
                                                   msecs);
                string = replay_strconcat_and_free(string, msecs_str);
        }
        if (usecs) {
                gchar *usecs_str = g_strdup_printf(" %03" G_GINT64_FORMAT "µs",
                                                   usecs);
                string = replay_strconcat_and_free(string, usecs_str);
        }

        return string;
}

gchar *replay_timestamp_to_string(gint64 timestamp,
                                  gboolean absolute)
{
        gchar *string = NULL;
        gint64 times[REPLAY_NUM_TIME_UNITS];
        ReplayTimeUnit max;

        max = replay_timestamp_get_times(timestamp, times);

        if (absolute) {
                string = absolute_to_string(times, max);
        } else {
                string = relative_to_string(times, max);
        }
        return string;
}
