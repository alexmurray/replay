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

#ifndef __REPLAY_TIMEVAL_UTILS_H__
#define __REPLAY_TIMEVAL_UTILS_H__

#include <glib.h>
#include <sys/time.h>

/**
 * ReplayTimeUnit:
 * @REPLAY_TIME_UNIT_MICROSECONDS: Time in microseconds
 * @REPLAY_TIME_UNIT_MILLISECONDS: Time in milliseconds
 * @REPLAY_TIME_UNIT_SECONDS: Time in seconds
 * @REPLAY_TIME_UNIT_MINUTES: Time in minutes
 * @REPLAY_TIME_UNIT_HOURS: Time in hours
 * @REPLAY_TIME_UNIT_DAYS: Time in days
 * @REPLAY_NUM_TIME_UNITS: The number of different time units
 *
 * The time unit used to display timestamps
 */
typedef enum _ReplayTimeUnit
{
  REPLAY_TIME_UNIT_MICROSECONDS = 0,
  REPLAY_TIME_UNIT_MILLISECONDS,
  REPLAY_TIME_UNIT_SECONDS,
  REPLAY_TIME_UNIT_MINUTES,
  REPLAY_TIME_UNIT_HOURS,
  REPLAY_TIME_UNIT_DAYS,
  REPLAY_NUM_TIME_UNITS
} ReplayTimeUnit;

#define REPLAY_DEFAULT_ABSOLUTE_TIME FALSE
#define REPLAY_DEFAULT_TIME_UNIT REPLAY_TIME_UNIT_SECONDS

ReplayTimeUnit replay_timestamp_get_times(gint64 timestamp,
                                           gint64 times[REPLAY_NUM_TIME_UNITS]);
gint64 replay_timestamp_from_times(gint64 times[REPLAY_NUM_TIME_UNITS]);
gchar *replay_timestamp_to_string(gint64 timestamp,
                                  gboolean absolute);

#endif /* __REPLAY_TIMEVAL_UTILS_H__ */
