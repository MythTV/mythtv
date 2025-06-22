// C++ headers
#include <algorithm>
#include <cstdlib>
#include <thread> // for sleep_for

// FFmpeg headers
extern "C" {
#include "libavutil/frame.h"
}

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythcommflagplayer.h"

// Commercial Flagging headers
#include "ClassicCommDetector.h"
#include "ClassicLogoDetector.h"

struct EdgeMaskEntry
{
    int m_isEdge;
    int m_horiz;
    int m_vert;
    int m_rdiag;
    int m_ldiag;
};


ClassicLogoDetector::ClassicLogoDetector(ClassicCommDetector* commdetector,
                                         unsigned int w, unsigned int h,
                                         unsigned int commdetectborder_in)
    : LogoDetectorBase(w,h),
      m_commDetector(commdetector),
      m_commDetectBorder(commdetectborder_in),
      m_commDetectLogoSecondsNeeded((int)(1.3 * m_commDetectLogoSamplesNeeded *
                                          m_commDetectLogoSampleSpacing)),
      m_edgeMask(new EdgeMaskEntry[m_width * m_height]),
      // cppcheck doesn't understand deleteLater
      m_logoMaxValues(new unsigned char[m_width * m_height]),
      m_logoMinValues(new unsigned char[m_width * m_height]),
      m_logoFrame(new unsigned char[m_width * m_height]),
      m_logoMask(new unsigned char[m_width * m_height]),
      m_logoCheckMask(new unsigned char[m_width * m_height])
{
    m_commDetectLogoSamplesNeeded =
        gCoreContext->GetNumSetting("CommDetectLogoSamplesNeeded", 240);
    m_commDetectLogoSampleSpacing =
        gCoreContext->GetNumSetting("CommDetectLogoSampleSpacing", 2);
    m_commDetectLogoGoodEdgeThreshold =
        gCoreContext->GetSetting("CommDetectLogoGoodEdgeThreshold", "0.75")
        .toDouble();
    m_commDetectLogoBadEdgeThreshold =
        gCoreContext->GetSetting("CommDetectLogoBadEdgeThreshold", "0.85")
        .toDouble();
    m_commDetectLogoLocation =
        gCoreContext->GetSetting("CommDetectLogoLocation", "");
    m_commDetectLogoWidthRatio =
        gCoreContext->GetNumSetting("CommDetectLogoWidthRatio", 4);
    m_commDetectLogoHeightRatio =
        gCoreContext->GetNumSetting("CommDetectLogoHeightRatio", 4);
    m_commDetectLogoMinPixels =
        gCoreContext->GetNumSetting("CommDetectLogoMinPixels", 50);
}

ClassicLogoDetector::~ClassicLogoDetector()
{
    delete [] m_edgeMask;
    delete [] m_logoFrame;
    delete [] m_logoMask;
    delete [] m_logoCheckMask;
    delete [] m_logoMaxValues;
    delete [] m_logoMinValues;
}

unsigned int ClassicLogoDetector::getRequiredAvailableBufferForSearch()
{
    return m_commDetectLogoSecondsNeeded;
}

void ClassicLogoDetector::deleteLater(void)
{
    m_commDetector = nullptr;
    LogoDetectorBase::deleteLater();
}

