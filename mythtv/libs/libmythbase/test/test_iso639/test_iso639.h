/*
 *  Class TestISO639
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

#include "iso639.h"

class TestISO639 : public QObject
{
    Q_OBJECT

private slots:
    static void test_str2name_data(void);
    static void test_str2name(void);
    static void test_key2str3_data(void);
    static void test_key2str3(void);
    static void test_str3_to_key_data(void);
    static void test_str3_to_key(void);
    static void test_key2name_data(void);
    static void test_key2name(void);
    static void test_str2_to_str3_data(void);
    static void test_str2_to_str3(void);
    static void test_key_to_cankey_data(void);
    static void test_key_to_cankey(void);
};
