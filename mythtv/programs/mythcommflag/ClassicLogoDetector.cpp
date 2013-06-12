// POSIX headers
#include <unistd.h>

// ANSI C headers
#include <cstdlib>

// MythTV headers
#include "mythcorecontext.h"
#include "mythplayer.h"

// Commercial Flagging headers
#include "ClassicLogoDetector.h"
#include "ClassicCommDetector.h"

typedef struct edgemaskentry
{
    int isedge;
    int horiz;
    int vert;
    int rdiag;
    int ldiag;
}
EdgeMaskEntry;


ClassicLogoDetector::ClassicLogoDetector(ClassicCommDetector* commdetector,
                                         unsigned int w, unsigned int h,
                                         unsigned int commdetectborder_in,
                                         unsigned int xspacing_in,
                                         unsigned int yspacing_in)
    : LogoDetectorBase(w,h),
      commDetector(commdetector),                       frameNumber(0),
      previousFrameWasSceneChange(false),
      xspacing(xspacing_in),                            yspacing(yspacing_in),
      commDetectBorder(commdetectborder_in),            edgeMask(new EdgeMaskEntry[width * height]),
      logoMaxValues(new unsigned char[width * height]), logoMinValues(new unsigned char[width * height]),
      logoFrame(new unsigned char[width * height]),     logoMask(new unsigned char[width * height]),
      logoCheckMask(new unsigned char[width * height]), tmpBuf(new unsigned char[width * height]),
      logoEdgeDiff(0),                                  logoFrameCount(0),
      logoMinX(0),                                      logoMaxX(0),
      logoMinY(0),                                      logoMaxY(0),
      logoInfoAvailable(false)
{
    commDetectLogoSamplesNeeded =
        gCoreContext->GetNumSetting("CommDetectLogoSamplesNeeded", 240);
    commDetectLogoSampleSpacing =
        gCoreContext->GetNumSetting("CommDetectLogoSampleSpacing", 2);
    commDetectLogoSecondsNeeded = (int)(1.3 * commDetectLogoSamplesNeeded *
                                              commDetectLogoSampleSpacing);
    commDetectLogoGoodEdgeThreshold =
        gCoreContext->GetSetting("CommDetectLogoGoodEdgeThreshold", "0.75")
        .toDouble();
    commDetectLogoBadEdgeThreshold =
        gCoreContext->GetSetting("CommDetectLogoBadEdgeThreshold", "0.85")
        .toDouble();
}

unsigned int ClassicLogoDetector::getRequiredAvailableBufferForSearch()
{
    return commDetectLogoSecondsNeeded;
}

void ClassicLogoDetector::deleteLater(void)
{
    commDetector = 0;
    if (edgeMask)
        delete [] edgeMask;
    if (logoFrame)
        delete [] logoFrame;
    if (logoMask)
        delete [] logoMask;
    if (logoCheckMask)
        delete [] logoCheckMask;
    if (logoMaxValues)
        delete [] logoMaxValues;
    if (logoMinValues)
        delete [] logoMinValues;
    if (tmpBuf)
        delete [] tmpBuf;

    LogoDetectorBase::deleteLater();
}

