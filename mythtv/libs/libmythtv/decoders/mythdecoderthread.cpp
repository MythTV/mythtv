// MythTV
#include "libmythbase/mythlogging.h"
#include "mythplayer.h"
#include "mythdecoderthread.h"

#define LOC QString("DecThread: ")

MythDecoderThread::MythDecoderThread(MythPlayer *Player, bool StartPaused)
  : MThread("Decoder"),
    m_player(Player),
    m_startPaused(StartPaused)
{
}

MythDecoderThread::~MythDecoderThread()
{
    wait();
}

void MythDecoderThread::run()
{
    RunProlog();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Decoder thread starting.");
    if (m_player)
        m_player->DecoderLoop(m_startPaused);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Decoder thread exiting.");
    RunEpilog();
}
