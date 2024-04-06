/*
 *  Class TestMusicMetadata
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

#include "libmythbase/mythcorecontext.h"
#include "libmythmetadata/metaio.h"

class TestMusicMetadata : public QObject
{
    Q_OBJECT

private slots:
    static void initTestCase();
    static void dump(MusicMetadata *data);
    static void test_flac(void);
    static void test_ogg(void);
    static void test_mp4(void);
    static void test_mp3(void);
    static void test_wv(void);
    static void test_aiff(void);
    static void cleanupTestCase();
};
