#ifndef _Driver_Option_h_
#define _Driver_Option_h_

#include <QMap>

struct DriverOption
{
    // The order of this list dictates the order the options will be shown
    enum category_t { UNKNOWN_CAT, STREAM_TYPE, VIDEO_ENCODING, VIDEO_ASPECT,
                      VIDEO_B_FRAMES, VIDEO_GOP_SIZE,
                      VIDEO_BITRATE_MODE, VIDEO_BITRATE, VIDEO_BITRATE_PEAK,
                      AUDIO_ENCODING, AUDIO_BITRATE_MODE, AUDIO_SAMPLERATE,
                      AUDIO_BITRATE, AUDIO_LANGUAGE, VOLUME,
                      BRIGHTNESS, CONTRAST, SATURATION, HUE, SHARPNESS
    };
    enum type_t { UNKNOWN_TYPE, INTEGER, BOOLEAN, STRING, MENU,
                  BUTTON, BITMASK };

    typedef QMap<int, QString> menu_t;
    typedef QMap<category_t, DriverOption> Options;

    DriverOption(void) : category(UNKNOWN_CAT), minimum(0), maximum(0),
                         default_value(0), current(0), step(0), flags(0),
                         type(UNKNOWN_TYPE) {}
    ~DriverOption(void) {}

    QString    name;
    category_t category;
    int32_t    minimum, maximum, default_value, current;
    uint32_t   step;
    uint32_t   flags;
    menu_t     menu;
    type_t     type;
};

#endif
