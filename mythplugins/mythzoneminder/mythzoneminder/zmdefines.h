/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef ZMDEFINES_H
#define ZMDEFINES_H

#include <vector>

// qt
#include <QString>

// event details
typedef struct
{
    int monitorID;
    int eventID;
    QString eventName;
    QString monitorName;
    QString startTime;
    QString length;
} Event;

// event frame details
typedef struct
{
    QString type;
    double delta;
} Frame;

enum MonitorPalette
{
    MP_GREY = 1,
    MP_RGB565 = 3,
    MP_RGB24 = 4
};

typedef struct
{
    // used by console view
    int     id;
    QString name;
    QString type;
    QString function;
    int enabled;
    QString device;
    QString zmcStatus;
    QString zmaStatus;
    int events;
    // used by live view
    QString status;
    int width;
    int height;
    int palette;
    bool isV4L2;
} Monitor;

#endif
