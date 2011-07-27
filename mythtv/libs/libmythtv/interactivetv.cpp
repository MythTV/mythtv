/**
 * This is the interface between an MHEG, or possibly, MHP
 * engine and the rest of Myth.
 */

// Qt headers
#include <QString>
#include <QImage>

// MythTV headers
#include "interactivetv.h"
#include "mhi.h"
#include "mythlogging.h"

InteractiveTV::InteractiveTV(MythPlayer *nvp)
    : m_context(new MHIContext(this)), m_nvp(nvp)
{
    Restart(0, 0, false);

    MHSetLogging(stdout,
        VERBOSE_LEVEL_CHECK(VB_MHEG, LOG_DEBUG) ? MHLogAll :
        VERBOSE_LEVEL_CHECK(VB_MHEG, LOG_ANY) ?
            MHLogError | MHLogWarning | MHLogNotifications /*| MHLogLinks | MHLogActions | MHLogDetail*/ :
        MHLogError | MHLogWarning );
}

InteractiveTV::~InteractiveTV()
{
    delete m_context;
}

// Start or restart the MHEG engine.
void InteractiveTV::Restart(int chanid, int sourceid, bool isLive)
{
    m_context->Restart(chanid, sourceid, isLive);
}

// Called by the video player to see if the image needs to be updated
bool InteractiveTV::ImageHasChanged(void)
{
    return m_context->ImageUpdated();
}

// Called by the video player to redraw the image.
void InteractiveTV::UpdateOSD(InteractiveScreen *osdWindow,
                              MythPainter *osdPainter)
{
    m_context->UpdateOSD(osdWindow, osdPainter);
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

void InteractiveTV::SetNetBootInfo(const unsigned char *data, uint length)
{
    m_context->SetNetBootInfo(data, length);
}

bool InteractiveTV::StreamStarted(bool bStarted)
{
    return m_context->StreamStarted(bStarted);
}
