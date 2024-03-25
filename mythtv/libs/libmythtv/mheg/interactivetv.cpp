/**
 * This is the interface between an MHEG, or possibly, MHP
 * engine and the rest of Myth.
 */

// Qt headers
#include <QImage>
#include <QString>
#include <utility>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "interactivetv.h"
#include "mhi.h"

InteractiveTV::InteractiveTV(MythPlayerCaptionsUI *Player)
  : m_context(new MHIContext(this)),
    m_player(Player)
{
    Restart(0, 0, false);

    if (VERBOSE_LEVEL_CHECK(VB_MHEG, LOG_DEBUG))
        MHSetLogging(stdout, MHLogAll);
    else if (VERBOSE_LEVEL_CHECK(VB_MHEG, LOG_ANY))
        MHSetLogging(stdout, MHLogError | MHLogWarning | MHLogNotifications /*| MHLogLinks | MHLogActions | MHLogDetail*/);
    else
        MHSetLogging(stdout, MHLogError | MHLogWarning );
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

void InteractiveTV::Reinit(QRect videoRect, QRect dispRect, float aspect)
{
    m_context->Reinit(videoRect, dispRect, aspect);
}

bool InteractiveTV::OfferKey(const QString& key)
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
