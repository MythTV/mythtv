#ifndef MYTHRENDER_OPENGL2ES_H
#define MYTHRENDER_OPENGL2ES_H

#include "mythrender_opengl2.h"

class MUI_PUBLIC MythRenderOpenGL2ES : public MythRenderOpenGL2
{
  public:
    MythRenderOpenGL2ES(const MythRenderFormat& format, QPaintDevice* device);
    explicit MythRenderOpenGL2ES(const MythRenderFormat& format);

    // MythRenderOpenGL2
    void InitProcs(void) override;
    bool InitFeatures(void) override;
};

#endif // MYTHRENDER_OPENGL2ES_H