bool ClassicLogoDetector::searchForLogo(MythCommFlagPlayer *player)
{
    int seekIncrement =
        (int)(m_commDetectLogoSampleSpacing * player->GetFrameRate());
    int maxLoops = m_commDetectLogoSamplesNeeded;
    const std::array<const int,9> edgeDiffs {5, 7, 10, 15, 20, 30, 40, 50, 60 };


    LOG(VB_COMMFLAG, LOG_INFO, "Searching for Station Logo");

    m_logoInfoAvailable = false;

    auto *edgeCounts = new EdgeMaskEntry[m_width * m_height];

    // Back in 2005, a threshold of 50 minimum pixelsInMask was established.
    // I don't know whether that was tested against SD or HD resolutions.
    // I do know that in 2010, mythcommflag was changed to use ffmpeg's
    // lowres support, effectively dividing the video area by 16.
    // But the 50 pixel minimum was not adjusted accordingly.
    // I believe the minimum threshold should vary with the video's area.
    // I am using 1280x720 (for 720p) video as the baseline.
    // This should improve logo detection for SD video.
    int minPixelsInMask =
        m_commDetectLogoMinPixels * (m_width*m_height) / (1280*720 / 16);

    for (uint edgediff : edgeDiffs)
    {
        int pixelsInMask = 0;

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Trying with edgeDiff == %1, minPixelsInMask=%2")
            .arg(edgediff).arg(minPixelsInMask));

        memset(edgeCounts, 0, sizeof(EdgeMaskEntry) * m_width * m_height);
        memset(m_edgeMask, 0, sizeof(EdgeMaskEntry) * m_width * m_height);

        player->DiscardVideoFrame(player->GetRawVideoFrame(0));

        int loops = 0;
        long long seekFrame = m_commDetector->m_preRoll + seekIncrement;
        while (loops < maxLoops && player->GetEof() == kEofStateNone)
        {
            MythVideoFrame* vf = player->GetRawVideoFrame(seekFrame);

            if ((loops % 50) == 0)
                m_commDetector->logoDetectorBreathe();

            if (m_commDetector->m_bStop)
            {
                player->DiscardVideoFrame(vf);
                delete[] edgeCounts;
                return false;
            }

            if (!m_commDetector->m_fullSpeed)
                std::this_thread::sleep_for(10ms);

            DetectEdges(vf, edgeCounts, edgediff);

            seekFrame += seekIncrement;
            loops++;

            player->DiscardVideoFrame(vf);
        }

        LOG(VB_COMMFLAG, LOG_INFO, "Analyzing edge data");

        for (uint y = 0; y < m_height; y++)
        {
            if ((y > (m_height/4)) && (y < (m_height * 3 / 4)))
                continue;

            for (uint x = 0; x < m_width; x++)
            {
                if ((x > (m_width/4)) && (x < (m_width * 3 / 4)))
                    continue;

                uint pos = (y * m_width) + x;

                if (edgeCounts[pos].m_isEdge > (maxLoops * 0.66))
                {
                    m_edgeMask[pos].m_isEdge = 1;
                    pixelsInMask++;
                }

                if (edgeCounts[pos].m_horiz > (maxLoops * 0.66))
                    m_edgeMask[pos].m_horiz = 1;

                if (edgeCounts[pos].m_vert > (maxLoops * 0.66))
                    m_edgeMask[pos].m_vert = 1;

                if (edgeCounts[pos].m_ldiag > (maxLoops * 0.66))
                    m_edgeMask[pos].m_ldiag = 1;
                if (edgeCounts[pos].m_rdiag > (maxLoops * 0.66))
                    m_edgeMask[pos].m_rdiag = 1;
            }
        }

        SetLogoMaskArea();

        for (uint y = m_logoMinY; y < m_logoMaxY; y++)
        {
            for (uint x = m_logoMinX; x < m_logoMaxX; x++)
            {
                int neighbors = 0;

                if (!m_edgeMask[(y * m_width) + x].m_isEdge)
                    continue;

                for (uint dy = y - 2; dy <= (y + 2); dy++ )
                {
                    for (uint dx = x - 2; dx <= (x + 2); dx++ )
                    {
                        if (m_edgeMask[(dy * m_width) + dx].m_isEdge)
                            neighbors++;
                    }
                }

                if (neighbors < 5)
                    m_edgeMask[(y * m_width) + x].m_isEdge = 0;
            }
        }

        SetLogoMaskArea();
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Testing Logo area: topleft (%1,%2), bottomright (%3,%4)")
                .arg(m_logoMinX).arg(m_logoMinY)
                .arg(m_logoMaxX).arg(m_logoMaxY));

        if (((m_logoMaxX - m_logoMinX) < (m_width  / m_commDetectLogoWidthRatio)) &&
            ((m_logoMaxY - m_logoMinY) < (m_height / m_commDetectLogoHeightRatio)) &&
            (pixelsInMask > minPixelsInMask))
        {
            m_logoInfoAvailable = true;
            m_logoEdgeDiff = edgediff;

            LOG(VB_COMMFLAG, LOG_INFO,
                QString("Using Logo area: topleft (%1,%2), "
                        "bottomright (%3,%4), pixelsInMask (%5).")
                    .arg(m_logoMinX).arg(m_logoMinY)
                    .arg(m_logoMaxX).arg(m_logoMaxY)
                    .arg(pixelsInMask));
            break;
        }

        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Rejecting Logo area: topleft (%1,%2), "
                    "bottomright (%3,%4), pixelsInMask (%5). "
                    "Not within specified limits.")
            .arg(m_logoMinX).arg(m_logoMinY)
            .arg(m_logoMaxX).arg(m_logoMaxY)
            .arg(pixelsInMask));
    }

    delete [] edgeCounts;

    if (!m_logoInfoAvailable)
        LOG(VB_COMMFLAG, LOG_NOTICE, "No suitable logo area found.");

    player->DiscardVideoFrame(player->GetRawVideoFrame(0));
    return m_logoInfoAvailable;
}


