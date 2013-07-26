/*
 *  Class TestMPEGTables
 *
 *  Copyright (C) Karl Dietz 2013
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

#include "mpegtables.h"
#include "dvbtables.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

class TestMPEGTables: public QObject
{
    Q_OBJECT

  private slots:
    void pat_test(void)
    {
        const unsigned char si_data[] = {
            0x00, 0xb0, 0x31, 0x04, 0x37, 0xdf, 0x00, 0x00,  0x2b, 0x66, 0xf7, 0xd4, 0x6d, 0x66, 0xe0, 0x64,  /* ..1.7...+f..mf.d */
            0x6d, 0x67, 0xe0, 0xc8, 0x6d, 0x68, 0xe1, 0x2c,  0x6d, 0x6b, 0xe2, 0x58, 0x6d, 0x6c, 0xe2, 0xbc,  /* mg..mh.,mk.Xml.. */
            0x6d, 0x6d, 0xe3, 0x20, 0x6d, 0x6e, 0xe2, 0x8a,  0x6d, 0x70, 0xe4, 0x4c, 0x6d, 0x71, 0xe1, 0x9b,  /* mm. mn..mp.Lmq.. */
            0xc0, 0x79, 0xa6, 0x2b                                                                            /* .y.+ */
        };

        PSIPTable si_table(si_data);

        QVERIFY  (si_table.IsGood());

        ProgramAssociationTable pat = si_table;

        QVERIFY  (pat.HasCRC());
        QVERIFY  (pat.VerifyCRC());
        QCOMPARE (si_table.Length(),       (uint32_t)  0x31);
        QCOMPARE (pat.Length(),            (uint32_t)    49);
        QCOMPARE (pat.SectionLength(),     (uint32_t)    49+3);
        QCOMPARE (pat.TransportStreamID(), (uint32_t)  1079);
        QCOMPARE (pat.ProgramCount(),      (uint32_t)    10);

        QCOMPARE (pat.ProgramNumber(0),    (uint32_t) 11110);
        QCOMPARE (pat.ProgramPID(0),       (uint32_t)  6100);

        QCOMPARE (pat.ProgramNumber(1),    (uint32_t) 28006);
        QCOMPARE (pat.ProgramPID(1),       (uint32_t)   100);

        QCOMPARE (pat.ProgramNumber(2),    (uint32_t) 28007);
        QCOMPARE (pat.ProgramPID(2),       (uint32_t)   200);

        QCOMPARE (pat.ProgramNumber(3),    (uint32_t) 28008);
        QCOMPARE (pat.ProgramPID(3),       (uint32_t)   300);

        QCOMPARE (pat.ProgramNumber(4),    (uint32_t) 28011);
        QCOMPARE (pat.ProgramPID(4),       (uint32_t)   600);

        QCOMPARE (pat.ProgramNumber(5),    (uint32_t) 28012);
        QCOMPARE (pat.ProgramPID(5),       (uint32_t)   700);

        QCOMPARE (pat.ProgramNumber(6),    (uint32_t) 28013);
        QCOMPARE (pat.ProgramPID(6),       (uint32_t)   800);

        QCOMPARE (pat.ProgramNumber(7),    (uint32_t) 28014);
        QCOMPARE (pat.ProgramPID(7),       (uint32_t)   650);

        QCOMPARE (pat.ProgramNumber(8),    (uint32_t) 28016);
        QCOMPARE (pat.ProgramPID(8),       (uint32_t)  1100);

        QCOMPARE (pat.ProgramNumber(9),    (uint32_t) 28017);
        QCOMPARE (pat.ProgramPID(9),       (uint32_t)   411);

        // go out of bounds and see what happens if we parse the CRC
        QCOMPARE (pat.ProgramNumber(10) << 16 | pat.ProgramPID(10), pat.CRC() & 0xFFFF1FFF);

        QCOMPARE (pat.FindPID(11110),      (uint32_t)  6100); // the first
        QCOMPARE (pat.FindPID(28017),      (uint32_t)   411); // the last
        QCOMPARE (pat.FindPID(28008),      (uint32_t)   300); // random one
        QCOMPARE (pat.FindPID(12345),      (uint32_t)     0); // not found

        QCOMPARE (pat.FindProgram(6100),   (uint32_t) 11110); // the first
        QCOMPARE (pat.FindProgram(411),    (uint32_t) 28017); // the last
        QCOMPARE (pat.FindProgram(300),    (uint32_t) 28008); // random one
        QCOMPARE (pat.FindProgram(12345),  (uint32_t)     0); // not found

        // first PID that is a real PMT PID and not the NIT PID
        QCOMPARE (pat.FindAnyPID(),        (uint32_t)  6100);
    }

    void dvbdate(void)
    {
        unsigned char dvbdate_data[] = {
            0xdc, 0xa9, 0x12, 0x33, 0x37 /* day 0xdca9, 12:33:37 UTC */
        };

        QCOMPARE (dvbdate2unix (dvbdate_data), (time_t) 1373978017);
        QCOMPARE (dvbdate2qt (dvbdate_data), MythDate::fromString("2013-07-16 12:33:37 Z"));
    }

    void tdt_test(void)
    {
        const unsigned char si_data[] = {
            0x70, 0x70, 0x05, 0xdc, 0xa9, 0x12, 0x33, 0x37                                                    /* pp....37 */
        };

        PSIPTable si_table(si_data);

//      test for the values needed for the copy constructor, especially the length
//        QCOMPARE (si_table._pesdataSize, 8); // is protected, so use TSSizeInBuffer() instead
        QCOMPARE (si_table.TSSizeInBuffer(), (unsigned int) 8);

        TimeDateTable tdt = si_table;

        QVERIFY  (tdt.IsGood());

        QVERIFY  (!tdt.HasCRC());
        QVERIFY  (tdt.VerifyCRC());

        for (size_t i=0; i<5; i++) {
            QCOMPARE (si_table.pesdata()[i+3], (uint8_t) si_data[i+3]);
        }

        const unsigned char *dvbDateBuf = tdt.UTCdata();

        for (size_t i=0; i<5; i++) {
            QCOMPARE (dvbDateBuf[i], (uint8_t) si_data[i+3]);
        }

        // actual is 2013-03-30 01:00:00 UTC, 24 hours before the switch to DST in europe
        QCOMPARE (tdt.UTCUnix(), (time_t) 1373978017);
        QCOMPARE (tdt.UTC(), MythDate::fromString("2013-07-16 12:33:37 Z"));
    }
};
