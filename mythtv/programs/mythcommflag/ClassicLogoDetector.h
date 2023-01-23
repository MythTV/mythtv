#ifndef CLASSICLOGOGEDETECTOR_H
#define CLASSICLOGOGEDETECTOR_H

#include "LogoDetectorBase.h"

struct EdgeMaskEntry;
class ClassicCommDetector;

class ClassicLogoDetector : public LogoDetectorBase
{
  public:
    ClassicLogoDetector(ClassicCommDetector* commDetector,unsigned int width,
        unsigned int height, unsigned int commdetectborder);
    virtual void deleteLater(void);

    bool searchForLogo(MythCommFlagPlayer* player) override; // LogoDetectorBase
    bool doesThisFrameContainTheFoundLogo(MythVideoFrame* frame) override; // LogoDetectorBase
    bool pixelInsideLogo(unsigned int x, unsigned int y) override; // LogoDetectorBase

    unsigned int getRequiredAvailableBufferForSearch() override; // LogoDetectorBase

  protected:
    ~ClassicLogoDetector() override;

  private:
    void SetLogoMaskArea();
    void DumpLogo(bool fromCurrentFrame,const unsigned char* framePtr);
    void DetectEdges(MythVideoFrame *frame, EdgeMaskEntry *edges, int edgeDiff);

    ClassicCommDetector *m_commDetector                    {nullptr};
    unsigned int         m_frameNumber                     {0};
    unsigned int         m_commDetectBorder                {16};

    int                  m_commDetectLogoSamplesNeeded     {240};
    int                  m_commDetectLogoSampleSpacing     {2};
    int                  m_commDetectLogoSecondsNeeded     {624};
    int                  m_commDetectLogoWidthRatio        {4};
    int                  m_commDetectLogoHeightRatio       {4};
    int                  m_commDetectLogoMinPixels         {50};
    double               m_commDetectLogoGoodEdgeThreshold {0.75};
    double               m_commDetectLogoBadEdgeThreshold  {0.85};
    QString              m_commDetectLogoLocation;

    EdgeMaskEntry       *m_edgeMask                        {nullptr};

    unsigned char       *m_logoMaxValues                   {nullptr};
    unsigned char       *m_logoMinValues                   {nullptr};
    unsigned char       *m_logoFrame                       {nullptr};
    unsigned char       *m_logoMask                        {nullptr};
    unsigned char       *m_logoCheckMask                   {nullptr};

    int                  m_logoEdgeDiff                    {0};
    unsigned int         m_logoMinX                        {0};
    unsigned int         m_logoMaxX                        {0};
    unsigned int         m_logoMinY                        {0};
    unsigned int         m_logoMaxY                        {0};

    bool                 m_logoInfoAvailable               {false};
};

#endif // CLASSICLOGOGEDETECTOR_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */

