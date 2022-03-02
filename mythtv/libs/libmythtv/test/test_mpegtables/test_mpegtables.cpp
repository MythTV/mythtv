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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "test_mpegtables.h"

#include <iconv.h>

#include "libmythtv/mpeg/atsc_huffman.h"
#include "libmythtv/mpeg/atsctables.h"
#include "libmythtv/mpeg/dvbtables.h"
#include "libmythtv/mpeg/mpegtables.h"

static std::array<uint8_t,3+8*12> high8 {
    0x10, 0x00, 0x00,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

void TestMPEGTables::pat_test(void)
{
    const std::vector<uint8_t> si_data {
        0x00, 0xb0, 0x31, 0x04, 0x37, 0xdf, 0x00, 0x00,  0x2b, 0x66, 0xf7, 0xd4, 0x6d, 0x66, 0xe0, 0x64,  /* ..1.7...+f..mf.d */
        0x6d, 0x67, 0xe0, 0xc8, 0x6d, 0x68, 0xe1, 0x2c,  0x6d, 0x6b, 0xe2, 0x58, 0x6d, 0x6c, 0xe2, 0xbc,  /* mg..mh.,mk.Xml.. */
        0x6d, 0x6d, 0xe3, 0x20, 0x6d, 0x6e, 0xe2, 0x8a,  0x6d, 0x70, 0xe4, 0x4c, 0x6d, 0x71, 0xe1, 0x9b,  /* mm. mn..mp.Lmq.. */
        0xc0, 0x79, 0xa6, 0x2b                                                                            /* .y.+ */
    };

    PSIPTable si_table(si_data);

    QVERIFY  (si_table.IsGood());

    ProgramAssociationTable pat(si_table);

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

    // Create a PAT no CRC error
    std::vector<uint> pnums;
    std::vector<uint> pids;
    pnums.push_back(1);
    pids.push_back(0x100);
    ProgramAssociationTable* pat2 =
        ProgramAssociationTable::Create(1, 0, pnums, pids);
    QVERIFY (pat2->VerifyCRC());

    // Create a blank PAT, CRC error!
    ProgramAssociationTable* pat3 =
        ProgramAssociationTable::CreateBlank();
    QVERIFY (!pat3->VerifyCRC());
    // we still have not found "CRC mismatch 0 != 0xFFFFFFFF"
    QCOMPARE (pat3->CalcCRC(), (uint) 0x334FF8A0);
    pat3->SetCRC(pat3->CalcCRC());
    QVERIFY (pat3->VerifyCRC());

    // Create a PAT object
    std::vector<uint8_t> si_data4(188,'\0');
    si_data4[1] = 1 << 7 & 0 << 6 & 3 << 4 & 0 << 2 & 0;
    si_data4[2] = 0x00;
    auto* pat4 = new ProgramAssociationTable(PSIPTable(si_data4));
    QCOMPARE (pat4->CalcCRC(), (uint) 0xFFFFFFFF);
    QVERIFY (pat4->VerifyCRC());
    delete pat4;
}

void TestMPEGTables::dvbdate(void)
{
    const std::array<uint8_t,5> dvbdate_data {
        0xdc, 0xa9, 0x12, 0x33, 0x37 /* day 0xdca9, 12:33:37 UTC */
    };

    QCOMPARE (dvbdate2unix (dvbdate_data), (time_t) 1373978017);
    QCOMPARE (dvbdate2qt (dvbdate_data), MythDate::fromString("2013-07-16 12:33:37Z"));
}

void TestMPEGTables::tdt_test(void)
{
    const std::vector<uint8_t> si_data {
        0x70, 0x70, 0x05, 0xdc, 0xa9, 0x12, 0x33, 0x37                                                    /* pp....37 */
    };

    PSIPTable si_table(si_data);

    // test for the values needed for the copy constructor, especially the length
    // QCOMPARE (si_table._pesdataSize, 8); // is protected, so use TSSizeInBuffer() instead
    QCOMPARE (si_table.TSSizeInBuffer(), (unsigned int) 8);

    TimeDateTable tdt(si_table);

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
    QCOMPARE (tdt.UTC(), MythDate::fromString("2013-07-16 12:33:37Z"));
}

void TestMPEGTables::ContentIdentifierDescriptor_test(void)
{
    const std::vector<uint8_t> eit_data {
        0x4f, 0xf2, 0x17, 0x42, 0xd8, 0xdb, 0x00, 0x01,  0x00, 0xab, 0x27, 0x0f, 0x01, 0x4f, 0x30, 0x17,  /* O..B......'..O0. */
        0xdc, 0xc9, 0x07, 0x15, 0x00, 0x00, 0x25, 0x00,  0x81, 0xfc, 0x4d, 0xb2, 0x65, 0x6e, 0x67, 0x0d,  /* ......%...M.eng. */
        0x05, 0x4d, 0x6f, 0x6e, 0x65, 0x79, 0x62, 0x72,  0x6f, 0x74, 0x68, 0x65, 0x72, 0xa0, 0x05, 0x44,  /* .Moneybrother..D */
        0x6f, 0x63, 0x75, 0x6d, 0x65, 0x6e, 0x74, 0x61,  0x72, 0x79, 0x20, 0x73, 0x65, 0x72, 0x69, 0x65,  /* ocumentary serie */
        0x73, 0x20, 0x6f, 0x6e, 0x20, 0x53, 0x77, 0x65,  0x64, 0x65, 0x6e, 0x27, 0x73, 0x20, 0x41, 0x6e,  /* s on Sweden's An */
        0x64, 0x65, 0x72, 0x73, 0x20, 0x57, 0x65, 0x6e,  0x64, 0x69, 0x6e, 0x20, 0x61, 0x6e, 0x64, 0x20,  /* ders Wendin and  */
        0x74, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x6b, 0x69,  0x6e, 0x67, 0x20, 0x6f, 0x66, 0x20, 0x68, 0x69,  /* the making of hi */
        0x73, 0x20, 0x6e, 0x65, 0x77, 0x20, 0x61, 0x6c,  0x62, 0x75, 0x6d, 0x2e, 0x20, 0x54, 0x68, 0x69,  /* s new album. Thi */
        0x73, 0x20, 0x65, 0x70, 0x69, 0x73, 0x6f, 0x64,  0x65, 0x20, 0x73, 0x68, 0x6f, 0x77, 0x73, 0x20,  /* s episode shows  */
        0x74, 0x68, 0x65, 0x20, 0x70, 0x72, 0x65, 0x70,  0x61, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20,  /* the preparation  */
        0x69, 0x6e, 0x20, 0x53, 0x74, 0x6f, 0x63, 0x6b,  0x68, 0x6f, 0x6c, 0x6d, 0x20, 0x61, 0x6e, 0x64,  /* in Stockholm and */
        0x20, 0x72, 0x65, 0x63, 0x6f, 0x72, 0x64, 0x69,  0x6e, 0x67, 0x73, 0x20, 0x69, 0x6e, 0x20, 0x43,  /*  recordings in C */
        0x68, 0x69, 0x63, 0x61, 0x67, 0x6f, 0x20, 0x61,  0x6e, 0x64, 0x20, 0x4c, 0x41, 0x2e, 0x4d, 0xc7,  /* hicago and LA.M. */
        0x67, 0x65, 0x72, 0x0d, 0x05, 0x4d, 0x6f, 0x6e,  0x65, 0x79, 0x62, 0x72, 0x6f, 0x74, 0x68, 0x65,  /* ger..Moneybrothe */
        0x72, 0xb5, 0x05, 0x44, 0x69, 0x65, 0x73, 0x65,  0x20, 0x46, 0x6f, 0x6c, 0x67, 0x65, 0x20, 0x64,  /* r..Diese Folge d */
        0x65, 0x73, 0x20, 0x4d, 0x75, 0x73, 0x69, 0x6b,  0x6d, 0x61, 0x67, 0x61, 0x7a, 0x69, 0x6e, 0x73,  /* es Musikmagazins */
        0x20, 0x6d, 0x69, 0x74, 0x20, 0x64, 0x65, 0x6d,  0x20, 0x73, 0x63, 0x68, 0x77, 0x65, 0x64, 0x69,  /*  mit dem schwedi */
        0x73, 0x63, 0x68, 0x65, 0x6e, 0x20, 0x4d, 0x75,  0x73, 0x69, 0x6b, 0x65, 0x72, 0x20, 0x41, 0x6e,  /* schen Musiker An */
        0x64, 0x65, 0x72, 0x73, 0x20, 0x57, 0x65, 0x6e,  0x64, 0x69, 0x6e, 0x20, 0x61, 0x2e, 0x6b, 0x2e,  /* ders Wendin a.k. */
        0x61, 0x2e, 0x20, 0x4d, 0x6f, 0x6e, 0x65, 0x79,  0x62, 0x72, 0x6f, 0x74, 0x68, 0x65, 0x72, 0x20,  /* a. Moneybrother  */
        0x7a, 0x65, 0x69, 0x67, 0x74, 0x20, 0x64, 0x69,  0x65, 0x20, 0x56, 0x6f, 0x72, 0x62, 0x65, 0x72,  /* zeigt die Vorber */
        0x65, 0x69, 0x74, 0x75, 0x6e, 0x67, 0x65, 0x6e,  0x20, 0x69, 0x6e, 0x20, 0x53, 0x74, 0x6f, 0x63,  /* eitungen in Stoc */
        0x6b, 0x68, 0x6f, 0x6c, 0x6d, 0x20, 0x75, 0x6e,  0x64, 0x20, 0x64, 0x69, 0x65, 0x20, 0x65, 0x72,  /* kholm und die er */
        0x73, 0x74, 0x65, 0x6e, 0x20, 0x41, 0x75, 0x66,  0x6e, 0x61, 0x68, 0x6d, 0x65, 0x6e, 0x20, 0x43,  /* sten Aufnahmen C */
        0x68, 0x69, 0x63, 0x61, 0x67, 0x6f, 0x20, 0x75,  0x6e, 0x64, 0x20, 0x4c, 0x6f, 0x73, 0x20, 0x41,  /* hicago und Los A */
        0x6e, 0x67, 0x65, 0x6c, 0x65, 0x73, 0x2e, 0x76,  0x73, 0x04, 0x40, 0x65, 0x76, 0x65, 0x6e, 0x74,  /* ngeles.vs.@event */
        0x69, 0x73, 0x2e, 0x6e, 0x6c, 0x2f, 0x30, 0x30,  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x30,  /* is.nl/00000000-0 */
        0x30, 0x30, 0x30, 0x2d, 0x31, 0x30, 0x30, 0x30,  0x2d, 0x30, 0x36, 0x30, 0x34, 0x2d, 0x30, 0x30,  /* 000-1000-0604-00 */
        0x30, 0x30, 0x30, 0x30, 0x30, 0x45, 0x30, 0x37,  0x31, 0x31, 0x23, 0x30, 0x30, 0x31, 0x30, 0x33,  /* 00000E0711#00103 */
        0x38, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x32,  0x30, 0x31, 0x37, 0x08, 0x2f, 0x65, 0x76, 0x65,  /* 89900002017./eve */
        0x6e, 0x74, 0x69, 0x73, 0x2e, 0x6e, 0x6c, 0x2f,  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,  /* ntis.nl/00000000 */
        0x2d, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x31, 0x30,  0x30, 0x30, 0x2d, 0x30, 0x36, 0x30, 0x38, 0x2d,  /* -0000-1000-0608- */
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,  0x33, 0x46, 0x39, 0x43, 0x55, 0x04, 0x44, 0x45,  /* 000000003F9CU.DE */
        0x55, 0x00, 0x54, 0x02, 0x23, 0x00, 0x3b, 0xf9,  0x94, 0xa5                                       /* U.T.#.;...       */
    };

    /* pick just the ContentIdentifierDescriptor from the event_information_section */
    DVBContentIdentifierDescriptor descriptor(&eit_data[407]);

    QCOMPARE (descriptor.CRIDCount(),  (size_t) 2);
    /* CRID with IMI appended, the separator and IMI are properly removed from the CRID
     * see ETSI TS 102 323 V1.5.1 page 102
     * The content identifier is composed of the CRID authority "eventis.nl",
     * the separator "/",
     * the CRID data "00000000-0000-1000-0604-0000000E0711",
     * the separator "#", and
     * the IMI "0010389900002017"
     */
    QCOMPARE (descriptor.ContentId(),  QString("eventis.nl/00000000-0000-1000-0604-0000000E0711"));
    /* there is a second content_id in the same descriptor */
    QCOMPARE (descriptor.ContentId(1), QString("eventis.nl/00000000-0000-1000-0608-000000003F9C"));
}

void TestMPEGTables::clone_test(void)
{
    auto *si_data = new unsigned char[8];
    si_data[0] = 0x70; /* pp....37 */
    si_data[1] = 0x70;
    si_data[2] = 0x05;
    si_data[3] = 0xdc;
    si_data[4] = 0xa9;
    si_data[5] = 0x12;
    si_data[6] = 0x33;
    si_data[7] = 0x37;

    const PSIPTable si_table(si_data);

    QVERIFY (!si_table.IsClone());
}

void TestMPEGTables::PrivateDataSpecifierDescriptor_test (void)
{
    /* from https://code.mythtv.org/trac/ticket/12091 */
    const std::vector<uint8_t> si_data {
        0x5f, 0x04, 0x00, 0x00, 0x06, 0x00
    };
    PrivateDataSpecifierDescriptor desc(si_data);
    QCOMPARE (desc.IsValid(), true);
    if (!desc.IsValid())
        return;
    QCOMPARE (desc.PrivateDataSpecifier(), (uint32_t) PrivateDataSpecifierID::UPC1);
}

void TestMPEGTables::PrivateUPCCablecomEpisodetitleDescriptor_test (void)
{
    const std::vector<uint8_t> si_data {
        0xa7, 0x13, 0x67, 0x65, 0x72, 0x05, 0x4b, 0x72,  0x61, 0x6e, 0x6b, 0x20, 0x76, 0x6f, 0x72, 0x20,  /* ..ger.Krank vor  */
        0x4c, 0x69, 0x65, 0x62, 0x65                                                                      /* Liebe            */
    };

    PrivateUPCCablecomEpisodeTitleDescriptor descriptor(si_data);
    QCOMPARE (descriptor.IsValid(), true);
    if (!descriptor.IsValid())
        return;
    QCOMPARE (descriptor.CanonicalLanguageString(), QString("ger"));
    QCOMPARE (descriptor.TextLength(), (uint) 16);
    QCOMPARE (descriptor.Text(), QString("Krank vor Liebe"));
}

void TestMPEGTables::ItemList_test (void)
{
    ShortEventDescriptor descriptor(&eit_data_0000[26]);
    if (!descriptor.IsValid()) {
        QFAIL("The eit_data_0000 descriptor is invalid");
        return;
    }
    QCOMPARE (descriptor.DescriptorTag(), (unsigned int) DescriptorID::short_event);
    QCOMPARE (descriptor.size(), (unsigned int) 194);
    QCOMPARE (descriptor.LanguageString(), QString("ger"));
    QVERIFY  (descriptor.Text().startsWith(QString("Krimiserie. ")));

    ExtendedEventDescriptor descriptor2(&eit_data_0000[26+descriptor.size()]);
    if (!descriptor2.IsValid()) {
        QFAIL("The eit_data_0000 descriptor2 is invalid");
        return;
    }
    QCOMPARE (descriptor2.DescriptorTag(), (unsigned int) DescriptorID::extended_event);
    /* tests for items start here */
    QCOMPARE (descriptor2.LengthOfItems(), (uint) 139);
}

void TestMPEGTables::TestUCS2 (void)
{
    std::array<uint8_t,24> ucs2_data {
        0x17, 0x11, 0x80, 0x06, 0x5e, 0xb7, 0x67, 0x03,  0x54, 0x48, 0x73, 0x7b, 0x00, 0x3a, 0x95, 0x8b,
        0xc3, 0x80, 0x01, 0x53, 0xcb, 0x8a, 0x18, 0xbf
    };

    std::array<wchar_t,12> wchar_data { L"\u8006\u5eb7\u6703\u5448\u737b\u003a\u958b\uc380\u0153\ucb8a\u18bf"};

    QCOMPARE (sizeof (QChar), (size_t) 2);
    QCOMPARE (sizeof (ucs2_data) - 1, (size_t) ucs2_data[0]);
    QString ucs2 = dvb_decode_text (&ucs2_data[1], ucs2_data[0], {});
    QCOMPARE (ucs2.length(), (int) (ucs2_data[0] - 1) / 2);
    QCOMPARE (ucs2, QString::fromWCharArray (wchar_data.data()));
}

void TestMPEGTables::TestISO8859_data (void)
{
    QTest::addColumn<int>("iso");
    QTest::addColumn<QString>("expected");

    QTest::newRow("iso-8859-1") << 1 <<
        QStringLiteral(u" ¡¢£¤¥¦§¨©ª«¬­®¯" \
                        "°±²³´µ¶·¸¹º»¼½¾¿" \
                        "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ" \
                        "ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß" \
                        "àáâãäåæçèéêëìíîï" \
                        "ðñòóôõö÷øùúûüýþÿ" );
    QTest::newRow("iso-8859-2") << 2 <<
        QStringLiteral(u" Ą˘Ł¤ĽŚ§¨ŠŞŤŹ­ŽŻ" \
                        "°ą˛ł´ľśˇ¸šşťź˝žż" \
                        "ŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎ" \
                        "ĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢß" \
                        "ŕáâăäĺćçčéęëěíîď" \
                        "đńňóôőö÷řůúűüýţ˙" );
    QTest::newRow("iso-8859-3") << 3 <<
        QStringLiteral(u" Ħ˘£¤�Ĥ§¨İŞĞĴ­�Ż" \
                        "°ħ²³´µĥ·¸ışğĵ½�ż" \
                        "ÀÁÂ�ÄĊĈÇÈÉÊËÌÍÎÏ" \
                        "�ÑÒÓÔĠÖ×ĜÙÚÛÜŬŜß" \
                        "àáâ�äċĉçèéêëìíîï" \
                        "�ñòóôġö÷ĝùúûüŭŝ˙" );
    QTest::newRow("iso-8859-4") << 4 <<
        QStringLiteral(u" ĄĸŖ¤ĨĻ§¨ŠĒĢŦ­Ž¯" \
                        "°ą˛ŗ´ĩļˇ¸šēģŧŊžŋ" \
                        "ĀÁÂÃÄÅÆĮČÉĘËĖÍÎĪ" \
                        "ĐŅŌĶÔÕÖ×ØŲÚÛÜŨŪß" \
                        "āáâãäåæįčéęëėíîī" \
                        "đņōķôõö÷øųúûüũū˙" );
    QTest::newRow("iso-8859-5") << 5 <<
        QStringLiteral(u" ЁЂЃЄЅІЇЈЉЊЋЌ­ЎЏ" \
                        "АБВГДЕЖЗИЙКЛМНОП" \
                        "РСТУФХЦЧШЩЪЫЬЭЮЯ" \
                        "абвгдежзийклмноп" \
                        "рстуфхцчшщъыьэюя" \
                        "№ёђѓєѕіїјљњћќ§ўџ" );
    // iso-8859-6: latin/arabic
    QTest::newRow("iso-8859-7") << 7 <<
        QStringLiteral(u" ‘’£€₯¦§¨©ͺ«¬­�―" \
                        "°±²³΄΅Ά·ΈΉΊ»Ό½ΎΏ" \
                        "ΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟ" \
                        "ΠΡ�ΣΤΥΦΧΨΩΪΫάέήί" \
                        "ΰαβγδεζηθικλμνξο" \
                        "πρςστυφχψωϊϋόύώ�" );
    // iso-8859-6: latin/hebrew
    QTest::newRow("iso-8859-9") << 9 <<
        QStringLiteral(u" ¡¢£¤¥¦§¨©ª«¬­®¯" \
                        "°±²³´µ¶·¸¹º»¼½¾¿" \
                        "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ" \
                        "ĞÑÒÓÔÕÖ×ØÙÚÛÜİŞß" \
                        "àáâãäåæçèéêëìíîï" \
                        "ğñòóôõö÷øùúûüışÿ" );
    QTest::newRow("iso-8859-10") << 10 <<
        QStringLiteral(u" ĄĒĢĪĨĶ§ĻĐŠŦŽ­ŪŊ" \
                        "°ąēģīĩķ·ļđšŧž―ūŋ" \
                        "ĀÁÂÃÄÅÆĮČÉĘËĖÍÎÏ" \
                        "ÐŅŌÓÔÕÖŨØŲÚÛÜÝÞß" \
                        "āáâãäåæįčéęëėíîï" \
                        "ðņōóôõöũøųúûüýþĸ" );
    QTest::newRow("iso-8859-11") << 11 <<
        QStringLiteral(u" กขฃคฅฆงจฉชซฌญฎฏ" \
                        "ฐฑฒณดตถทธนบปผฝพฟ" \
                        "ภมยรฤลฦวศษสหฬอฮฯ" \
                        "ะัาำิีึืฺุู����฿" \
                        "เแโใไๅๆ็่้๊๋์ํ๎๏" \
                        "๐๑๒๓๔๕๖๗๘๙๚๛����" );
    // iso-8859-12 was abandoned
    QTest::newRow("iso-8859-13") << 13 <<
        QStringLiteral(u" ”¢£¤„¦§Ø©Ŗ«¬­®Æ" \
                        "°±²³“µ¶·ø¹ŗ»¼½¾æ" \
                        "ĄĮĀĆÄÅĘĒČÉŹĖĢĶĪĻ" \
                        "ŠŃŅÓŌÕÖ×ŲŁŚŪÜŻŽß" \
                        "ąįāćäåęēčéźėģķīļ" \
                        "šńņóōõö÷ųłśūüżž’" );
    QTest::newRow("iso-8859-14") << 14 <<
        QStringLiteral(u" Ḃḃ£ĊċḊ§Ẁ©ẂḋỲ­®Ÿ" \
                        "ḞḟĠġṀṁ¶ṖẁṗẃṠỳẄẅṡ" \
                        "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ" \
                        "ŴÑÒÓÔÕÖṪØÙÚÛÜÝŶß" \
                        "àáâãäåæçèéêëìíîï" \
                        "ŵñòóôõöṫøùúûüýŷÿ" );
    QTest::newRow("iso-8859-15") << 15 <<
        QStringLiteral(u" ¡¢£€¥Š§š©ª«¬­®¯" \
                        "°±²³Žµ¶·ž¹º»ŒœŸ¿" \
                        "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ" \
                        "ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß" \
                        "àáâãäåæçèéêëìíîï" \
                        "ðñòóôõö÷øùúûüýþÿ" );
}

void TestMPEGTables::TestISO8859 (void)
{
    QFETCH(int, iso);
    QFETCH(QString, expected);

    high8[2] = iso;
    QString actual = dvb_decode_text(high8.data(), high8.size());
    QCOMPARE (actual, expected);
}

void TestMPEGTables::ParentalRatingDescriptor_test (void)
{
    /* from https://forum.mythtv.org/viewtopic.php?p=4376 / #12553 */
    const std::vector<uint8_t> si_data {
        0x55, 0x04, 0x47, 0x42, 0x52, 0x0B
    };
    ParentalRatingDescriptor desc(si_data);
    QCOMPARE (desc.IsValid(), true);
    if (!desc.IsValid())
        return;
    QCOMPARE (desc.Count(), 1U);
    QCOMPARE (desc.CountryCodeString(0), QString("GBR"));
    QCOMPARE (desc.Rating(0), 14);
}

void TestMPEGTables::ExtendedEventDescriptor_test (void)
{
    ExtendedEventDescriptor desc(&eit_data_0000[16*13+12]);
    if (!desc.IsValid()) {
        QFAIL("The eit_data_0000 descriptor is invalid");
        return;
    }
    QCOMPARE (desc.LengthOfItems(), 139U);
    QMultiMap<QString,QString> items = desc.Items();
    QCOMPARE (items.count(), 5);
    QVERIFY (items.contains (QString ("Role Player")));
    QCOMPARE (items.count (QString ("Role Player")), 5);
    QCOMPARE (items.count (QString ("Role Player"), QString ("Nathan Fillion")), 1);
}

void TestMPEGTables::OTAChannelName_test (void)
{
    /* manually crafted according to A65/2013
     * http://atsc.org/wp-content/uploads/2015/03/Program-System-Information-Protocol-for-Terrestrial-Broadcast-and-Cable.pdf
     */
    const std::vector<uint8_t> tvct_data {
        0xc8, 0xf0, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x01, 0x00,  'A', 0x00,  'B', 0x00,  'C',
        0x00,  'D', 0x00,  'E', 0x00,  'F', 0x00, '\0',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x60, 0xc4, 0x82, 0xb9
    };

    PSIPTable psip = PSIPTable(tvct_data.data());
    TerrestrialVirtualChannelTable table(psip);

    QVERIFY (table.HasCRC());
    QCOMPARE (table.CalcCRC(), table.CRC());
    QVERIFY (table.VerifyCRC());

    QCOMPARE (table.SectionLength(), static_cast<uint>(tvct_data.size()));

    QCOMPARE (table.ChannelCount(), 1U);
    QCOMPARE (table.ShortChannelName(0), QString("ABCDEF"));
    QCOMPARE (table.ShortChannelName(1), QString());

    PSIPTable psip2 = PSIPTable(tvct_data_0000.data());
    TerrestrialVirtualChannelTable tvct(psip2);
    QVERIFY (tvct.VerifyCRC());
    QVERIFY (tvct.VerifyPSIP(false));

    QCOMPARE (tvct.ChannelCount(), 6U);
    /*
     * ShortChannelName is fixed width 7-8 characters.
     * A65/2013 says to fill trailing characters with \0
     * but here the space is used
     */
    QCOMPARE (tvct.ShortChannelName(0), QString("KYNM-HD"));
    QCOMPARE (tvct.ShortChannelName(1), QString("TUFF-TV"));
    QCOMPARE (tvct.ShortChannelName(2), QString("Retro"));
    QCOMPARE (tvct.ShortChannelName(3), QString("REV'N"));
    QCOMPARE (tvct.ShortChannelName(4), QString("QVC"));
    QCOMPARE (tvct.ShortChannelName(5), QString("Antenna"));
    QCOMPARE (tvct.ShortChannelName(6), QString(""));
    QCOMPARE (tvct.ShortChannelName(999), QString());
    /*
     * ExtendedChannelName has a \0 terminated string inside
     * strings with length. That's uncommon.
     */
    QCOMPARE (tvct.GetExtendedChannelName(0), QString());
    QCOMPARE (tvct.GetExtendedChannelName(1), QString("KYNM TUFF-T"));
    QCOMPARE (tvct.GetExtendedChannelName(2), QString("KYNM Albuquerque, N"));
    QCOMPARE (tvct.GetExtendedChannelName(3), QString("KYNM PBJ-T"));
    QCOMPARE (tvct.GetExtendedChannelName(4), QString("KYNM QV"));
    QCOMPARE (tvct.GetExtendedChannelName(5), QString());
    QCOMPARE (tvct.GetExtendedChannelName(6), QString());
    QCOMPARE (tvct.GetExtendedChannelName(999), QString());
}

void TestMPEGTables::atsc_huffman_test_data (void)
{
    QTest::addColumn<QString>("encoding");
    QTest::addColumn<QByteArray>("compressed");
    QTest::addColumn<QString>("e_uncompressed");

    // This is the only example I could find online.
    const std::array<uint8_t,5> example1
        {0b01000011, 0b00101000, 0b11011100, 0b10000100, 0b11010100};
    QTest::newRow("Title")
        << "C5"
        << QByteArray((char *)example1.data(), example1.size())
        << "The next";

    // M = 1010, y = 011, t = 1101001, h = 111, 27 = 1110001,
    // T = 01010100, V = 111100, ' ' = 01010, i = 010010, s = 0011,
    // ' ' = 10, c = 01000000, o = 1001, o = 0011, l = 0100,
    // 27 = 0111001, '!' = 00100001, END = 1
    const std::array<uint8_t,12> example2
        { 0b10100111, 0b10100111, 0b11110001, 0b01010100,
          0b11110001, 0b01001001, 0b00111010, 0b10000001,
          0b00100110, 0b10001110, 0b01001000, 0b01100000};
    QTest::newRow("myth title")
        << "C5"
        << QByteArray((char *)example2.data(), example2.size())
        << "MythTV is cool!";

    // M = 1111, 27 = 11010, y = 0111 1001, 27 = 01010, t = 0111 0100,
    // h = 00, 27 = 1011100, T = 0101 0100, V = 1000, ' ' = 10,
    // i = 10101, s = 101, ' ' = 0, c = 10011, o = 101, o = 10100,
    // l = 0101, '.' = 00100, END = 1
    const std::array<uint8_t,11> example3
        { 0b11111101, 0b00111100, 0b10101001, 0b11010000,
          0b10111000, 0b10101001, 0b00010101, 0b01101010,
          0b01110110, 0b10001010, 0b01001000};
    QTest::newRow("myth descr")
        << "C7"
        << QByteArray((char *)example3.data(), example3.size())
        << "MythTV is cool.";
}

void TestMPEGTables::atsc_huffman_test (void)
{
    QFETCH(QString,    encoding);
    QFETCH(QByteArray, compressed);
    QFETCH(QString,    e_uncompressed);

    QString uncompressed {};
    if (encoding == "C5") {
        uncompressed = atsc_huffman1_to_string((uchar *)compressed.data(),
                                               compressed.size(), 1);
    } else if (encoding == "C7") {
        uncompressed = atsc_huffman1_to_string((uchar *)compressed.data(),
                                               compressed.size(), 2);
    }
    QCOMPARE(uncompressed.trimmed(), e_uncompressed);
}

QTEST_APPLESS_MAIN(TestMPEGTables)
