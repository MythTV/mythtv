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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

/* test data */
extern unsigned char eit_data_0000[];
extern unsigned char tvct_data_0000[];

class TestMPEGTables: public QObject
{
    Q_OBJECT

  private slots:
    void pat_test(void);

    void dvbdate(void);

    void tdt_test(void);

    void ContentIdentifierDescriptor_test(void);

    /** test for coverity 1047220: Incorrect deallocator used:
     * Calling "PSIPTable::~PSIPTable()" frees "(&psip)->_fullbuffer"
     * using "free" but it should have been freed using "operator delete[]".
     *
     * _allocSize should be 0 thus we are not freeing something we didn't
     * allocate in the first place (false positive)
     */
    void clone_test(void);

    /** test PrivateDataSpecifierDescriptor */
    void PrivateDataSpecifierDescriptor_test (void);

    /** test for https://code.mythtv.org/trac/ticket/12091
     * UPC Cablecom switched from standard DVB key/value set to
     * custom descriptors
     */
    void PrivateUPCCablecomEpisodetitleDescriptor_test (void);

    /** test for DVB-EIT style cast
     */
    void ItemList_test (void);

    /** test decoding chinese multi-byte strings, #12507
     */
    void TestUCS2 (void);

    /** test ParentalRatingDescriptor, #12553
     */
    void ParentalRatingDescriptor_test (void);

    /** test items from ExtendedEventDescriptor
     */
    void ExtendedEventDescriptor_test (void);

    /** test US channel names for trailing \0 characters, #12612
      */
    void OTAChannelName_test (void);
};
