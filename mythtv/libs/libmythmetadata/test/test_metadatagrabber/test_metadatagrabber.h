/*
 *  Class TestMetadataGrabber
 *
 *  Copyright (c) David Hampton 2021
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
#include "libmythmetadata/musicmetadata.h"
#include "libmythmetadata/metadatagrabber.h"

class TestMetadataGrabber : public QObject
{
    Q_OBJECT

    static void dump(void);

private slots:
    static void initTestCase();
    static void test_inetref(void);
    static void test_fromInetref(void);
    static void cleanupTestCase();
};