bool ClassicLogoDetector::searchForLogo(MythPlayer* player)
{
    int seekIncrement = 
        (int)(commDetectLogoSampleSpacing * player->GetFrameRate());
    long long seekFrame;
    int loops;
    int maxLoops = commDetectLogoSamplesNeeded;
    EdgeMaskEntry *edgeCounts;
    unsigned int pos, i, x, y, dx, dy;
    int edgeDiffs[] = {5, 7, 10, 15, 20, 30, 40, 50, 60, 0 };


    LOG(VB_COMMFLAG, LOG_INFO, "Searching for Station Logo");

    logoInfoAvailable = false;

    edgeCounts = new EdgeMaskEntry[width * height];

    for (i = 0; edgeDiffs[i] != 0 && !logoInfoAvailable; i++)
    {
        int pixelsInMask = 0;

        LOG(VB_COMMFLAG, LOG_INFO, QString("Trying with edgeDiff == %1")
                .arg(edgeDiffs[i]));

        memset(edgeCounts, 0, sizeof(EdgeMaskEntry) * width * height);
        memset(edgeMask, 0, sizeof(EdgeMaskEntry) * width * height);

        player->DiscardVideoFrame(player->GetRawVideoFrame(0));

        loops = 0;
        seekFrame = commDetector->preRoll + seekIncrement;
        while (loops < maxLoops && player->GetEof() == kEofStateNone)
        {
            VideoFrame* vf = player->GetRawVideoFrame(seekFrame);

            if ((loops % 50) == 0)
                commDetector->logoDetectorBreathe();

            if (commDetector->m_bStop)
            {
                player->DiscardVideoFrame(vf);
                delete[] edgeCounts;
                return false;
            }

            if (!commDetector->fullSpeed)
                usleep(10000);

            DetectEdges(vf, edgeCounts, edgeDiffs[i]);

            seekFrame += seekIncrement;
            loops++;

            player->DiscardVideoFrame(vf);
        }

        LOG(VB_COMMFLAG, LOG_INFO, "Analyzing edge data");

#ifdef SHOW_DEBUG_WIN
        unsigned char *fakeFrame;
        fakeFrame = new unsigned char[width * height * 3 / 2];
        memset(fakeFrame, 0, width * height * 3 / 2);
#endif

        for (y = 0; y < height; y++)
        {
            if ((y > (height/4)) && (y < (height * 3 / 4)))
                continue;

            for (x = 0; x < width; x++)
            {
                if ((x > (width/4)) && (x < (width * 3 / 4)))
                    continue;

                pos = y * width + x;

                if (edgeCounts[pos].isedge > (maxLoops * 0.66))
                {
                    edgeMask[pos].isedge = 1;
                    pixelsInMask++;
#ifdef SHOW_DEBUG_WIN
                    fakeFrame[pos] = 0xff;
#endif

                }

                if (edgeCounts[pos].horiz > (maxLoops * 0.66))
                    edgeMask[pos].horiz = 1;

                if (edgeCounts[pos].vert > (maxLoops * 0.66))
                    edgeMask[pos].vert = 1;

                if (edgeCounts[pos].ldiag > (maxLoops * 0.66))
                    edgeMask[pos].ldiag = 1;
                if (edgeCounts[pos].rdiag > (maxLoops * 0.66))
                    edgeMask[pos].rdiag = 1;
            }
        }

        SetLogoMaskArea();

        for (y = logoMinY; y < logoMaxY; y++)
        {
            for (x = logoMinX; x < logoMaxX; x++)
            {
                int neighbors = 0;

                if (!edgeMask[y * width + x].isedge)
                    continue;

                for (dy = y - 2; dy <= (y + 2); dy++ )
                {
                    for (dx = x - 2; dx <= (x + 2); dx++ )
                    {
                        if (edgeMask[dy * width + dx].isedge)
                            neighbors++;
                    }
                }

                if (neighbors < 5)
                    edgeMask[y * width + x].isedge = 0;
            }
        }

        SetLogoMaskArea();
        LOG(VB_COMMFLAG, LOG_INFO,
            QString("Testing Logo area: topleft (%1,%2), bottomright (%3,%4)")
                .arg(logoMinX).arg(logoMinY)
                .arg(logoMaxX).arg(logoMaxY));

#ifdef SHOW_DEBUG_WIN
        for (x = logoMinX; x < logoMaxX; x++)
        {
            pos = logoMinY * width + x;
            fakeFrame[pos] = 0x7f;
            pos = logoMaxY * width + x;
            fakeFrame[pos] = 0x7f;
        }
        for (y = logoMinY; y < logoMaxY; y++)
        {
            pos = y * width + logoMinX;
            fakeFrame[pos] = 0x7f;
            pos = y * width + logoMaxX;
            fakeFrame[pos] = 0x7f;
        }

        comm_debug_show(fakeFrame);
        delete [] fakeFrame;

        cerr << "Hit ENTER to continue" << endl;
        getchar();
#endif
        if (((logoMaxX - logoMinX) < (width / 4)) &&
            ((logoMaxY - logoMinY) < (height / 4)) &&
            (pixelsInMask > 50))
        {
            logoInfoAvailable = true;
            logoEdgeDiff = edgeDiffs[i];

            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("Using Logo area: topleft (%1,%2), "
                        "bottomright (%3,%4)")
                    .arg(logoMinX).arg(logoMinY)
                    .arg(logoMaxX).arg(logoMaxY));
        }
        else
        {
            LOG(VB_COMMFLAG, LOG_INFO, 
                QString("Rejecting Logo area: topleft (%1,%2), "
                        "bottomright (%3,%4), pixelsInMask (%5). "
                        "Not within specified limits.")
                    .arg(logoMinX).arg(logoMinY)
                    .arg(logoMaxX).arg(logoMaxY)
                    .arg(pixelsInMask));
        }
    }

    delete [] edgeCounts;

    if (!logoInfoAvailable)
        LOG(VB_COMMFLAG, LOG_NOTICE, "No suitable logo area found.");

    player->DiscardVideoFrame(player->GetRawVideoFrame(0));
    return logoInfoAvailable;
}


