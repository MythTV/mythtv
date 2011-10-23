#include "mythlogging.h"
#include "packetqueue.h"
#include "resultslist.h"
#include "videoconsumer.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

void VideoConsumer::ProcessPacket(Packet *packet)
{
    LOG(VB_GENERAL, LOG_INFO, "Video Frame");
    // Decode the packet to YUV (using HW if possible)

    // Push YUV frame to GPU/CPU Processing memory

    // Loop through the list of detection routines
        // Run the routine in GPU/CPU
        // Pull the results
        // Toss the results onto the results list

    // Free the frame in GPU/CPU memory if not needed
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
