#ifndef SNESROMINFO_H_
#define SNESROMINFO_H_

#include <qstring.h>
#include <mythtv/mythcontext.h>
#include "rominfo.h"


class SnesGameSettings
{
  public:
    bool default_options;
    bool transparency;
    bool sixteen;
    bool hi_res;
    unsigned short interpolate;
    bool no_mode_switch;
    bool full_screen;
    bool stretch;
    bool no_sound;
    unsigned short sound_skip;
    bool stereo;
    unsigned short sound_quality;
    bool envx;
    bool thread_sound;
    bool sound_sync;
    bool interpolated_sound;
    unsigned int buffer_size;
    bool no_sample_caching;
    bool alt_sample_decode;
    bool no_echo;
    bool no_master_volume;
    bool no_joy;
    bool interleaved;
    bool alt_interleaved;
    bool hi_rom;
    bool low_rom;
    bool header;
    bool no_header;
    bool pal;
    bool ntsc;
    bool layering;
    bool no_hdma;
    bool no_speed_hacks;
    bool no_windows;
    QString extra_options;
};

class SnesRomInfo : public RomInfo
{
  public:
    SnesRomInfo(QString lromname = "",
                QString lsystem = "",
                QString lgamename ="",
                QString lgenre = "",
                int lyear = 0) :
            RomInfo(lromname, lsystem, lgamename, lgenre, lyear)
            {}
            
    SnesRomInfo(const RomInfo &lhs) :
                RomInfo(lhs) {}

    virtual ~SnesRomInfo() {}

    virtual bool FindImage(QString type,QString *result);
};

#endif
