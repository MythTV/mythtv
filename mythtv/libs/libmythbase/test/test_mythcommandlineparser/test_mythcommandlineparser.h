/*
 *  Class TestCommandLineParser
 *
 *  Copyright (c) David Hampton 2020
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

#include <QTest>
#include <QTemporaryFile>
#include <iostream>
#include <sstream>

#include "mythcommandlineparser.h"

class TestCommandLineParser : public QObject
{
    Q_OBJECT

    QTemporaryFile m_testfile {QDir::tempPath() + "/myth-test-XXXXXX"};

private slots:
    void initTestCase();
    static void test_getOpt_data();
    static void test_getOpt();
    static void test_getOpt_passthrough();
    static void test_parse_help();
    static void test_overrides();
    void test_override_file();
    void cleanupTestCase();

    static void test_parse_cmdline_data(void);
    static void test_parse_cmdline(void);
};
