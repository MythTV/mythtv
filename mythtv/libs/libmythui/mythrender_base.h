#ifndef MYTHRENDER_H_
#define MYTHRENDER_H_

#include <QString>
#include <QMutex>
#include <QSize>

#include "referencecounter.h"
#include "mythuiexp.h"

enum RenderType
{
    kRenderUnknown = 0,
    kRenderDirect3D9,
    kRenderVDPAU,
    kRenderOpenGL
};

class MUI_PUBLIC MythRender : public ReferenceCounter
{
  public:
    explicit MythRender(RenderType type) :
        ReferenceCounter(QString("MythRender:%1").arg(type)),
        m_type(type), m_size(QSize()), m_errored(false)
    {
    }

    /// Warning: The reference count can be decremented between
    /// the call to this function and the use of it's value.
    bool IsShared(void) const
    {
        return const_cast<QAtomicInt&>(m_referenceCount)
            .fetchAndAddOrdered(0) > 1;
    }

    RenderType Type(void) const { return m_type;    }
    bool  IsErrored(void) const { return m_errored; }
    QSize GetSize(void) const   { return m_size;    }
    virtual void ReleaseResources(void) { }

  protected:
    virtual  ~MythRender() = default;

    RenderType  m_type;
    QSize       m_size;
    bool        m_errored;
};

#endif
