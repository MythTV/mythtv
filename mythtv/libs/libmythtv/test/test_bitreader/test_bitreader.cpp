/*
 *  Class TestBitReader
 *
 *  Copyright (c) Scott Theisen 2022
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
#include "test_bitreader.h"

#include <cstdint>

void TestBitReader::get_bits()
{
    constexpr std::array<uint8_t,4> array =
    {
        0b0111'0100,
        0b0001'0100,
        0b0000'0011,
        0b1111'1111,
    };

    auto br = BitReader(array.data(), array.size());

    QVERIFY(!br.next_bit());
    QCOMPARE(br.get_bits_left(), INT64_C(31));
    br.skip_bits(2);
    QCOMPARE(br.get_bits_left(), INT64_C(29));
    QVERIFY(br.next_bit());
    QCOMPARE(br.get_bits(4), 4U);
    QCOMPARE(br.get_bits_left(), INT64_C(24));
    QCOMPARE(br.get_ue_golomb_31(), 9);
    QCOMPARE(br.get_bits_left(), INT64_C(17));

    // start at byte index 2
    br.skip_bits();
    QCOMPARE(br.get_ue_golomb_31(), -1); // too big
    QCOMPARE(br.get_bits_left(), INT64_C(3));
    QCOMPARE(br.get_ue_golomb_31(), 0);
    QCOMPARE(br.get_bits_left(), INT64_C(2));
    QCOMPARE(br.get_ue_golomb_31(), 0);
    QCOMPARE(br.get_bits_left(), INT64_C(1));
    QCOMPARE(br.get_ue_golomb_31(), 0);
    QCOMPARE(br.get_bits_left(), INT64_C(0));

     // past the end
    QCOMPARE(br.get_ue_golomb_31(), -1);
    QCOMPARE(br.get_bits(4), 0U);
}

void TestBitReader::get_ue_golomb_long()
{
    constexpr std::array<uint8_t,8> array2 =
    {
        0b1000'0000, 0b0000'0000, 0b0000'0000, 0b0000'0000,
        0xDE, 0xAD, 0xBE, 0xEF, // 3735928558 + 1
    };
    auto br2 = BitReader(array2.data(), array2.size());
    br2.skip_bits(); // skip first bit for 31 zeroes
    QCOMPARE(br2.get_ue_golomb_long(), 3735928558U);
    QCOMPARE(br2.get_bits_left(), INT64_C(0));
}

void TestBitReader::get_se_golomb1()
{
    constexpr std::array<uint8_t,16> array3 =
    {
        0b1000'0000, 0b0000'0000, 0b0000'0000, 0b0000'0000,
        0xDE, 0xAD, 0xBE, 0xEF, // (-2 * -1867964279) + 1
        0b1000'0000, 0b0000'0000, 0b0000'0000, 0b0000'0000,
        0xDE, 0xAD, 0xBE, 0xEE, // ((2 * 1867964279) - 1) + 1
    };
    auto br3 = BitReader(array3.data(), array3.size());
    br3.skip_bits(); // skip first bit for 31 zeroes
    QCOMPARE(br3.get_se_golomb(), -1867964279);
    QCOMPARE(br3.get_bits_left(), INT64_C(64));
    br3.skip_bits(); // skip first bit for 31 zeroes
    QCOMPARE(br3.get_se_golomb(), +1867964279);
    QCOMPARE(br3.get_bits_left(), INT64_C(0));
}

void TestBitReader::get_se_golomb2()
{
    constexpr std::array<uint8_t,8> array4 =
    {
        0b1000'0000, 0b0000'0000,
        0xDE, 0xAD, // (-2 * -28502) + 1
        0b1000'0000, 0b0000'0000,
        0xDE, 0xAC, // ((2 * 28502) - 1) + 1
    };
    auto br4 = BitReader(array4.data(), array4.size());
    br4.skip_bits(); // skip first bit for 15 zeroes
    QCOMPARE(br4.get_se_golomb(), -28502);
    QCOMPARE(br4.get_bits_left(), INT64_C(32));
    br4.skip_bits(); // skip first bit for 15 zeroes
    QCOMPARE(br4.get_se_golomb(), +28502);
    QCOMPARE(br4.get_bits_left(), INT64_C(0));
}

QTEST_APPLESS_MAIN(TestBitReader)