void ClassicLogoDetector::SetLogoMaskArea()
{
    LOG(VB_COMMFLAG, LOG_INFO, "SetLogoMaskArea()");

    logoMinX = width - 1;
    logoMaxX = 0;
    logoMinY = height - 1;
    logoMaxY = 0;

    for (unsigned int y = 0; y < height; y++)
    {
        for (unsigned int x = 0; x < width; x++)
        {
            if (edgeMask[y * width + x].isedge)
            {
                if (x < logoMinX)
                    logoMinX = x;
                if (y < logoMinY)
                    logoMinY = y;
                if (x > logoMaxX)
                    logoMaxX = x;
                if (y > logoMaxY)
                    logoMaxY = y;
            }
        }
    }

    logoMinX -= 5;
    logoMaxX += 5;
    logoMinY -= 5;
    logoMaxY += 5;

    if (logoMinX < 4)
        logoMinX = 4;
    if (logoMaxX > (width-5))
        logoMaxX = (width-5);
    if (logoMinY < 4)
        logoMinY = 4;
    if (logoMaxY > (height-5))
        logoMaxY = (height-5);
}

void ClassicLogoDetector::SetLogoMask(unsigned char *mask)
{
    int pixels = 0;

    memcpy(logoMask, mask, width * height);

    SetLogoMaskArea();

    for(unsigned int y = logoMinY; y <= logoMaxY; y++)
        for(unsigned int x = logoMinX; x <= logoMaxX; x++)
            if (!logoMask[y * width + x] == 1)
                pixels++;

    if (pixels < 30)
        return;

    // set the pixels around our logo
    for(unsigned int y = (logoMinY - 1); y <= (logoMaxY + 1); y++)
    {
        for(unsigned int x = (logoMinX - 1); x <= (logoMaxX + 1); x++)
        {
            if (!logoMask[y * width + x])
            {
                for (unsigned int y2 = y - 1; y2 <= (y + 1); y2++)
                {
                    for (unsigned int x2 = x - 1; x2 <= (x + 1); x2++)
                    {
                        if ((logoMask[y2 * width + x2] == 1) &&
                            (!logoMask[y * width + x]))
                        {
                            logoMask[y * width + x] = 2;
                            x2 = x + 2;
                            y2 = y + 2;

                            logoCheckMask[y2 * width + x2] = 1;
                            logoCheckMask[y * width + x] = 1;
                        }
                    }
                }
            }
        }
    }

    for(unsigned int y = (logoMinY - 2); y <= (logoMaxY + 2); y++)
    {
        for(unsigned int x = (logoMinX - 2); x <= (logoMaxX + 2); x++)
        {
            if (!logoMask[y * width + x])
            {
                for (unsigned int y2 = y - 1; y2 <= (y + 1); y2++)
                {
                    for (unsigned int x2 = x - 1; x2 <= (x + 1); x2++)
                    {
                        if ((logoMask[y2 * width + x2] == 2) &&
                            (!logoMask[y * width + x]))
                        {
                            logoMask[y * width + x] = 3;
                            x2 = x + 2;
                            y2 = y + 2;

                            logoCheckMask[y * width + x] = 1;
                        }
                    }
                }
            }
        }
    }

#ifdef SHOW_DEBUG_WIN
    DumpLogo(true,framePtr);
#endif

    logoFrameCount = 0;
    logoInfoAvailable = true;
}


