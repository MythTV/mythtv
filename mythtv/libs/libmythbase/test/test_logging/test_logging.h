/*
 *  Class TestLogging
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
#include <iostream>
#include <sstream>

#include "mythsyslog.h"
#include "exitcodes.h"
#include "logging.h"
#include "mythlogging.h"

class TestLogging : public QObject
{
    Q_OBJECT

private slots:
    static void initialize(void);
    static void test_syslogGetFacility_data(void);
    static void test_syslogGetFacility(void);
    static void test_logLevelGet_data(void);
    static void test_logLevelGet(void);
    static void test_logLevelGetName_data(void);
    static void test_logLevelGetName(void);
    static void test_verboseArgParse_kwd_data(void);
    static void test_verboseArgParse_kwd(void);
    static void test_verboseArgParse_twice(void);
    static void test_verboseArgParse_class_data(void);
    static void test_verboseArgParse_class(void);
    static void test_verboseArgParse_level_data(void);
    static void test_verboseArgParse_level(void);
    static void test_logPropagateCalc_data(void);
    static void test_logPropagateCalc(void);
};
