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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <chrono> // for milliseconds
#include <thread> // for sleep_for

#include <QTest>

#include "mythtimer.h"

class TestMythTimer: public QObject
{
    Q_OBJECT

  private slots:
    static void StartsNotRunning(void)
    {
        MythTimer t;
        QVERIFY(!t.isRunning());
    }

    static void StartsOnStart(void)
    {
        MythTimer t;
        t.start();
        QVERIFY(t.isRunning());
    }

    static void TimeElapsesAfterStart(void)
    {
        MythTimer t;
        t.start();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
    }

    static void TimeElapsesAfterRestart(void)
    {
        MythTimer t;
        t.restart();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
    }

    static void TimeDoesNotElapseImmediatelyAfterConstructionByDefault(void)
    {
        MythTimer t;
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() == 0ms);
    }

    static void TimeDoesNotElapsesImmediatelyAfterContructionIfIntended(void)
    {
        MythTimer t(MythTimer::kStartRunning);
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
    }

    static void TimeElapsesContinually(void)
    {
        MythTimer t;
        t.start();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
        std::this_thread::sleep_for(500ms);
        QVERIFY(t.elapsed() > 1s);
    }

    static void TimeResetsOnRestart(void)
    {
        MythTimer t;
        t.start();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.restart() > 500ms);
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms && t.elapsed() < 750ms);
    }

    static void AddMSecsWorks(void)
    {
        MythTimer t;
        t.start();
        t.addMSecs(-250ms);
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 250ms && t.elapsed() < 500ms);
    }

    static void AddMSecsIsResetOnStart(void)
    {
        MythTimer t;
        t.addMSecs(-250ms);
        t.start();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
    }

    static void AddMSecsIsResetOnRestart(void)
    {
        MythTimer t;
        t.addMSecs(-250ms);
        t.restart();
        std::this_thread::sleep_for(520ms);
        QVERIFY(t.elapsed() > 500ms);
    }
};
