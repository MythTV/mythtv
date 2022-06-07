// MythTV
#include "mythrender_base.h"

MythRender::MythRender(RenderType type)
  : ReferenceCounter(QString("MythRender:%1").arg(type)),
    m_type(type),
    m_errored(false)
{
}

bool MythRender::IsShared() const
{
    return const_cast<QAtomicInt&>(m_referenceCount).fetchAndAddOrdered(0) > 1;
}

QStringList MythRender::GetDescription()
{
    return {};
}
