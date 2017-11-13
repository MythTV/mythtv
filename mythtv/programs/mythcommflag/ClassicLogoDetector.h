#ifndef _CLASSICLOGOGEDETECTOR_H_
#define _CLASSICLOGOGEDETECTOR_H_

#include "LogoDetectorBase.h"

typedef struct edgemaskentry EdgeMaskEntry;
typedef struct VideoFrame_ VideoFrame;
class ClassicCommDetector;

class ClassicLogoDetector : public LogoDetectorBase
{
  public:
    ClassicLogoDetector(ClassicCommDetector* commDetector,unsigned int width,
        unsigned int height, unsigned int commdetectborder);
    virtual void deleteLater(void);

    bool searchForLogo(MythPlayer* player);
    bool doesThisFrameContainTheFoundLogo(VideoFrame* frame);
    bool pixelInsideLogo(unsigned int x, unsigned int y);

    unsigned int getRequiredAvailableBufferForSearch();

  protected:
    virtual ~ClassicLogoDetector() {}

  private:
    void SetLogoMaskArea();
    void DumpLogo(bool fromCurrentFrame,unsigned char* framePtr);
    void DetectEdges(VideoFrame *frame, EdgeMaskEntry *edges, int edgeDiff);

    ClassicCommDetector* commDetector;
    unsigned int frameNumber;
    unsigned int commDetectBorder;

    int commDetectLogoSamplesNeeded;
    int commDetectLogoSampleSpacing;
    int commDetectLogoSecondsNeeded;
    double commDetectLogoGoodEdgeThreshold;
    double commDetectLogoBadEdgeThreshold;

    EdgeMaskEntry *edgeMask;

    unsigned char *logoMaxValues;
    unsigned char *logoMinValues;
    unsigned char *logoFrame;
    unsigned char *logoMask;
    unsigned char *logoCheckMask;
    unsigned char *tmpBuf;

    int logoEdgeDiff;
    unsigned int logoMinX;
    unsigned int logoMaxX;
    unsigned int logoMinY;
    unsigned int logoMaxY;

    bool logoInfoAvailable;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

