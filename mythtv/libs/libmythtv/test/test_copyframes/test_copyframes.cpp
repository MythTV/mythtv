#include "mythcorecontext.h"
#include "test_copyframes.h"
#include "mythframe.h"

void TestCopyFrames::initTestCase(void)
{
}

void TestCopyFrames::TestInvalidFrames()
{
    MythVideoFrame dummy;
    QVERIFY(!MythVideoFrame::CopyFrame(nullptr, nullptr));
    QVERIFY(!MythVideoFrame::CopyFrame(nullptr, &dummy));
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy,  nullptr));
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy,  &dummy));
}

void TestCopyFrames::TestInvalidFormats()
{
    MythVideoFrame dummy1;
    MythVideoFrame dummy2;
    dummy1.codec = FMT_YV12;
    dummy2.codec = FMT_NV12;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    dummy1.codec = FMT_NONE;
    dummy2.codec = FMT_NONE;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    dummy1.codec = FMT_VAAPI;
    dummy2.codec = FMT_VAAPI;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
}

void TestCopyFrames::TestInvalidSizes()
{
    MythVideoFrame dummy1;
    MythVideoFrame dummy2;
    dummy1.codec = FMT_YV12;
    dummy2.codec = FMT_YV12;
    dummy1.width  = 720;
    dummy2.width  = 720;
    dummy1.height = 576;
    dummy2.height = 500;
    // Different heights
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Different widths
    dummy2.height = 576;
    dummy2.width  = 1024;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Invalid height
    dummy2.width = 720;
    dummy1.height = 0;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Invalid width
    dummy1.height = 576;
    dummy2.width  = 0;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
}

void TestCopyFrames::TestInvalidBuffers()
{
    MythVideoFrame dummy1;
    MythVideoFrame dummy2;
    // Both null buffers
    init(&dummy1, FMT_YV12, nullptr, 720, 576, 0);
    init(&dummy2, FMT_YV12, nullptr, 720, 576, 0);
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));

    size_t size1 = MythVideoFrame::GetBufferSize(FMT_YV12, 720, 576);
    unsigned char dummy = '0';
    unsigned char * buf1 = &dummy;
    unsigned char * buf2 = &dummy;

    // One null buffer
    dummy1.buf = buf1;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Two buffers, both zero sized
    dummy2.buf = buf2;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Same buffer
    dummy2.buf = buf1;
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
    // Invalid size
    dummy2.buf = buf2;
    dummy1.size = 16;
    dummy2.size = static_cast<int>(size1);
    QVERIFY(!MythVideoFrame::CopyFrame(&dummy1, &dummy2));
}

#include "mythmiscutil.h"

static uint64_t FillRandom(MythVideoFrame* Frame)
{
    uint64_t sum = 0;
    uint64_t counts = 0;
    uint count = MythVideoFrame::GetNumPlanes(Frame->codec);
    for (uint plane = 0; plane < count; ++plane)
    {
        int width = MythVideoFrame::GetPitchForPlane(Frame->codec, Frame->width, plane);
        int offset = Frame->offsets[plane];
        for (int i = 0; i < width; ++i)
        {
            unsigned char val = MythRandom() & 0xFF;
            Frame->buf[offset++] = val;
            sum += val;
            counts++;
        }
    }
    return sum;
}

static uint64_t GetSum(const MythVideoFrame* Frame)
{
    uint64_t sum = 0;
    uint64_t counts = 0;
    uint count = MythVideoFrame::GetNumPlanes(Frame->codec);
    for (uint plane = 0; plane < count; ++plane)
    {
        int width = MythVideoFrame::GetPitchForPlane(Frame->codec, Frame->width, plane);
        int offset = Frame->offsets[plane];
        for (int i = 0; i < width; ++i)
        {
            sum += Frame->buf[offset++];
            counts++;
        }
    }
    return sum;
}

