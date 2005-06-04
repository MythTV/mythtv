#include "CommDetectorBase.h"

CommDetectorBase::CommDetectorBase() : m_bPaused(false), m_bStop(false)
{
}

void CommDetectorBase::stop()
{
    m_bStop = true;
}

void CommDetectorBase::pause()
{
    m_bPaused = true;
    emit(statusUpdate("Paused"));
}

void CommDetectorBase::resume()
{
    m_bPaused = false;
    emit(statusUpdate("Resume"));
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
