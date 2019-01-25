#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

// Qt
#include <QMap>
#include <QMatrix4x4>

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"

class VideoColourSpace : public QMatrix4x4
{
  public:
    explicit VideoColourSpace(MythColorSpace colour_std = ColorSpaceUnknown);
   ~VideoColourSpace() = default;

    PictureAttributeSupported SupportedAttributes(void) const;
    void  SetSupportedAttributes(PictureAttributeSupported supported);
    bool  HasChanged(void);
    int   GetPictureAttribute(PictureAttribute attribute);
    int   SetPictureAttribute(PictureAttribute attribute, int value);
    bool  SetColourSpace(MythColorSpace csp);
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
    MythColorSpace m_colourSpace;
};

#endif