void ClassicLogoDetector::SetLogoMaskArea()
{
    LOG(VB_COMMFLAG, LOG_INFO, "SetLogoMaskArea()");

    m_logoMinX = m_width - 1;
    m_logoMaxX = 0;
    m_logoMinY = m_height - 1;
    m_logoMaxY = 0;

    for (unsigned int y = 0; y < m_height; y++)
    {
        for (unsigned int x = 0; x < m_width; x++)
        {
            if (m_edgeMask[(y * m_width) + x].m_isEdge)
            {
                m_logoMinX = std::min(x, m_logoMinX);
                m_logoMinY = std::min(y, m_logoMinY);
                m_logoMaxX = std::max(x, m_logoMaxX);
                m_logoMaxY = std::max(y, m_logoMaxY);
            }
        }
    }

    m_logoMinX -= 5;
    m_logoMaxX += 5;
    m_logoMinY -= 5;
    m_logoMaxY += 5;

    m_logoMinX = std::max<unsigned int>(m_logoMinX, 4);
    m_logoMaxX = std::min<size_t>(m_logoMaxX, m_width-5);
    m_logoMinY = std::max<unsigned int>(m_logoMinY, 4);
    m_logoMaxY = std::min<size_t>(m_logoMaxY, m_height-5);
}


void ClassicLogoDetector::DumpLogo(bool fromCurrentFrame,
    const unsigned char* framePtr)
{
    std::string scrPixels {" .oxX"};

    if (!m_logoInfoAvailable)
        return;

    std::cerr << "\nLogo Data ";
    if (fromCurrentFrame)
        std::cerr << "from current frame\n";

    std::cerr << "\n     ";

    for(unsigned int x = m_logoMinX - 2; x <= (m_logoMaxX + 2); x++)
        std::cerr << (x % 10);
    std::cerr << "\n";

    for(unsigned int y = m_logoMinY - 2; y <= (m_logoMaxY + 2); y++)
    {
        QString tmp = QString("%1: ").arg(y, 3);
        QString ba = tmp.toLatin1();
        std::cerr << ba.constData();
        for(unsigned int x = m_logoMinX - 2; x <= (m_logoMaxX + 2); x++)
        {
            if (fromCurrentFrame)
            {
                std::cerr << scrPixels[framePtr[(y * m_width) + x] / 50];
            }
            else
            {
                switch (m_logoMask[(y * m_width) + x])
                {
                        case 0:
                        case 2: std::cerr << " ";
                        break;
                        case 1: std::cerr << "*";
                        break;
                        case 3: std::cerr << ".";
                        break;
                }
            }
        }
        std::cerr << "\n";
    }
    std::cerr.flush();
}


/* ideas for this method ported back from comskip.c mods by Jere Jones
 * which are partially mods based on Myth's original commercial skip
 * code written by Chris Pinkham. */
