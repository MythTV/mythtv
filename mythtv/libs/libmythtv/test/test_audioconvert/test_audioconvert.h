/*
 *  Class TestAudioConvert
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
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

#include "libmythbase/mythcorecontext.h"
#include "libmythtv/audio/audioconvert.h"

#define SSEALIGN 16     // for 16 bytes memory alignment

#define ISIZEOF(type) ((int)sizeof(type))

class TestAudioConvert: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    static void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", nullptr);
    }

    static void Identical_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16
    static void Identical(void)
    {
        QFETCH(int, SAMPLES);

        int   SIZEARRAY = SAMPLES;
        auto *arrays1   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac = AudioConvert(FORMAT_S16, FORMAT_S16);

        int val1 = ac.Process(arrays2, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }
        av_free(arrays1);
        av_free(arrays2);
    }

    static void S16ToFloat_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless
    static void S16ToFloat(void)
    {
        QFETCH(int, SAMPLES);

        int   SIZEARRAY = SAMPLES;
        auto *arrays1   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrayf    = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert acf = AudioConvert(FORMAT_S16, FORMAT_FLT);
        AudioConvert acs = AudioConvert(FORMAT_FLT, FORMAT_S16);

        int val1 = acf.Process(arrayf, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrayf[0]));
        int val2 = acs.Process(arrays2, arrayf, SAMPLES * ISIZEOF(arrayf[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void S16ToS24LSB_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24LSB -> S16 is lossless
    static void S16ToS24LSB(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays24 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S16, FORMAT_S24LSB);
        AudioConvert acs    = AudioConvert(FORMAT_S24LSB, FORMAT_S16);

        int val1 = ac24.Process(arrays24, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays24[0]));
        int val2 = acs.Process(arrays2, arrays24, SAMPLES * ISIZEOF(arrays24[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // Check we are indeed getting a 24 bits int
            QVERIFY(arrays24[i] >= -(1<<23));
            QVERIFY(arrays24[i] <= ((1<<23)-1));
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays24);
    }

    static void S24LSBToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    static void S24LSBToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays2  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S24LSB, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S24LSB);

        int val1 = ac24.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S16ToS24_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24 -> S16 is lossless
    static void S16ToS24(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays24 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j << 8;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S16, FORMAT_S24);
        AudioConvert acs    = AudioConvert(FORMAT_S24, FORMAT_S16);

        int val1 = ac24.Process(arrays24, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays24[0]));
        int val2 = acs.Process(arrays2, arrays24, SAMPLES * ISIZEOF(arrays24[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // Check we are indeed getting a 24 bits int
            QCOMPARE(arrays24[i] & ~0xffff, arrays24[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays24);
    }

    static void S24ToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    static void S24ToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays2  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j << 8;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_S24, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S24);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S16ToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24 -> S16 is lossless
    static void S16ToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_S16, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S16);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void U8ToS16_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << 256;
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test U8 -> S16 -> U8 is lossless
    static void U8ToS16(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = 256;

        auto *arrays1  = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrays2  = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrays32 = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));

        uint8_t j = 0;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_U8, FORMAT_S16);
        AudioConvert acs    = AudioConvert(FORMAT_S16, FORMAT_U8);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S32ClipTest(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.0;
        arrayf2[1+offsetfloat2] = -1.0;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.0;
        arrayf2[7+offsetfloat2] = 1.0;
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void FloatS32ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatS32ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0;
        arrays2[0] = -2147483648;
        arrays2[1] = -2147483648;
        arrays2[2] = -2147483648;
        arrays2[3] = -2147483648;
        arrays2[4] = 2147483520;   // (1<<31)-128
        arrays2[5] = 2147483520;
        arrays2[6] = 2147483520;
        arrays2[7] = 2147483520;
        arrays2[8] = 0;
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }

    static void FloatS16ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatS16ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint16_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint16_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0.0;
        arrays2[0] = -32768;
        arrays2[1] = -32768;
        arrays2[2] = -32768;
        arrays2[3] = -32768;
        arrays2[4] = 32767;
        arrays2[5] = 32767;
        arrays2[6] = 32767;
        arrays2[7] = 32767;
        arrays2[8] = 0;
        AudioConvert::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }

    static void FloatU8ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatU8ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint8_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint8_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0.0;
        arrays2[0] = 0;
        arrays2[1] = 0;
        arrays2[2] = 0;
        arrays2[3] = 0;
        arrays2[4] = 255;
        arrays2[5] = 255;
        arrays2[6] = 255;
        arrays2[7] = 255;
        arrays2[8] = 128;
        AudioConvert::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }

    static void S16ToFloatSSE_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless (SSE code)
    static void S16ToFloatSSE(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY = SAMPLES;
        auto *arrays1 = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2 = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrayf  = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        int val1 = AudioConvert::toFloat(FORMAT_S16, arrayf, arrays1, SAMPLES * ISIZEOF(uint16_t));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S16, arrays2, arrayf, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(uint16_t));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i] >= -1.0F);
            QVERIFY(arrayf[i] <= 1.0F);
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void S16ToFloatC_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless (C code)
    static void S16ToFloatC(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY       = SAMPLES;
        int offsetshort     = 1;
        int offsetfloat     = 1;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrayf  = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i+offsetshort] = j;
        }

        // +1 will never be 16-bytes aligned, forcing C-code
        int val1 = AudioConvert::toFloat(FORMAT_S16, arrayf+offsetfloat, arrays1+offsetshort, SAMPLES * ISIZEOF(uint16_t));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S16, arrays2+offsetshort, arrayf+offsetfloat, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(uint16_t));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i+offsetshort], arrays2[i+offsetshort]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i+offsetfloat] >= -1.0F);
            QVERIFY(arrayf[i+offsetfloat] <= 1.0F);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test s16 -> float -> s16 SSE vs C-code. Also compare converted floats
    static void S16ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 0;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetshort] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioConvert::toFloat(FORMAT_S16, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(uint16_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S16, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint16_t));
        // Done by SSE
        val1 = AudioConvert::toFloat(FORMAT_S16, arrayf2, arrays2, SIZEARRAY * ISIZEOF(uint16_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioConvert::fromFloat(FORMAT_S16, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint16_t));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+offsetshort], arrays2[i]);
            QCOMPARE(arrayf1[i+offsetfloat], arrayf2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat] >= -1.0F);
            QVERIFY(arrayf1[i+offsetfloat] <= 1.0F);
            QVERIFY(arrayf2[i] >= -1.0F);
            QVERIFY(arrayf2[i] <= 1.0F);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void S16ToFloatBoundsSSE_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    static void S16ToFloatBoundsSSE(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY    = SAMPLES;
        int offsetshort = SSEALIGN / ISIZEOF(uint16_t);
        int offsetfloat = SSEALIGN / ISIZEOF(float);

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrayf  = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        uint16_t pattern_16 = 0;
        float pattern_f = 0;
        memset(&pattern_16, 0xbc, sizeof(pattern_16));
        memset(&pattern_f, 0xbc, sizeof(pattern_f));
        memset(&arrays2[offsetshort - 4], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrays2[SAMPLES + offsetshort], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrayf[offsetfloat - 4], 0xbc, sizeof(*arrayf) * 4);
        memset(&arrayf[SAMPLES + offsetfloat], 0xbc, sizeof(*arrayf) * 4);

        // sanity tests
        QCOMPARE(SAMPLES*2, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetshort - 4], pattern_16);
        QCOMPARE(arrays2[offsetshort - 3], pattern_16);
        QCOMPARE(arrays2[offsetshort - 2], pattern_16);
        QCOMPARE(arrays2[offsetshort - 1], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 0], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 1], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 2], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 3], pattern_16);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioConvert::toFloat(FORMAT_S16, arrayf+offsetfloat, arrays1, SAMPLES * ISIZEOF(uint16_t));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S16, arrays2+offsetshort, arrayf+offsetfloat, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(uint16_t));

        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SAMPLES + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetshort - 4], pattern_16);
        QCOMPARE(arrays2[offsetshort - 3], pattern_16);
        QCOMPARE(arrays2[offsetshort - 2], pattern_16);
        QCOMPARE(arrays2[offsetshort - 1], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 0], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 1], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 2], pattern_16);
        QCOMPARE(arrays2[SAMPLES + offsetshort + 3], pattern_16);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void S16ClipTest(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.2;
        arrayf2[1+offsetfloat2] = -1.1;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.1;
        arrayf2[7+offsetfloat2] = 1.2;
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        AudioConvert::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_S16, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            // sanity check
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i+offsetfloat2]);

            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void S16ToFloatCvsSSESpeed_data(void)
    {
        QTest::addColumn<bool>("useSSE");
        QTest::newRow("Aligned memory") << true;
        QTest::newRow("Unaligned memory") << false;
    }

    static void S16ToFloatCvsSSESpeed(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 1;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(uint16_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetshort] = j;
            arrays2[i] = j;
        }

        QFETCH(bool, useSSE);

        if (useSSE)
        {
            QBENCHMARK
            {
                for (int i = 0; i < 128; i++)
                {
                    // Done by SSE
                    AudioConvert::toFloat(FORMAT_S16, arrayf2, arrays2, SIZEARRAY * ISIZEOF(uint16_t));
                    AudioConvert::fromFloat(FORMAT_S16, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        else
        {
            QBENCHMARK
            {
                for (int i = 0; i < 128; i++)
                {
                    // Done by C
                    AudioConvert::toFloat(FORMAT_S16, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(uint16_t));
                    AudioConvert::fromFloat(FORMAT_S16, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // test u8 -> float -> u8 is lossless (SSE code)
    static void U8ToFloatSSE(void)
    {
        int SIZEARRAY = 256;
        auto *arrays1 = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrays2 = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrayf  = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        uint8_t j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        int val1 = AudioConvert::toFloat(FORMAT_U8, arrayf, arrays1, SIZEARRAY * ISIZEOF(uint8_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_U8, arrays2, arrayf, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint8_t));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i] >= -1.0F);
            QVERIFY(arrayf[i] <= 1.0F);
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test u8 -> float -> u8 is lossless (C code)
    static void U8ToFloatC(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = SSEALIGN / ISIZEOF(uint8_t);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrayf  = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint8_t j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+1] = j;
        }

        int val1 = AudioConvert::toFloat(FORMAT_U8, arrayf+1, arrays1+1, SIZEARRAY * ISIZEOF(uint8_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_U8, arrays2+1, arrayf+1, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint8_t));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+1], arrays2[i+1]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i+1] >= -1.0F);
            QVERIFY(arrayf[i+1] <= 1.0F);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test u8 -> float -> u8 SSE vs C-code. Also compare converted floats
    static void U8ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = 1;
        int offsetfloat =   0;

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint8_t j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetuchar] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioConvert::toFloat(FORMAT_U8, arrayf1+offsetfloat, arrays1+offsetuchar, SIZEARRAY * ISIZEOF(uint8_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_U8, arrays1+offsetuchar, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint8_t));
        // Done by SSE
        val1 = AudioConvert::toFloat(FORMAT_U8, arrayf2, arrays2, SIZEARRAY * ISIZEOF(uint8_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioConvert::fromFloat(FORMAT_U8, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint8_t));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+offsetuchar], arrays2[i]);
            QCOMPARE(arrayf1[i+offsetfloat], arrayf2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat] >= -1.0F);
            QVERIFY(arrayf1[i+offsetfloat] <= 1.0F);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    static void U8ToFloatBoundsSSE(void)
    {
        int SIZEARRAY       = 256;
        int offsetuchar     = SSEALIGN / ISIZEOF(uint8_t);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrayf  = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint8_t j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        uint8_t pattern_8 = 0;
        float pattern_f = 0;
        memset(&pattern_8, 0xbc, sizeof(pattern_8));
        memset(&pattern_f, 0xbc, sizeof(pattern_f));
        memset(&arrays2[offsetuchar - 4], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrays2[SIZEARRAY + offsetuchar], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrayf[offsetfloat - 4], 0xbc, sizeof(*arrayf) * 4);
        memset(&arrayf[SIZEARRAY + offsetfloat], 0xbc, sizeof(*arrayf) * 4);

        // sanity tests
        QCOMPARE(SIZEARRAY*1, SIZEARRAY * ISIZEOF(arrays1[0]));
        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetuchar - 4], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 3], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 2], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 1], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 0], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 1], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 2], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 3], pattern_8);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioConvert::toFloat(FORMAT_U8, arrayf+offsetfloat, arrays1, SIZEARRAY * ISIZEOF(uint8_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_U8, arrays2+offsetuchar, arrayf+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uint8_t));

        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetuchar - 4], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 3], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 2], pattern_8);
        QCOMPARE(arrays2[offsetuchar - 1], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 0], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 1], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 2], pattern_8);
        QCOMPARE(arrays2[SIZEARRAY + offsetuchar + 3], pattern_8);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void U8ClipTest(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uint8_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.2;
        arrayf2[1+offsetfloat2] = -1.1;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.1;
        arrayf2[7+offsetfloat2] = 1.2;
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        AudioConvert::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_U8, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            // sanity check
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i+offsetfloat2]);
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // test s16 -> float -> s16 SSE vs C-code. Also compare converted floats
    static void S32ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 1;
        int offsetfloat1    = 0;
        int offsetfloat2    = 0;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        int j = INT_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetint32_t] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioConvert::toFloat(FORMAT_S32, arrayf1+offsetfloat1, arrays1+offsetint32_t, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S32, arrays1+offsetint32_t, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(int32_t));
        // Done by SSE
        val1 = AudioConvert::toFloat(FORMAT_S32, arrayf2+offsetfloat2, arrays2, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioConvert::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(int32_t));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            //fprintf(stderr, "arrayf1[%d]=%f (%.14g) arrayf2[%d]=%f arrayf1[i]: >=-1:%d <=1:%d\n",
            //        i, arrayf1[i+offsetfloat1], (double)arrayf1[i+offsetfloat1],
            //        i, arrayf2[i+offsetfloat2],
            //        arrayf1[i+offsetfloat1] >= -1.0, arrayf1[i+offsetfloat1] <= 1.0);
            QCOMPARE(arrays1[i+offsetint32_t], arrays2[i]);
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i]+offsetfloat2);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat1] >= -1.0F);
            QVERIFY(arrayf1[i+offsetfloat1] <= 1.0F);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    static void S32ToFloatBoundsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        int offsetint32_t   = SSEALIGN / ISIZEOF(int32_t);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf  = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        int j = INT_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        int32_t pattern_32 = 0;
        float pattern_f = 0;
        memset(&pattern_32, 0xbc, sizeof(pattern_32));
        memset(&pattern_f, 0xbc, sizeof(pattern_f));
        memset(&arrays2[offsetint32_t - 4], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrays2[SIZEARRAY + offsetint32_t], 0xbc, sizeof(*arrays2) * 4);
        memset(&arrayf[offsetfloat - 4], 0xbc, sizeof(*arrayf) * 4);
        memset(&arrayf[SIZEARRAY + offsetfloat], 0xbc, sizeof(*arrayf) * 4);

        // sanity tests
        QCOMPARE(SIZEARRAY*4, SIZEARRAY * ISIZEOF(arrays1[0]));
        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetint32_t - 4], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 3], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 2], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 1], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 0], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 1], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 2], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 3], pattern_32);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioConvert::toFloat(FORMAT_S32, arrayf+offsetfloat, arrays1, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioConvert::fromFloat(FORMAT_S32, arrays2+offsetint32_t, arrayf+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(int32_t));

        QCOMPARE(arrayf[offsetfloat - 4], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 3], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 2], pattern_f);
        QCOMPARE(arrayf[offsetfloat - 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 0], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 1], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 2], pattern_f);
        QCOMPARE(arrayf[SIZEARRAY + offsetfloat + 3], pattern_f);
        QCOMPARE(arrays2[offsetint32_t - 4], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 3], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 2], pattern_32);
        QCOMPARE(arrays2[offsetint32_t - 1], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 0], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 1], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 2], pattern_32);
        QCOMPARE(arrays2[SIZEARRAY + offsetint32_t + 3], pattern_32);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void S32ClipTest2(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.2;
        arrayf2[1+offsetfloat2] = -1.1;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.1;
        arrayf2[7+offsetfloat2] = 1.2;
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            // sanity check
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i+offsetfloat2]);

            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void FloatToS32CvsSSESpeed_data(void)
    {
        QTest::addColumn<bool>("useSSE");
        QTest::newRow("Aligned memory") << true;
        QTest::newRow("Unaligned memory") << false;
    }

    static void FloatToS32CvsSSESpeed(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 1;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(int32_t));
        // has to be 16 uint16_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetshort] = j;
            arrays2[i] = j;
        }

        QFETCH(bool, useSSE);

        if (useSSE)
        {
            QBENCHMARK
            {
                for (int i = 0; i < 128; i++)
                {
                    // Done by SSE
                    AudioConvert::toFloat(FORMAT_S32, arrayf2, arrays2, SIZEARRAY * ISIZEOF(int32_t));
                    AudioConvert::fromFloat(FORMAT_S32, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        else
        {
            QBENCHMARK
            {
                for (int i = 0; i < 128; i++)
                {
                    // Done by C
                    AudioConvert::toFloat(FORMAT_S32, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(int32_t));
                    AudioConvert::fromFloat(FORMAT_S32, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void FloatClipping(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));
        auto *arrayf3 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.2;
        arrayf2[1+offsetfloat2] = -1.1;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.1;
        arrayf2[7+offsetfloat2] = 1.2;
        // arrayf2 is produced by C-code
        // arrayf3 is produced by SSE
        AudioConvert::fromFloat(FORMAT_FLT, arrayf2, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_FLT, arrayf3, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            QCOMPARE(arrayf2[i], arrayf3[i]);
            QVERIFY(arrayf2[i] >= -1.0F);
            QVERIFY(arrayf2[i] <= 1.0F);
        }

        // test all float array are properly clipped
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        // also test in-place float conversion
        AudioConvert::fromFloat(FORMAT_FLT, arrayf1+offsetfloat1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_FLT, arrayf2+offsetfloat2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            // sanity check
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i+offsetfloat2]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat1] >= -1.0F);
            QVERIFY(arrayf1[i+offsetfloat1] <= 1.0F);
        }

        av_free(arrayf1);
        av_free(arrayf2);
        av_free(arrayf3);
    }
};
