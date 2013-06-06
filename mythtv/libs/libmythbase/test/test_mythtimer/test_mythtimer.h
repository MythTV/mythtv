/*
 *  Class TestMythTimer
 *
 *  Copyright (C) Daniel Kristjansson 2013
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h> // for usleep

#include <QtTest/QtTest>

#include "mythtimer.h"

class TestMythTimer: public QObject
{
    Q_OBJECT

  private slots:
    void StartsNotRunning(void)
    {
        MythTimer t;
        QVERIFY(!t.isRunning());
    }

    void StartsOnStart(void)
    {
        MythTimer t;
        t.start();
        QVERIFY(t.isRunning());
    }

    void TimeElapsesAfterStart(void)
    {
        MythTimer t;
        t.start();
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
    }

    void TimeElapsesAfterRestart(void)
    {
        MythTimer t;
        t.restart();
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
    }

    void TimeDoesNotElapseImmediatelyAfterConstructionByDefault(void)
    {
        MythTimer t;
        usleep(52 * 1000);
        QVERIFY(t.elapsed() == 0);
    }

    void TimeDoesNotElapsesImmediatelyAfterContructionIfIntended(void)
    {
        MythTimer t(MythTimer::kStartRunning);
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
    }

    void TimeElapsesContinually(void)
    {
        MythTimer t;
        t.start();
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
        usleep(50 * 1000);
        QVERIFY(t.elapsed() > 100);
    }

    void TimeResetsOnRestart(void)
    {
        MythTimer t;
        t.start();
        usleep(52 * 1000);
        QVERIFY(t.restart() > 50);
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50 && t.elapsed() < 75);
    }

    void AddMSecsWorks(void)
    {
        MythTimer t;
        t.start();
        t.addMSecs(-25);
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 25 && t.elapsed() < 50);
    }

    void AddMSecsIsResetOnStart(void)
    {
        MythTimer t;
        t.addMSecs(-25);
        t.start();
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
    }

    void AddMSecsIsResetOnRestart(void)
    {
        MythTimer t;
        t.addMSecs(-25);
        t.restart();
        usleep(52 * 1000);
        QVERIFY(t.elapsed() > 50);
    }
};
