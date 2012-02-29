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
#include <QDateTime>

// event details
typedef struct
{
    int monitorID;
    int eventID;
    QString eventName;
    QString monitorName;
    QDateTime startTime;
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
    MP_RGB24 = 4
};

class Monitor
{
  public:
    Monitor() :
        id(0), enabled(0), events(0),
        width(0), height(0), palette(0), isV4L2(false)
    {
    }

  public:
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
};

#endif
