/**
 * This is the interface between an MHEG, or possibly, MHP
 * engine and the rest of Myth.
 */

// POSIX header
#include <pthread.h>

// Qt headers
#include <qstring.h>
#include <qimage.h>

// MythTV headers
#include "interactivetv.h"
#include "mhi.h"

InteractiveTV::InteractiveTV(NuppelVideoPlayer *nvp)
    : m_context(new MHIContext(this)), m_nvp(nvp)
{
    Restart(0, 0, false);

    if (print_verbose_messages & VB_MHEG)
    {
        MHSetLogging(stdout, MHLogAll);
    }
    else
    {
        MHSetLogging(stdout, MHLogError);
    }
}

InteractiveTV::~InteractiveTV()
{
    delete m_context;
}

// Start or restart the MHEG engine.
void InteractiveTV::Restart(uint chanid, uint cardid, bool isLive)
{
    m_context->Restart(chanid, cardid, isLive);
}

// Called by the video player to see if the image needs to be updated
bool InteractiveTV::ImageHasChanged(void)
{
    return m_context->ImageUpdated();
}

// Called by the video player to redraw the image.
void InteractiveTV::UpdateOSD(OSDSet *osdSet)
{
    m_context->UpdateOSD(osdSet);
}

// Process an incoming DSMCC table.
void InteractiveTV::ProcessDSMCCSection(
    unsigned char *data, int length,
    int componentTag, unsigned carouselId, int dataBroadcastId)
{
    m_context->QueueDSMCCPacket(data, length, componentTag,
                                carouselId, dataBroadcastId);
}

void InteractiveTV::Reinit(const QRect &display)
{
    m_context->Reinit(display);
}

bool InteractiveTV::OfferKey(QString key)
{
    return m_context->OfferKey(key);
}

void InteractiveTV::GetInitialStreams(int &audioTag, int &videoTag)
{
    m_context->GetInitialStreams(audioTag, videoTag);
}
