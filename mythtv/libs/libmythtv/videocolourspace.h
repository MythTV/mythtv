#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

// Qt
#include <QMap>
#include <QObject>
#include <QMatrix4x4>

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"
#include "referencecounter.h"

class VideoColourSpace : public QObject, public QMatrix4x4, public ReferenceCounter
{
    Q_OBJECT

    friend class VideoOutput;

  public:
    explicit VideoColourSpace(VideoColourSpace *Parent = nullptr);

    bool  UpdateColourSpace(const VideoFrame *Frame);
    PictureAttributeSupported SupportedAttributes(void) const;
    void  SetSupportedAttributes(PictureAttributeSupported Supported);
    int   GetPictureAttribute(PictureAttribute Attribute);
    void  SetAlpha(int Value);

  public slots:
    int   SetPictureAttribute(PictureAttribute Attribute, int Value);

  signals:
    void  Updated(void);
    void  PictureAttributeChanged(PictureAttribute Attribute, int Value);

  protected:
    ~VideoColourSpace();

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
    PictureAttributeSupported  m_supportedAttributes;
    QMap<PictureAttribute,int> m_dbSettings;

    bool      m_studioLevels;
    float     m_brightness;
    float     m_contrast;
    float     m_saturation;
    float     m_hue;
    float     m_alpha;
    int       m_colourSpace;
    int       m_colourSpaceDepth;
    int       m_range;
    bool      m_updatesDisabled;
    int       m_colorShifted;
    VideoColourSpace *m_parent;
};

#endif
