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

#include <QtTest/QtTest>

/* test data */
extern std::array<uint8_t,805> eit_data_0000;
extern std::array<uint8_t,433> tvct_data_0000;

class TestMPEGTables: public QObject
{
    Q_OBJECT

  private slots:
    static void pat_test(void);

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
