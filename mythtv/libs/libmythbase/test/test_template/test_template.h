/*
 *  Class TestTemplate
 *
 *  Copyright (C) AUTHOR_GOES_HERE 2013
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

class TestTemplate: public QObject
{
    Q_OBJECT

  private slots:
    // Functions to setup/cleanup or reset between tests
    static void initTestCase(void);
    static void cleanupTestCase(void);
    static void init(void);
    static void cleanup(void);

    // Individual function tests
    static void example_passing_unit_test(void);
    static void example_benchmark_test(void);
    static void example_skipped_test(void);

    static void example_repeated_test_data(void);
    static void example_repeated_test(void);
};
