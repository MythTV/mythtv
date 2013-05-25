/*
 *  Class TestMythSystem
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

#include <QtTest/QtTest>

#include <QDir>

#include <iostream>
using namespace std;

#include "mythcorecontext.h"
#include "mythsystem.h"

class TestMythSystem: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", NULL);
    }

    // called at the end of these sets of tests
    void cleanupTestCase(void)
    {
        ShutdownMythSystem();
    }

    // called before each test case
    void init(void)
    {
    }

    // called after each test case
    void cleanup(void)
    {
    }

    void constructed_command_is_run(void)
    {
        MythSystem cmd("pwd", kMSStdOut);
        cmd.Run();
        int ret = cmd.Wait();
        QVERIFY(ret == 0);
        QVERIFY(QString(cmd.ReadAll()).contains(QDir::currentPath()));
    }
};
