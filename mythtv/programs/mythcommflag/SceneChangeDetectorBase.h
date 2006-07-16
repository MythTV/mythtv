#ifndef _SCENECHANGEDETECTORBASE_H_
#define _SCENECHANGEDETECTORBASE_H_

#include "qobject.h"

class SceneChangeDetectorBase : public QObject
{
        Q_OBJECT

public:
        SceneChangeDetectorBase(unsigned int w, unsigned int h) :
            width(w), height(h) {}
        ~SceneChangeDetectorBase() {};

        virtual void processFrame(unsigned char* frame) = 0;

signals:
        void haveNewInformation(unsigned int framenum, bool scenechange,
                                float debugValue = 0.0);

protected:
       unsigned int width, height;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */

