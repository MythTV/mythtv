#ifndef MYTHRENDER_H_
#define MYTHRENDER_H_

#include <QSize>

typedef enum
{
    kMasterUI    = 0,
    kMasterVideo = 1,
} RenderMaster;

class MythRender
{
  public:
    MythRender() : m_master(kMasterUI), m_size(QSize()), m_errored(false) { }
    virtual  ~MythRender() { }

    void  SetMaster(RenderMaster master) { m_master = master; }
    bool  IsErrored(void) const          { return m_errored;  }
    QSize GetSize(void) const            { return m_size;     }

  protected:
    RenderMaster  m_master;
    QSize         m_size;
    bool          m_errored;
};

#endif
