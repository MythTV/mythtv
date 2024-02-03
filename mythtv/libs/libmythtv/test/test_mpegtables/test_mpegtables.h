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

#include <QTest>

/* test data */
extern std::array<uint8_t,805> eit_data_0000;
extern std::array<uint8_t,433> tvct_data_0000;

class TestMPEGTables: public QObject
{
    Q_OBJECT

    static void update_crc(std::vector<uint8_t> &data);

  private slots:
    static void pat_test(void);
    static void mpeg_pmt_test1(void);
    static void mpeg_pmt_test2(void);
    static void mpeg_pmt_test3(void);
    static void mpeg_pmt_test4(void);

    // ATSC Tables
    static void atsc_mgt_test1a(void);
    static void atsc_mgt_test1b(void);
    static void atsc_mgt_test2(void);
    static void atsc_mgt_test3(void);
    static void atsc_tvct_test1a(void);
    static void atsc_cvct_test1(void);
    static void atsc_cvct_test2(void);
    static void atsc_eit_test1a(void);
    static void atsc_ett_test(void);
    static void atsc_rrt_test(void);
    static void atsc_stt_test(void);

    // SCTE 35 Splice Points
    static void scte35_sit_test1(void);

    //DVB Tables
    static void dvb_nit_test1(void);
    static void dvb_nit_test2(void);
    static void dvb_sdt_test1(void);
    static void dvb_sdt_test2a(void);
    static void dvb_sdt_test2o(void);
    static void dvb_eit_test1(void);
    static void dvb_tdt_test1(void);
    static void dvb_tdt_test2(void);
    static void dvb_tot_test2(void);

    static void dvbdate(void);

    static void tdt_test(void);

    static void ContentIdentifierDescriptor_test(void);

    /** test for coverity 1047220: Incorrect deallocator used:
     * Calling "PSIPTable::~PSIPTable()" frees "(&psip)->_fullbuffer"
     * using "free" but it should have been freed using "operator delete[]".
     *
     * _allocSize should be 0 thus we are not freeing something we didn't
     * allocate in the first place (false positive)
     */
    static void clone_test(void);

    /** test PrivateDataSpecifierDescriptor */
    static void PrivateDataSpecifierDescriptor_test (void);

    /** test for https://code.mythtv.org/trac/ticket/12091
     * UPC Cablecom switched from standard DVB key/value set to
     * custom descriptors
     */
    static void PrivateUPCCablecomEpisodetitleDescriptor_test (void);

    /** test for DVB-EIT style cast
     */
    static void ItemList_test (void);

    /** test decoding chinese multi-byte strings, #12507
     */
    static void TestUCS2 (void);

    /** test vauious ISO-8859 character sets
     */
    static void TestISO8859_data (void);
    static void TestISO8859 (void);

    /** test ParentalRatingDescriptor, #12553
     */
    static void ParentalRatingDescriptor_test (void);

    /** test items from ExtendedEventDescriptor
     */
    static void ExtendedEventDescriptor_test (void);

    /** test US channel names for trailing \0 characters, #12612
      */
    static void OTAChannelName_test (void);

    /** test atsc huffman1 decoding */
    static void atsc_huffman_test_data (void);
    static void atsc_huffman_test (void);
};
