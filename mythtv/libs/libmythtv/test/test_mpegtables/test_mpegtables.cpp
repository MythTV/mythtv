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

#include "atsctables.h"
#include "mpegtables.h"
#include "dvbtables.h"

void TestMPEGTables::pat_test(void)
{
    const unsigned char si_data[] = {
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
    vector<uint> pnums;
    vector<uint> pids;
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
    unsigned char si_data4[188];
    memset (&si_data4, 0, sizeof(si_data4));
    si_data4[1] = 1 << 7 & 0 << 6 & 3 << 4 & 0 << 2 & 0;
    si_data4[2] = 0x00;
    auto* pat4 = new ProgramAssociationTable(PSIPTable((unsigned char*)&si_data4));
    QCOMPARE (pat4->CalcCRC(), (uint) 0xFFFFFFFF);
    QVERIFY (pat4->VerifyCRC());
    delete pat4;
}

void TestMPEGTables::dvbdate(void)
{
    unsigned char dvbdate_data[] = {
        0xdc, 0xa9, 0x12, 0x33, 0x37 /* day 0xdca9, 12:33:37 UTC */
    };

    QCOMPARE (dvbdate2unix (dvbdate_data), (time_t) 1373978017);
    QCOMPARE (dvbdate2qt (dvbdate_data), MythDate::fromString("2013-07-16 12:33:37 Z"));
}

void TestMPEGTables::tdt_test(void)
{
    const unsigned char si_data[] = {
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
    QCOMPARE (tdt.UTC(), MythDate::fromString("2013-07-16 12:33:37 Z"));
}

void TestMPEGTables::ContentIdentifierDescriptor_test(void)
{
    const unsigned char eit_data[] = {
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
    const unsigned char si_data[] = { 
        0x5f, 0x04, 0x00, 0x00, 0x06, 0x00
    };
    PrivateDataSpecifierDescriptor desc(si_data);
    QCOMPARE (desc.PrivateDataSpecifier(), (uint32_t) PrivateDataSpecifierID::UPC1);
}

void TestMPEGTables::PrivateUPCCablecomEpisodetitleDescriptor_test (void)
{
    const unsigned char si_data[] = {
        0xa7, 0x13, 0x67, 0x65, 0x72, 0x05, 0x4b, 0x72,  0x61, 0x6e, 0x6b, 0x20, 0x76, 0x6f, 0x72, 0x20,  /* ..ger.Krank vor  */
        0x4c, 0x69, 0x65, 0x62, 0x65                                                                      /* Liebe            */
    };

    PrivateUPCCablecomEpisodeTitleDescriptor descriptor(si_data);
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
    unsigned char ucs2_data[] = {
        0x17, 0x11, 0x80, 0x06, 0x5e, 0xb7, 0x67, 0x03,  0x54, 0x48, 0x73, 0x7b, 0x00, 0x3a, 0x95, 0x8b,
        0xc3, 0x80, 0x01, 0x53, 0xcb, 0x8a, 0x18, 0xbf
    };

    wchar_t wchar_data[] = L"\u8006\u5eb7\u6703\u5448\u737b\u003a\u958b\uc380\u0153\ucb8a\u18bf";

    QCOMPARE (sizeof (QChar), (size_t) 2);
    QCOMPARE (sizeof (ucs2_data) - 1, (size_t) ucs2_data[0]);
    QString ucs2 = dvb_decode_text (&ucs2_data[1], ucs2_data[0], nullptr, 0);
    QCOMPARE (ucs2.length(), (int) (ucs2_data[0] - 1) / 2);
    QCOMPARE (ucs2, QString::fromWCharArray (wchar_data));
}

void TestMPEGTables::ParentalRatingDescriptor_test (void)
{
    /* from https://forum.mythtv.org/viewtopic.php?p=4376 / #12553 */
    const unsigned char si_data[] = { 
        0x55, 0x04, 0x47, 0x42, 0x52, 0x0B
    };
    ParentalRatingDescriptor desc(si_data);
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
    unsigned char tvct_data[] = {
        0xc8, 0xf0, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x01, 0x00,  'A', 0x00,  'B', 0x00,  'C',
        0x00,  'D', 0x00,  'E', 0x00,  'F', 0x00, '\0',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x60, 0xc4, 0x82, 0xb9
    };

    PSIPTable psip = PSIPTable(tvct_data);
    TerrestrialVirtualChannelTable table(psip);

    QVERIFY (table.HasCRC());
    QCOMPARE (table.CalcCRC(), table.CRC());
    QVERIFY (table.VerifyCRC());

    QCOMPARE (table.SectionLength(), (unsigned int)sizeof (tvct_data));

    QCOMPARE (table.ChannelCount(), 1U);
    QCOMPARE (table.ShortChannelName(0), QString("ABCDEF"));
    QCOMPARE (table.ShortChannelName(1), QString());

    PSIPTable psip2 = PSIPTable(tvct_data_0000);
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

QTEST_APPLESS_MAIN(TestMPEGTables)
