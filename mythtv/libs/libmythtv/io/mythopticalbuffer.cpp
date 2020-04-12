// MythTV
#include "mythopticalbuffer.h"

MythOpticalBuffer::MythOpticalBuffer(MythBufferType Type)
  : MythMediaBuffer(Type)
{
}

bool MythOpticalBuffer::IsInMenu(void) const
{
    return m_inMenu;
}