void ClassicLogoDetector::DumpLogo(bool fromCurrentFrame,
    unsigned char* framePtr)
{
    char scrPixels[] = " .oxX";

    if (!logoInfoAvailable)
        return;

    cerr << "\nLogo Data ";
    if (fromCurrentFrame)
        cerr << "from current frame\n";

    cerr << "\n     ";

    for(unsigned int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        cerr << (x % 10);
    cerr << "\n";

    for(unsigned int y = logoMinY - 2; y <= (logoMaxY + 2); y++)
    {
        QString tmp = QString("%1: ").arg(y, 3);
        QString ba = tmp.toLatin1();
        cerr << ba.constData();
        for(unsigned int x = logoMinX - 2; x <= (logoMaxX + 2); x++)
        {
            if (fromCurrentFrame)
            {
                cerr << scrPixels[framePtr[y * width + x] / 50];
            }
            else
            {
                switch (logoMask[y * width + x])
                {
                        case 0:
                        case 2: cerr << " ";
                        break;
                        case 1: cerr << "*";
                        break;
                        case 3: cerr << ".";
                        break;
                }
            }
        }
        cerr << "\n";
    }
    cerr.flush();
}


/* ideas for this method ported back from comskip.c mods by Jere Jones
 * which are partially mods based on Myth's original commercial skip
 * code written by Chris Pinkham. */
bool ClassicLogoDetector::doesThisFrameContainTheFoundLogo(
    unsigned char* framePtr)
{
    int radius = 2;
    unsigned int x, y;
    int pos1, pos2, pos3;
    int pixel;
    int goodEdges = 0;
    int badEdges = 0;
    int testEdges = 0;
    int testNotEdges = 0;

    for (y = logoMinY; y <= logoMaxY; y++ )
    {
        for (x = logoMinX; x <= logoMaxX; x++ )
        {
            pos1 = y * width + x;
            pos2 = (y - radius) * width + x;
            pos3 = (y + radius) * width + x;

            pixel = framePtr[pos1];

            if (edgeMask[pos1].horiz)
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos1 - radius] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos1 + radius] - pixel) >= logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }

            if (edgeMask[pos1].vert)
            {
                if ((abs(framePtr[pos2] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= logoEdgeDiff))
                    goodEdges++;
                testEdges++;
            }
            else
            {
                if ((abs(framePtr[pos2] - pixel) >= logoEdgeDiff) ||
                    (abs(framePtr[pos3] - pixel) >= logoEdgeDiff))
                    badEdges++;
                testNotEdges++;
            }
        }
    }

    frameNumber++;
    double goodEdgeRatio = (testEdges) ?
        (double)goodEdges / (double)testEdges : 0.0;
    double badEdgeRatio = (testNotEdges) ?
        (double)badEdges / (double)testNotEdges : 0.0;
    if ((goodEdgeRatio > commDetectLogoGoodEdgeThreshold) &&
        (badEdgeRatio < commDetectLogoBadEdgeThreshold))
        return true;
    else
        return false;
}

bool ClassicLogoDetector::pixelInsideLogo(unsigned int x, unsigned int y)
{
    if (!logoInfoAvailable)
        return false;

    return ((x > logoMinX) && (x < logoMaxX) &&
            (y > logoMinY) && (y < logoMaxY));
}

void ClassicLogoDetector::DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges,
                                      int edgeDiff)
{
    int r = 2;
    unsigned char *buf = frame->buf;
    unsigned char p;
    unsigned int pos, x, y;

    for (y = commDetectBorder + r; y < (height - commDetectBorder - r); y++)
    {
        if ((y > (height/4)) && (y < (height * 3 / 4)))
            continue;

        for (x = commDetectBorder + r; x < (width - commDetectBorder - r); x++)
        {
            int edgeCount = 0;

            if ((x > (width/4)) && (x < (width * 3 / 4)))
                continue;

            pos = y * width + x;
            p = buf[pos];

            if (( abs(buf[y * width + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[y * width + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].horiz++;
                edgeCount++;
            }
            if (( abs(buf[(y - r) * width + x] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + x] - p) >= edgeDiff))
            {
                edges[pos].vert++;
                edgeCount++;
            }

            if (( abs(buf[(y - r) * width + (x - r)] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + (x + r)] - p) >= edgeDiff))
            {
                edges[pos].ldiag++;
                edgeCount++;
            }

            if (( abs(buf[(y - r) * width + (x + r)] - p) >= edgeDiff) ||
                ( abs(buf[(y + r) * width + (x - r)] - p) >= edgeDiff))
            {
                edges[pos].rdiag++;
                edgeCount++;
            }

            if (edgeCount >= 3)
                edges[pos].isedge++;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

