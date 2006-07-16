#ifndef _LOGODETECTORBASE_H_
#define _LOGODETECTORBASE_H_

#include "qobject.h"

class NuppelVideoPlayer;

class LogoDetectorBase : public QObject
{
    Q_OBJECT

public:
    LogoDetectorBase(unsigned int w,unsigned int h) :
        foundLogo(false), width(w),height(h) {};
    ~LogoDetectorBase() {};

    virtual bool searchForLogo(NuppelVideoPlayer* nvp) = 0;
    virtual bool doesThisFrameContainTheFoundLogo(unsigned char* frame) = 0;
    virtual bool pixelInsideLogo(unsigned int x, unsigned int y) = 0;
    virtual unsigned int getRequiredAvailableBufferForSearch() = 0;

signals:
    void haveNewInformation(unsigned int framenum, bool haslogo,
                            float debugValue = 0.0);

protected:
    bool foundLogo;
    unsigned int width, height;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

