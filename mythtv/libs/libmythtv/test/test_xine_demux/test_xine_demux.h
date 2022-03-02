/*
 *  Class TestXineDemux
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

#include <QtTest/QtTest>
#include <iostream>
#include "libmythtv/captions/xine_demux_sputext.h"

class TestXineDemux : public QObject
{
    Q_OBJECT

private slots:
    static void initTestCase();
    static void open_file(const QString& filename, demux_sputext_t& sub_data, bool& loaded);
    static void test_captions_microdvd(void);
    static void test_captions_srt(void);
    static void test_captions_subviewer_data(void);
    static void test_captions_subviewer(void);
    static void test_captions_smi(void);
    static void test_captions_vplayer(void);
    static void test_captions_rt(void);
    static void test_captions_ssa_ass_data(void);
    static void test_captions_ssa_ass(void);
    static void test_captions_pjs(void);
    static void test_captions_mpsub(void);
    static void test_captions_aqtitle(void);
    static void test_captions_jaco(void);
    static void test_captions_subrip09(void);
    static void test_captions_mpl2(void);
    static void cleanupTestCase();
};
