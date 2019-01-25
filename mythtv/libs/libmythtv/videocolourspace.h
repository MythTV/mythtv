#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

#include <QMap>
#include <QMatrix4x4>
#include "videoouttypes.h"

typedef enum VideoCStd
{
    kCSTD_Unknown = 0,
    kCSTD_ITUR_BT_601,
    kCSTD_ITUR_BT_709,
    kCSTD_SMPTE_240M,
} VideoCStd;

class VideoColourSpace : public QMatrix4x4
{
  public:
    explicit VideoColourSpace(VideoCStd colour_std = kCSTD_ITUR_BT_601);
   ~VideoColourSpace() = default;

    PictureAttributeSupported SupportedAttributes(void) const;
    void  SetSupportedAttributes(PictureAttributeSupported supported);
    bool  HasChanged(void);
    int   GetPictureAttribute(PictureAttribute attribute);
    int   SetPictureAttribute(PictureAttribute attribute, int value);
    void  SetColourSpace(VideoCStd csp = kCSTD_Unknown);
    void  SetAlpha(int value);

  private:
    void  SetStudioLevels(bool studio);
    void  SetBrightness(int value);
    void  SetContrast(int value);
    void  SetHue(int value);
    void  SetSaturation(int value);

    void  SaveValue(PictureAttribute attribute, int value);
    void  Update(void);
    void  Debug(void);

  private:
    PictureAttributeSupported  m_supported_attributes;
    QMap<PictureAttribute,int> m_db_settings;

    bool      m_changed;
    bool      m_studioLevels;
    float     m_brightness;
    float     m_contrast;
    float     m_saturation;
    float     m_hue;
    float     m_alpha;
    VideoCStd m_colourSpace;
};

#endif
