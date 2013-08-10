/*
 *  Class TestAudioUtils
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <QtTest/QtTest>

#include "mythcorecontext.h"
#include "audiooutpututil.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

#define AOALIGN(x) (((long)&x + 15) & ~0xf);

#define SSEALIGN 16     // for 16 bytes memory alignment

#define ISIZEOF(type) ((int)sizeof(type))

class TestAudioUtils: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", NULL);
    }

    void S16ToFloatSSE_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless (SSE code)
    void S16ToFloatSSE(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY       = SAMPLES;
        short *arrays1      = (short*)av_malloc(SIZEARRAY * ISIZEOF(short));
        short *arrays2      = (short*)av_malloc(SIZEARRAY * ISIZEOF(short));
        float *arrayf       = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        short j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        int val1 = AudioOutputUtil::toFloat(FORMAT_S16, arrayf, arrays1, SAMPLES * ISIZEOF(short));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S16, arrays2, arrayf, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(short));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i] >= -1.0);
            QVERIFY(arrayf[i] <= 1.0);
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    void S16ToFloatC_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless (C code)
    void S16ToFloatC(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY       = SAMPLES;
        int offsetshort     = 1;
        int offsetfloat     = 1;

        short *arrays1      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        short *arrays2      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        float *arrayf       = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        short j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i+offsetshort] = j;
        }

        // +1 will never be 16-bytes aligned, forcing C-code
        int val1 = AudioOutputUtil::toFloat(FORMAT_S16, arrayf+offsetfloat, arrays1+offsetshort, SAMPLES * ISIZEOF(short));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S16, arrays2+offsetshort, arrayf+offsetfloat, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(short));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i+offsetshort], arrays2[i+offsetshort]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i+offsetfloat] >= -1.0);
            QVERIFY(arrayf[i+offsetfloat] <= 1.0);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test s16 -> float -> s16 SSE vs C-code. Also compare converted floats
    void S16ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 0;

        short *arrays1      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        // has to be 16 short for 16 bytes boundary * 2
        short *arrays2      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        short j = INT16_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetshort] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioOutputUtil::toFloat(FORMAT_S16, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(short));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S16, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(short));
        // Done by SSE
        val1 = AudioOutputUtil::toFloat(FORMAT_S16, arrayf2, arrays2, SIZEARRAY * ISIZEOF(short));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioOutputUtil::fromFloat(FORMAT_S16, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(short));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+offsetshort], arrays2[i]);
            QCOMPARE(arrayf1[i+offsetfloat], arrayf2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat] >= -1.0);
            QVERIFY(arrayf1[i+offsetfloat] <= 1.0);
            QVERIFY(arrayf2[i] >= -1.0);
            QVERIFY(arrayf2[i] <= 1.0);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    void S16ToFloatBoundsSSE_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    void S16ToFloatBoundsSSE(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY    = SAMPLES;
        int offsetshort = SSEALIGN / ISIZEOF(short);
        int offsetfloat = SSEALIGN / ISIZEOF(float);

        short *arrays1      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        // has to be 16 short for 16 bytes boundary * 2
        short *arrays2      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        float *arrayf       = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        short j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        uint32_t pattern        = 0xbcbcbcbc;
        arrays2[offsetshort-4]  = *(short*)&pattern;
        arrays2[offsetshort-3]  = *(short*)&pattern;
        arrays2[offsetshort-2]  = *(short*)&pattern;
        arrays2[offsetshort-1]  = *(short*)&pattern;
        arrayf[offsetfloat-4]   = *(float*)&pattern;
        arrayf[offsetfloat-3]   = *(float*)&pattern;
        arrayf[offsetfloat-2]   = *(float*)&pattern;
        arrayf[offsetfloat-1]   = *(float*)&pattern;
        arrays2[SAMPLES+offsetshort+0]    = *(short*)&pattern;
        arrays2[SAMPLES+offsetshort+1]    = *(short*)&pattern;
        arrays2[SAMPLES+offsetshort+2]    = *(short*)&pattern;
        arrays2[SAMPLES+offsetshort+3]    = *(short*)&pattern;
        arrayf[SAMPLES+offsetfloat+0]     = *(float*)&pattern;
        arrayf[SAMPLES+offsetfloat+1]     = *(float*)&pattern;
        arrayf[SAMPLES+offsetfloat+2]     = *(float*)&pattern;
        arrayf[SAMPLES+offsetfloat+3]     = *(float*)&pattern;

        // sanity tests
        QCOMPARE(SAMPLES*2, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+3],pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-4],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-3],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-2],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-1],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+0],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+1],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+2],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+3],*(short*)&pattern);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioOutputUtil::toFloat(FORMAT_S16, arrayf+offsetfloat, arrays1, SAMPLES * ISIZEOF(short));
        QCOMPARE(val1, SAMPLES * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S16, arrays2+offsetshort, arrayf+offsetfloat, SAMPLES * ISIZEOF(float));
        QCOMPARE(val2, SAMPLES * ISIZEOF(short));

        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SAMPLES+offsetfloat+3],pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-4],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-3],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-2],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[offsetshort-1],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+0],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+1],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+2],*(short*)&pattern);
        QCOMPARE(*(short*)&arrays2[SAMPLES+offsetshort+3],*(short*)&pattern);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    void S16ClipTest(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        short *arrays1      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        // has to be 16 short for 16 bytes boundary * 2
        short *arrays2      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

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
        AudioOutputUtil::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioOutputUtil::fromFloat(FORMAT_S16, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
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

    void S16ToFloatCvsSSESpeed_data(void)
    {
        QTest::addColumn<bool>("useSSE");
        QTest::newRow("Aligned memory") << true;
        QTest::newRow("Unaligned memory") << false;
    }

    void S16ToFloatCvsSSESpeed(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 1;

        short *arrays1      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        // has to be 16 short for 16 bytes boundary * 2
        short *arrays2      = (short*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(short));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        short j = INT16_MIN;
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
                    AudioOutputUtil::toFloat(FORMAT_S16, arrayf2, arrays2, SIZEARRAY * ISIZEOF(short));
                    AudioOutputUtil::fromFloat(FORMAT_S16, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
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
                    AudioOutputUtil::toFloat(FORMAT_S16, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(short));
                    AudioOutputUtil::fromFloat(FORMAT_S16, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // test u8 -> float -> u8 is lossless (SSE code)
    void U8ToFloatSSE(void)
    {
        int SIZEARRAY       = 256;
        uchar *arrays1      = (uchar*)av_malloc(SIZEARRAY * ISIZEOF(uchar));
        uchar *arrays2      = (uchar*)av_malloc(SIZEARRAY * ISIZEOF(uchar));
        float *arrayf       = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        uchar j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        int val1 = AudioOutputUtil::toFloat(FORMAT_U8, arrayf, arrays1, SIZEARRAY * ISIZEOF(uchar));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_U8, arrays2, arrayf, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uchar));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i] >= -1.0);
            QVERIFY(arrayf[i] <= 1.0);
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test u8 -> float -> u8 is lossless (C code)
    void U8ToFloatC(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = SSEALIGN / ISIZEOF(uchar);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        uchar *arrays1      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        uchar *arrays2      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        float *arrayf       = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uchar j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+1] = j;
        }

        int val1 = AudioOutputUtil::toFloat(FORMAT_U8, arrayf+1, arrays1+1, SIZEARRAY * ISIZEOF(uchar));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_U8, arrays2+1, arrayf+1, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uchar));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+1], arrays2[i+1]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf[i+1] >= -1.0);
            QVERIFY(arrayf[i+1] <= 1.0);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    // test u8 -> float -> u8 SSE vs C-code. Also compare converted floats
    void U8ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = 1;
        int offsetfloat =   0;

        uchar *arrays1      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        // has to be 16 short for 16 bytes boundary * 2
        uchar *arrays2      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uchar j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetuchar] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioOutputUtil::toFloat(FORMAT_U8, arrayf1+offsetfloat, arrays1+offsetuchar, SIZEARRAY * ISIZEOF(uchar));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_U8, arrays1+offsetuchar, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uchar));
        // Done by SSE
        val1 = AudioOutputUtil::toFloat(FORMAT_U8, arrayf2, arrays2, SIZEARRAY * ISIZEOF(uchar));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioOutputUtil::fromFloat(FORMAT_U8, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uchar));
        for (int i = 0; i < SIZEARRAY; i++)
        {
            QCOMPARE(arrays1[i+offsetuchar], arrays2[i]);
            QCOMPARE(arrayf1[i+offsetfloat], arrayf2[i]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat] >= -1.0);
            QVERIFY(arrayf1[i+offsetfloat] <= 1.0);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    void U8ToFloatBoundsSSE(void)
    {
        int SIZEARRAY       = 256;
        int offsetuchar     = SSEALIGN / ISIZEOF(uchar);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        uchar *arrays1      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        uchar *arrays2      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        float *arrayf       = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        uchar j = 0;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        uint32_t pattern        = 0xbcbcbcbc;
        arrays2[offsetuchar-4]  = *(uchar*)&pattern;
        arrays2[offsetuchar-3]  = *(uchar*)&pattern;
        arrays2[offsetuchar-2]  = *(uchar*)&pattern;
        arrays2[offsetuchar-1]  = *(uchar*)&pattern;
        arrayf[offsetfloat-4]   = *(float*)&pattern;
        arrayf[offsetfloat-3]   = *(float*)&pattern;
        arrayf[offsetfloat-2]   = *(float*)&pattern;
        arrayf[offsetfloat-1]   = *(float*)&pattern;
        arrays2[SIZEARRAY+offsetuchar+0]    = *(uchar*)&pattern;
        arrays2[SIZEARRAY+offsetuchar+1]    = *(uchar*)&pattern;
        arrays2[SIZEARRAY+offsetuchar+2]    = *(uchar*)&pattern;
        arrays2[SIZEARRAY+offsetuchar+3]    = *(uchar*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+0]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+1]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+2]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+3]     = *(float*)&pattern;

        // sanity tests
        QCOMPARE(SIZEARRAY*1, SIZEARRAY * ISIZEOF(arrays1[0]));
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+3],pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-4],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-3],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-2],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-1],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+0],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+1],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+2],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+3],*(uchar*)&pattern);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioOutputUtil::toFloat(FORMAT_U8, arrayf+offsetfloat, arrays1, SIZEARRAY * ISIZEOF(uchar));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_U8, arrays2+offsetuchar, arrayf+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(uchar));

        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+3],pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-4],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-3],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-2],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[offsetuchar-1],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+0],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+1],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+2],*(uchar*)&pattern);
        QCOMPARE(*(uchar*)&arrays2[SIZEARRAY+offsetuchar+3],*(uchar*)&pattern);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    void U8ClipTest(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetuchar     = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        uchar *arrays1      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        uchar *arrays2      = (uchar*)av_malloc((SIZEARRAY+offsetuchar+4) * ISIZEOF(uchar));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

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
        AudioOutputUtil::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioOutputUtil::fromFloat(FORMAT_U8, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
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
    void S32ToFloatCvsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 1;
        int offsetfloat1    = 0;
        int offsetfloat2    = 0;

        int32_t *arrays1    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        int32_t *arrays2    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        int j = INT_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i+offsetint32_t] = j;
            arrays2[i] = j;
        }

        // Done by C
        int val1 = AudioOutputUtil::toFloat(FORMAT_S32, arrayf1+offsetfloat1, arrays1+offsetint32_t, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S32, arrays1+offsetint32_t, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(int32_t));
        // Done by SSE
        val1 = AudioOutputUtil::toFloat(FORMAT_S32, arrayf2+offsetfloat2, arrays2, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        val2 = AudioOutputUtil::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
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
            QVERIFY(arrayf1[i+offsetfloat1] >= -1.0);
            QVERIFY(arrayf1[i+offsetfloat1] <= 1.0);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    // Test that float conversion only write where it's supposed to (SSE code)
    void S32ToFloatBoundsSSE(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        int offsetint32_t   = SSEALIGN / ISIZEOF(int32_t);
        int offsetfloat     = SSEALIGN / ISIZEOF(float);

        int32_t *arrays1    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        int32_t *arrays2    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        float *arrayf       = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        int j = INT_MIN;
        for (int i = 0; i < SIZEARRAY; i++, j++)
        {
            arrays1[i] = j;
        }

        uint32_t pattern        = 0xbcbcbcbc;
        arrays2[offsetint32_t-4]  = *(int32_t*)&pattern;
        arrays2[offsetint32_t-3]  = *(int32_t*)&pattern;
        arrays2[offsetint32_t-2]  = *(int32_t*)&pattern;
        arrays2[offsetint32_t-1]  = *(int32_t*)&pattern;
        arrayf[offsetfloat-4]   = *(float*)&pattern;
        arrayf[offsetfloat-3]   = *(float*)&pattern;
        arrayf[offsetfloat-2]   = *(float*)&pattern;
        arrayf[offsetfloat-1]   = *(float*)&pattern;
        arrays2[SIZEARRAY+offsetint32_t+0]    = *(int32_t*)&pattern;
        arrays2[SIZEARRAY+offsetint32_t+1]    = *(int32_t*)&pattern;
        arrays2[SIZEARRAY+offsetint32_t+2]    = *(int32_t*)&pattern;
        arrays2[SIZEARRAY+offsetint32_t+3]    = *(int32_t*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+0]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+1]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+2]     = *(float*)&pattern;
        arrayf[SIZEARRAY+offsetfloat+3]     = *(float*)&pattern;

        // sanity tests
        QCOMPARE(SIZEARRAY*4, SIZEARRAY * ISIZEOF(arrays1[0]));
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+3],pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-4],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-3],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-2],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-1],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+0],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+1],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+2],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+3],*(int32_t*)&pattern);
        QCOMPARE(arrayf+4,&arrayf[4]);
        QCOMPARE(arrays2+4,&arrays2[4]);

        int val1 = AudioOutputUtil::toFloat(FORMAT_S32, arrayf+offsetfloat, arrays1, SIZEARRAY * ISIZEOF(int32_t));
        QCOMPARE(val1, SIZEARRAY * ISIZEOF(float));
        int val2 = AudioOutputUtil::fromFloat(FORMAT_S32, arrays2+offsetint32_t, arrayf+offsetfloat, SIZEARRAY * ISIZEOF(float));
        QCOMPARE(val2, SIZEARRAY * ISIZEOF(int32_t));

        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-4],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-3],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[offsetfloat-1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+0],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+1],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+2],pattern);
        QCOMPARE(*(uint32_t*)&arrayf[SIZEARRAY+offsetfloat+3],pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-4],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-3],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-2],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[offsetint32_t-1],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+0],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+1],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+2],*(int32_t*)&pattern);
        QCOMPARE(*(int32_t*)&arrays2[SIZEARRAY+offsetint32_t+3],*(int32_t*)&pattern);

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    void S32ClipTest(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        int32_t *arrays1    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        int32_t *arrays2    = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

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
        AudioOutputUtil::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioOutputUtil::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
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

    void FloatToS32CvsSSESpeed_data(void)
    {
        QTest::addColumn<bool>("useSSE");
        QTest::newRow("Aligned memory") << true;
        QTest::newRow("Unaligned memory") << false;
    }

    void FloatToS32CvsSSESpeed(void)
    {
        int SIZEARRAY       = (INT16_MAX - INT16_MIN);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetshort     = 1;
        int offsetfloat     = 1;

        int32_t *arrays1    = (int32_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(int32_t));
        // has to be 16 short for 16 bytes boundary * 2
        int32_t *arrays2    = (int32_t*)av_malloc((SIZEARRAY+offsetshort+4) * ISIZEOF(int32_t));
        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat+4) * ISIZEOF(float));

        short j = INT16_MIN;
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
                    AudioOutputUtil::toFloat(FORMAT_S32, arrayf2, arrays2, SIZEARRAY * ISIZEOF(int32_t));
                    AudioOutputUtil::fromFloat(FORMAT_S32, arrays2, arrayf2, SIZEARRAY * ISIZEOF(float));
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
                    AudioOutputUtil::toFloat(FORMAT_S32, arrayf1+offsetfloat, arrays1+offsetshort, SIZEARRAY * ISIZEOF(int32_t));
                    AudioOutputUtil::fromFloat(FORMAT_S32, arrays1+offsetshort, arrayf1+offsetfloat, SIZEARRAY * ISIZEOF(float));
                }
            }
        }
        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    void FloatClipping(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        float *arrayf1      = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        float *arrayf2      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));
        float *arrayf3      = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

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
        AudioOutputUtil::fromFloat(FORMAT_FLT, arrayf2, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioOutputUtil::fromFloat(FORMAT_FLT, arrayf3, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            QCOMPARE(arrayf2[i], arrayf3[i]);
            QVERIFY(arrayf2[i] >= -1.0);
            QVERIFY(arrayf2[i] <= 1.0);
        }

        // test all float array are properly clipped
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        // also test in-place float conversion
        AudioOutputUtil::fromFloat(FORMAT_FLT, arrayf1+offsetfloat1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioOutputUtil::fromFloat(FORMAT_FLT, arrayf2+offsetfloat2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            // sanity check
            QCOMPARE(arrayf1[i+offsetfloat1], arrayf2[i+offsetfloat2]);
            // test that it's bound between -1 and 1
            QVERIFY(arrayf1[i+offsetfloat1] >= -1.0);
            QVERIFY(arrayf1[i+offsetfloat1] <= 1.0);
        }

        av_free(arrayf1);
        av_free(arrayf2);
        av_free(arrayf3);
    }
};
