/*
 *  Class TestCopyFrames
 *
 *  Copyright (C) Jean-Yves Avenard / Bubblestuff Pty Ltd 2014
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

#include "mythcorecontext.h"
#include "mythframe.h"
#include "mythavutil.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define MSKIP(MSG) QSKIP(MSG, SkipSingle)
#else
#define MSKIP(MSG) QSKIP(MSG)
#endif

#define ITER    48*30
#define WIDTH   720
#define HEIGHT  576

class TestCopyFrames: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", NULL);
    }

    void YV12copy_data(void)
    {
        QTest::addColumn<bool>("SSE");
        QTest::newRow("SSE") << true;
        QTest::newRow("Pure C") << false;
    }

    // YV12 -> YV12 SSE
    void YV12copy(void)
    {
        QFETCH(bool, SSE);
        VideoFrame src, dst;
        int ALIGN = 64;
        int ALIGNDST = 0;
        int sizesrc = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_YV12, bufsrc, WIDTH, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);

        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride / 2, src.pitches[1]);
        QCOMPARE(stride / 2, src.pitches[2]);

        // Fill up the src frame with data
        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            memset(src.buf + src.offsets[1] + src.pitches[1] * i, i % 255, WIDTH / 2);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            memset(src.buf + src.offsets[2] + src.pitches[2] * i, i % 255, WIDTH / 2);
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);

        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);
        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        // test the stride sizes
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src, SSE);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
            }
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[2] + i * src.pitches[2] + j),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12copy_data(void)
    {
        QTest::addColumn<bool>("SSE");
        QTest::newRow("SSE") << true;
        QTest::newRow("Pure C") << false;
    }

    // NV12 -> YV12
    void NV12copy(void)
    {
        QFETCH(bool, SSE);
        int ALIGN = 64;
        int ALIGNDST = 0;
        VideoFrame src, dst;
        int sizesrc = buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src, SSE);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12SSEcopy_data(void)
    {
        QTest::addColumn<int>("ALIGN");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE, various stride aligned sizes
    void NV12SSEcopy(void)
    {
        QFETCH(int, ALIGN);
        const int ALIGNDST = 0;
        VideoFrame src, dst;
        int sizesrc = buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);
        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12StrideAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12StrideAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);

        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12SSEcopySrcNotAligned_data(void)
    {
        QTest::addColumn<int>("ALIGN");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12SSEcopySrcNotAligned(void)
    {
        QFETCH(int, ALIGN);
        const int ALIGNDST = 0;
        VideoFrame src, dst;
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST /* align */);
        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12StrideAligned_DstNotAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12StrideAligned_DstNotAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST);

        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12StrideAligned_NeitherAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("7")  << 32;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12StrideAligned_NeitherAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST);

        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12VariousWidth_data(void)
    {
        QTest::addColumn<int>("width");
        QTest::newRow("1080") << 1080;
        QTest::newRow("1440") << 1440;
        QTest::newRow("720") << 720;
        QTest::newRow("600") << 600;
        QTest::newRow("300") << 300;
    }

    // NV12 -> YV12
    void NV12VariousWidth(void)
    {
        QFETCH(int, width);
        int ALIGN = 64;
        int ALIGNDST = 0;
        VideoFrame src, dst;
        int sizesrc = buffersize(FMT_NV12, width, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_NV12, bufsrc, width, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (width + ALIGN - 1) & ~(ALIGN -1) : width;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, width);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < width / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, width, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, width, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                framecopy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < width; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < width / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    // YV12 -> YV12 USWC
    void YV12USWCcopy(void)
    {
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        int ALIGN = 64;
        int ALIGNDST = 0;
        int sizesrc = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_YV12, bufsrc, WIDTH, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);

        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride / 2, src.pitches[1]);
        QCOMPARE(stride / 2, src.pitches[2]);

        // Fill up the src frame with data
        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            memset(src.buf + src.offsets[1] + src.pitches[1] * i, i % 255, WIDTH / 2);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            memset(src.buf + src.offsets[2] + src.pitches[2] * i, i % 255, WIDTH / 2);
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);

        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);
        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        // test the stride sizes
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
            }
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[2] + i * src.pitches[2] + j),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCcopy_data(void)
    {
        QTest::addColumn<int>("ALIGN");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE, various stride aligned sizes
    void NV12USWCcopy(void)
    {
        QFETCH(int, ALIGN);
        const int ALIGNDST = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        int sizesrc = buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);
        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCStrideAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12USWCStrideAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);

        int stride2 = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride2, dst.pitches[0]);
        QCOMPARE(stride2 / 2, dst.pitches[1]);
        QCOMPARE(stride2 / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCcopySrcNotAligned_data(void)
    {
        QTest::addColumn<int>("ALIGN");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12USWCcopySrcNotAligned(void)
    {
        QFETCH(int, ALIGN);
        const int ALIGNDST = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST /* align */);
        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCStrideAligned_DstNotAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12USWCStrideAligned_DstNotAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST);

        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCStrideAligned_NeitherAligned_data(void)
    {
        QTest::addColumn<int>("ALIGNDST");
        QTest::newRow("64") << 64;
        QTest::newRow("32") << 32;
        QTest::newRow("16") << 16;
        QTest::newRow("7")  << 32;
        QTest::newRow("0")  << 0;
    }

    // NV12 -> YV12 SSE
    void NV12USWCStrideAligned_NeitherAligned(void)
    {
        QFETCH(int, ALIGNDST);
        const int ALIGN = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        unsigned char* bufsrc =
            (unsigned char*)av_malloc(buffersize(FMT_NV12, WIDTH, HEIGHT, ALIGN));

        init(&src, FMT_NV12, bufsrc + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGN),
             NULL, NULL, 0, 0, ALIGN);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[0]);
        QCOMPARE(ALIGN ? (WIDTH + ALIGN - 1) & ~(ALIGN -1) : WIDTH , src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, WIDTH);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        unsigned char* bufdst =
            (unsigned char*)av_malloc(buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST));

        init(&dst, FMT_YV12, bufdst + 1, WIDTH, HEIGHT, buffersize(FMT_YV12, WIDTH, HEIGHT, ALIGNDST),
             NULL, NULL, 0, 0, ALIGNDST);

        int stride = ALIGNDST ? (WIDTH + ALIGNDST - 1) & ~(ALIGNDST -1) : WIDTH;
        QCOMPARE(stride, dst.pitches[0]);
        QCOMPARE(stride / 2, dst.pitches[1]);
        QCOMPARE(stride / 2, dst.pitches[2]);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < WIDTH; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < WIDTH / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }

    void NV12USWCVariousWidth_data(void)
    {
        QTest::addColumn<int>("width");
        QTest::newRow("1080") << 1080;
        QTest::newRow("1440") << 1440;
        QTest::newRow("720") << 720;
        QTest::newRow("600") << 600;
        QTest::newRow("300") << 300;
    }

    // NV12 -> YV12
    void NV12USWCVariousWidth(void)
    {
        QFETCH(int, width);
        int ALIGN = 64;
        int ALIGNDST = 0;
        VideoFrame src, dst;
        MythUSWCCopy mythcopy(WIDTH);
        mythcopy.setUSWC(true);
        int sizesrc = buffersize(FMT_NV12, width, HEIGHT, ALIGN);
        unsigned char* bufsrc = (unsigned char*)av_malloc(sizesrc);

        init(&src, FMT_NV12, bufsrc, width, HEIGHT, sizesrc,
             NULL, NULL, 0, 0, ALIGN);
        int stride = ALIGN ? (width + ALIGN - 1) & ~(ALIGN -1) : width;
        QCOMPARE(stride, src.pitches[0]);
        QCOMPARE(stride, src.pitches[1]);

        for (int i = 0; i < HEIGHT; i++)
        {
            memset(src.buf + src.offsets[0] + src.pitches[0] * i, i % 255, width);
        }
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < width / 2; j++)
            {
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2) = i % 255;
                *(src.buf + src.offsets[1] + src.pitches[1] * i + j * 2 + 1) = i % 255;
            }
        }

        int sizedst = buffersize(FMT_YV12, width, HEIGHT, ALIGNDST);
        unsigned char* bufdst = (unsigned char*)av_malloc(sizedst);
        init(&dst, FMT_YV12, bufdst, width, HEIGHT, sizedst,
             NULL, NULL, 0, 0, ALIGNDST);

        QBENCHMARK
        {
            for (int i = 0; i < ITER; i++)
            {
                mythcopy.copy(&dst, &src);
            }
        }

        // test the copy was okay
        // Y channel
        for (int i = 0; i < HEIGHT; i++)
        {
            for (int j = 0; j < width; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[0] + i * src.pitches[0] + j),
                         *(dst.buf + dst.offsets[0] + i * dst.pitches[0] + j));
            }
        }
        // test deinterleaving of U and V channels was okay
        for (int i = 0; i < HEIGHT / 2; i++)
        {
            for (int j = 0; j < width / 2; j++)
            {
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2),
                         *(dst.buf + dst.offsets[1] + i * dst.pitches[1] + j));
                QCOMPARE(*(src.buf + src.offsets[1] + i * src.pitches[1] + j * 2 + 1),
                         *(dst.buf + dst.offsets[2] + i * dst.pitches[2] + j));
            }
        }

        av_freep(&bufsrc);
        av_freep(&bufdst);
    }
};
