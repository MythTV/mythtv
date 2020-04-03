#ifndef MYTHOPENGLTONEMAP_H
#define MYTHOPENGLTONEMAP_H

// Qt
#include <QObject>

// MythTV
#include "opengl/mythrenderopengl.h"
#include "videocolourspace.h"
#include "mythvideotexture.h"

class MythOpenGLTonemap : public QObject
{
    Q_OBJECT

  public:
    MythOpenGLTonemap(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace);
   ~MythOpenGLTonemap();

    MythVideoTexture* Map(vector<MythVideoTexture*> &Inputs, QSize DisplaySize);
    MythVideoTexture* GetTexture(void);

  public slots:
    void UpdateColourSpace(bool PrimariesChanged);

  private:
    Q_DISABLE_COPY(MythOpenGLTonemap)

    bool CreateShader(size_t InputSize, VideoFrameType Type, QSize Size);
    bool CreateTexture(QSize Size);

    MythRenderOpenGL*      m_render       { nullptr  };
    QOpenGLExtraFunctions* m_extra        { nullptr  };
    VideoColourSpace*      m_colourSpace  { nullptr  };
    QOpenGLShaderProgram*  m_shader       { nullptr  };
    GLuint                 m_storageBuffer{ 0        };
    MythVideoTexture*      m_texture      { nullptr  };
    size_t                 m_inputCount   { 0        };
    QSize                  m_inputSize    { 0, 0     };
    VideoFrameType         m_inputType    { FMT_NONE };
    QSize                  m_outputSize   { 0, 0     };
};

#endif // MYTHOPENGLTONEMAP_H
