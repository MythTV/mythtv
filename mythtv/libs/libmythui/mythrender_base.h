#ifndef MYTHRENDER_H_
#define MYTHRENDER_H_

// Qt
#include <QString>
#include <QMutex>
#include <QRect>
#include <QSize>
#include <QStringList>

// MythTV
#include "libmythbase/referencecounter.h"
#include "mythuiexp.h"

enum RenderType
{
    kRenderUnknown = 0,
    kRenderDirect3D9,
    kRenderOpenGL,
    kRenderVulkan
};

class MUI_PUBLIC MythRender : public ReferenceCounter
{
  public:
    explicit MythRender(RenderType type);

    /// Warning: The reference count can be decremented between
    /// the call to this function and the use of it's value.
    bool IsShared(void) const;

    RenderType Type(void) const { return m_type;    }
    bool  IsErrored(void) const { return m_errored; }
    QSize GetSize(void) const   { return m_size;    }
    virtual QStringList GetDescription();
    virtual void SetViewPort(const QRect, bool = false) {}

  protected:
   ~MythRender() override = default;
    virtual void ReleaseResources(void) { }

    RenderType  m_type;
    QSize       m_size;
    bool        m_errored;

  private:
    Q_DISABLE_COPY(MythRender)
};

#endif
