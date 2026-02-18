#include "CommDetectorBase.h"

void CommDetectorBase::stop()
{
    m_bStop = true;
}

void CommDetectorBase::pause()
{
    m_bPaused = true;
}

void CommDetectorBase::resume()
{
    m_bPaused = false;
}

#include "moc_CommDetectorBase.cpp"
