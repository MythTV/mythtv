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

#ifndef ZMUTILS_H
#define ZMUTILS_H

class QSqlDatabase;

typedef struct
{
    QString binPath;

    QString webUser;
    QString webGroup;
    QString webPath;

    QString DBHost;
    QString DBDatabase;
    QString DBUser;
    QString DBPassword;
} ZMConfig;

// global database connection to the zm database
extern QSqlDatabase *g_ZMDatabase;
// global zm settings
extern ZMConfig     *g_ZMConfig;

extern bool openZMDatabase(void);
extern bool loadZMConfig(void);
extern void deleteEvent(int eventID);

#endif
