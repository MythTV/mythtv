/*
 *  Class TestBitReader
 *
 *  Copyright (c) Scott Theisen 2022
 *
 * This file is part of MythTV.
 *
 * MythTV is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MythTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with MythTV; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <QTest>

#include "libmythtv/bitreader.h"

class TestBitReader : public QObject
{
    Q_OBJECT

  private slots:
    static void skip_bits_nonaligned();
    static void skip_bits_aligned();
    static void get_bits();
    static void get_bits64();
    static void get_bits64_aligned();
    static void get_ue_golomb_long();
    static void get_se_golomb1();
    static void get_se_golomb2();
};
