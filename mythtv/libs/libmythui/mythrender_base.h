#ifndef MYTHRENDER_H_
#define MYTHRENDER_H_

#include <QSize>

typedef enum
{
    kRenderUnknown = 0,
    kRenderDirect3D9,
    kRenderVDPAU,
    kRenderOpenGL1,
    kRenderOpenGL2,
    kRenderOpenGL2ES,
} RenderType;

class MythRender
{
  public:
    MythRender(RenderType type)
      : m_type(type), m_size(QSize()), m_errored(false) { }
    virtual  ~MythRender() { }

    RenderType Type(void)                { return m_type;     }
    bool  IsErrored(void) const          { return m_errored;  }
    QSize GetSize(void) const            { return m_size;     }

  protected:
    RenderType    m_type;
    QSize         m_size;
    bool          m_errored;
};

#endif
