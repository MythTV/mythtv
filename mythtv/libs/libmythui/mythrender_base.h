#ifndef MYTHRENDER_H_
#define MYTHRENDER_H_

#include <QSize>
#include <QMutex>

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
      : m_type(type), m_size(QSize()), m_errored(false), m_refCount(0)
    {
        UpRef();
    }

    bool IsShared(void)
    {
        return m_refCount > 1;
    }

    void UpRef(void)
    {
        m_refLock.lock();
        m_refCount++;
        m_refLock.unlock();
    }

    void DownRef(void)
    {
        m_refLock.lock();
        m_refCount--;
        if (m_refCount <= 0)
        {
            m_refLock.unlock();
            delete this;
            return;
        }
        m_refLock.unlock();
   }

    RenderType Type(void)       { return m_type;    }
    bool  IsErrored(void) const { return m_errored; }
    QSize GetSize(void) const   { return m_size;    }

  protected:
    virtual  ~MythRender() { }

    RenderType  m_type;
    QSize       m_size;
    bool        m_errored;
    QMutex      m_refLock;
    int         m_refCount;
};

#endif
