#ifndef DRIVER_OPTION_H
#define DRIVER_OPTION_H

#include <QMap>

struct DriverOption
{
    // The order of this list dictates the order the options will be shown
    enum category_t : std::uint8_t
                    { UNKNOWN_CAT, STREAM_TYPE, VIDEO_ENCODING, VIDEO_ASPECT,
                      VIDEO_B_FRAMES, VIDEO_GOP_SIZE,
                      VIDEO_BITRATE_MODE, VIDEO_BITRATE, VIDEO_BITRATE_PEAK,
                      AUDIO_ENCODING, AUDIO_BITRATE_MODE, AUDIO_SAMPLERATE,
                      AUDIO_BITRATE, AUDIO_LANGUAGE, VOLUME,
                      BRIGHTNESS, CONTRAST, SATURATION, HUE, SHARPNESS
    };
    enum type_t : std::uint8_t
                { UNKNOWN_TYPE, INTEGER, BOOLEAN, STRING, MENU,
                  BUTTON, BITMASK };

    using menu_t = QMap<int, QString>;
    using Options = QMap<category_t, DriverOption>;

    DriverOption(void) = default;
    ~DriverOption(void) = default;

    QString    m_name;
    category_t m_category      {UNKNOWN_CAT};
    int32_t    m_minimum       {0};
    int32_t    m_maximum       {0};
    int32_t    m_defaultValue  {0};
    int32_t    m_current       {0};
    uint32_t   m_step          {0};
    uint32_t   m_flags         {0};
    menu_t     m_menu;
    type_t     m_type          {UNKNOWN_TYPE};
};

#endif // DRIVER_OPTION_H
