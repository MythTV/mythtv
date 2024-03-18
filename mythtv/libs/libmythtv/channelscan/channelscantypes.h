#ifndef CHANNEL_SCAN_TYPES_H
#define CHANNEL_SCAN_TYPES_H

enum ServiceRequirements : std::uint8_t
{
    kRequireNothing = 0x0,
    kRequireVideo   = 0x1,
    kRequireAudio   = 0x2,
    kRequireAV      = 0x3,
    kRequireData    = 0x4,
};

#endif // CHANNEL_SCAN_TYPES_H