void TestCopyFrames::TestCopy()
{
    // N.B. Over and above any *width* alignment, there is a default 16 height alignment in MythTV code
    // Note - these tests also infer correct behaviour of creating frames, not just copying
    std::array<int, 5> alignments({ 0, 16, 64, 96, 128});
    using frametest  = std::tuple<int, int, int, size_t, FramePitches, FrameOffsets>;
    using frametests = std::vector<frametest>;
    using framesuite = std::map<VideoFrameType, frametests>;
    // A semi-random selection of formats, sizes, alignments and extra offsets
    static const framesuite s_tests = {
    { FMT_YV12,
        {
            { 0,   720, 576, 622080,    { 720, 360, 360  }, { 0, 414720, 518400 } },
            { 64,  720, 576, 663552,    { 768, 384, 384  }, { 0, 442368, 552960 } },
            { 64,  720, 576, 763552,    { 768, 384, 384  }, { 100000, 542368, 652960 } },
            { 64,  720, 576, 763552,    { 768, 384, 384  }, { 0,      542368, 652960 } },
            { 128, 720, 576, 663552,    { 768, 384, 384  }, { 0, 442368, 552960 } },
            { 0,   1920, 540,  1566720, { 1920, 960, 960 }, { 0, 1044480, 1305600 } },
            { 0,   1920, 544,  1566720, { 1920, 960, 960 }, { 0, 1044480, 1305600 } },
            { 0,   1920, 1080, 3133440, { 1920, 960, 960 }, { 0, 2088960, 2611200 } },
            { 0,   1920, 1088, 3133440, { 1920, 960, 960 }, { 0, 2088960, 2611200 } },
            { 128, 1920, 1080, 3133440, { 1920, 960, 960 }, { 0, 2088960, 2611200 } },
            { 128, 1920, 1088, 3133440, { 1920, 960, 960 }, { 0, 2088960, 2611200 } }
        }
    },
    { FMT_YUV420P10,
        {
            { 0, 1920, 1080, 6266880, { 3840, 1920, 1920 }, { 0, 4177920, 522400 }}
        }
    },
    { FMT_RGB24,
        {
            { 0,  1000, 500, 1536000, { 3000, 0, 0 }, { 0, 0, 0 } },
            { 64, 1000, 500, 1572864, { 3072, 0, 0 }, { 0, 0, 0 } }
        }
    },
    { FMT_BGRA,
        {
            { 0,   1024, 576, 2359296, { 4096, 0, 0 }, { 0, 0, 0 } },
            { 256, 1024, 576, 2359296, { 4096, 0, 0 }, { 0, 0, 0 } }
        }
    },
    { FMT_YUV422P,
        {
            { 0,  1920, 1088, 4177920, { 1920, 960, 960 }, { 0, 2088960, 3133440 } },
            { 0,  1920, 1080, 4177920, { 1920, 960, 960 }, { 0, 2088960, 3133440 } },
            { 64, 1920, 1080, 4177920, { 1920, 960, 960 }, { 0, 2088960, 3133440 } }
        }
    },
    { FMT_YUV422P10,
        {
            { 64, 1920, 1080, 8355840, { 3840, 1920, 1920 }, { 0, 4177920, 6266880 } }
        }
    },
    { FMT_NV12,
        {
            { 0,   704, 480, 506880, { 704, 704, 0    }, { 0, 337920, 0      } },
            { 64,  704, 480, 506880, { 704, 704, 0    }, { 0, 337920, 0      } },
            { 128, 704, 480, 552960, { 768, 768, 0    }, { 0, 368640, 0      } },
            { 128, 704, 480, 552968, { 768, 768, 0    }, { 8, 368648, 0      } } // dodgy alignment
        }
    },
    { FMT_P010,
        {
            { 0, 3840, 2160, 24883200, { 7680, 7680, 0 }, { 0, 16588800, 0} }
        }
    },
    { FMT_P016,
        {
            { 0, 3840, 2160, 24883200, { 7680, 7680, 0 }, { 0, 16588800, 0} }
        }
    }
    };

    auto freeframe = [](MythVideoFrame* F) { av_free(F->buf); delete F; };

    auto gettestframe = [](VideoFrameType T, const frametest& P)
    {
        MythVideoFrame* frame = new MythVideoFrame;
        init(frame, T, MythVideoFrame::GetAlignedBufferZero(std::get<3>(P)), std::get<1>(P), std::get<2>(P),
             static_cast<int>(std::get<3>(P)), std::get<4>(P), std::get<5>(P), true, 1.0, 1.0, std::get<0>(P));
        return frame;
    };

    auto getdefaultframe = [](VideoFrameType T, int W, int H, int A)
    {
        size_t size = MythVideoFrame::GetBufferSize(T, W, H, A);
        MythVideoFrame* frame = new MythVideoFrame;
        init(frame, T, MythVideoFrame::GetAlignedBufferZero(size), W, H, static_cast<int>(size), 1.0, 1.0, A);
        return frame;
    };

    for (auto & tests : s_tests)
    {
        VideoFrameType type = tests.first;
        for (auto & test : tests.second)
        {
            auto * frame = gettestframe(type, test);
            auto sum     = FillRandom(frame);
            int width    = std::get<1>(test);
            int height   = std::get<2>(test);

            qInfo() << QString("Testing %1@%2x%3 Alignment: %4")
                       .arg(MythVideoFrame::FormatDescription(type)).arg(width)
                       .arg(height).arg(std::get<0>(test));
            // Test copy to various default created buffers with differing alignments
            for (auto alignment : alignments)
            {
                auto * to = getdefaultframe(type, width, height, alignment);
                QVERIFY(MythVideoFrame::CopyFrame(to, frame));
                QCOMPARE(sum, GetSum(to));
                freeframe(to);
            }

            // Test as destination from default created buffers...
            for (auto alignment : alignments)
            {
                auto * from = getdefaultframe(type, width, height, alignment);
                auto newsum = FillRandom(from);
                QVERIFY(MythVideoFrame::CopyFrame(frame, from));
                QCOMPARE(newsum, GetSum(frame));
                freeframe(from);
            }
            freeframe(frame);
        }
    }
}

QTEST_APPLESS_MAIN(TestCopyFrames)
