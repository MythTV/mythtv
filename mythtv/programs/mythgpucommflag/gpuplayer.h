#ifndef _GPUPLAYER_H
#define _GPUPLAYER_H

#include "mythplayer.h"
#include "packetqueue.h"

class MTV_PUBLIC GPUPlayer : public MythPlayer
{
  public:
    GPUPlayer(PlayerFlags flags = kNoFlags) : MythPlayer(flags) { };
    bool ProcessFile(bool showPercentage, StatusCallback StatusCB,
                     void *cbData, PacketQueue *audioQ, PacketQueue *videoQ);
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
