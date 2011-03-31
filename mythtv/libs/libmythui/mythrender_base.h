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

typedef enum
{
    kMasterUI    = 0,
    kMasterVideo = 1,
} RenderMaster;

class MythRender
{
  public:
    MythRender(RenderType type)
      : m_type(type), m_master(kMasterUI), m_size(QSize()), m_errored(false) { }
    virtual  ~MythRender() { }

    RenderType Type(void)                { return m_type;     }
    void  SetMaster(RenderMaster master) { m_master = master; }
    bool  IsErrored(void) const          { return m_errored;  }
    QSize GetSize(void) const            { return m_size;     }

  protected:
    RenderType    m_type;
    RenderMaster  m_master;
    QSize         m_size;
    bool          m_errored;
};

#endif
