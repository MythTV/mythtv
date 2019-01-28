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
    VideoColourSpace();
   ~VideoColourSpace() = default;

    bool  UpdateColourSpace(const VideoFrame *Frame);
    PictureAttributeSupported SupportedAttributes(void) const;
    void  SetSupportedAttributes(PictureAttributeSupported Supported);
    int   GetPictureAttribute(PictureAttribute Attribute);
    int   SetPictureAttribute(PictureAttribute Attribute, int Value);
    void  SetAlpha(int Value);

  private:
    void  SetStudioLevels(bool Studio);
    void  SetBrightness(int Value);
    void  SetContrast(int Value);
    void  SetHue(int Value);
    void  SetSaturation(int Value);
    void  SaveValue(PictureAttribute Attribute, int Value);
    void  Update(void);
    void  Debug(void);

  private:
    PictureAttributeSupported  m_supported_attributes;
    QMap<PictureAttribute,int> m_db_settings;

    bool      m_studioLevels;
    float     m_brightness;
    float     m_contrast;
    float     m_saturation;
    float     m_hue;
    float     m_alpha;
    int       m_colourSpace;
    int       m_colourSpaceDepth;
    bool      m_updatesDisabled;
};

#endif