bool ClassicLogoDetector::doesThisFrameContainTheFoundLogo(
    MythVideoFrame* frame)
{
    int radius = 2;
    int goodEdges = 0;
    int badEdges = 0;
    int testEdges = 0;
    int testNotEdges = 0;

    unsigned char* framePtr = frame->m_buffer;
    int bytesPerLine = frame->m_pitches[0];

    for (uint y = m_logoMinY; y <= m_logoMaxY; y++ )
    {
        for (uint x = m_logoMinX; x <= m_logoMaxX; x++ )
        {
            int pos1 = (y * bytesPerLine) + x;
            int edgePos = (y * m_width) + x;
            int pos2 = ((y - radius) * bytesPerLine) + x;
            int pos3 = ((y + radius) * bytesPerLine) + x;

            int pixel = framePtr[pos1];

            if (m_edgeMask[edgePos].m_horiz)
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= m_logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= m_logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= m_logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= m_logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }

            if (m_edgeMask[edgePos].m_vert)
            {
                if ((abs(framePtr[pos2] - pixel) >= m_logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= m_logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos2] - pixel) >= m_logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= m_logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }
        }
    }

    m_frameNumber++;
    double goodEdgeRatio = (testEdges) ?
        (double)goodEdges / (double)testEdges : 0.0;
    double badEdgeRatio = (testNotEdges) ?
        (double)badEdges / (double)testNotEdges : 0.0;
    return (goodEdgeRatio > m_commDetectLogoGoodEdgeThreshold) &&
           (badEdgeRatio < m_commDetectLogoBadEdgeThreshold);
}

bool ClassicLogoDetector::pixelInsideLogo(unsigned int x, unsigned int y)
{
    if (!m_logoInfoAvailable)
        return false;

    return ((x > m_logoMinX) && (x < m_logoMaxX) &&
            (y > m_logoMinY) && (y < m_logoMaxY));
}

void ClassicLogoDetector::DetectEdges(MythVideoFrame *frame, EdgeMaskEntry *edges,
                                      int edgeDiff)
{
    int r = 2;
    unsigned char *buf = frame->m_buffer;
    int bytesPerLine = frame->m_pitches[0];

    for (uint y = m_commDetectBorder + r; y < (m_height - m_commDetectBorder - r); y++)
    {
        if (
            (m_commDetectLogoLocation.contains("N") && (y > (m_height/4))) ||
            (m_commDetectLogoLocation.contains("S") && (y < (m_height * 3 / 4))) ||
            ((y > (m_height/4)) && (y < (m_height * 3 / 4)))
            )
            continue;

        for (uint x = m_commDetectBorder + r; x < (m_width - m_commDetectBorder - r); x++)
        {
            int edgeCount = 0;

            if (
                (m_commDetectLogoLocation.contains("W") && (x > (m_width/4))) ||
                (m_commDetectLogoLocation.contains("E") && (x < (m_width * 3 / 4))) ||
                ((x > (m_width/4)) && (x < (m_width * 3 / 4)))
                )
                continue;

            uint pos = (y * m_width) + x;
            uchar p = buf[(y * bytesPerLine) + x];

            if (( abs(buf[(y * bytesPerLine) + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[(y * bytesPerLine) + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].m_horiz++;
                edgeCount++;
            }
            if (( abs(buf[((y - r) * bytesPerLine) + x] - p) >= edgeDiff) ||
                ( abs(buf[((y + r) * bytesPerLine) + x] - p) >= edgeDiff))
            {
                edges[pos].m_vert++;
                edgeCount++;
            }

            if (( abs(buf[((y - r) * bytesPerLine) + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[((y + r) * bytesPerLine) + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].m_ldiag++;
                edgeCount++;
            }

            if (( abs(buf[((y - r) * bytesPerLine) + (x + r)] - p) >= edgeDiff) ||
                ( abs(buf[((y + r) * bytesPerLine) + (x - r)] - p) >= edgeDiff))
            {
                edges[pos].m_rdiag++;
                edgeCount++;
            }

            if (edgeCount >= 3)
                edges[pos].m_isEdge++;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
