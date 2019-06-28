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
    QMatrix4x4 GetPrimaryMatrix(void);
    float GetColourGamma(void);
    float GetDisplayGamma(void);
    PrimariesMode GetPrimariesMode(void);

  public slots:
    int   SetPictureAttribute(PictureAttribute Attribute, int Value);
    void  SetPrimariesMode(PrimariesMode Mode);

  signals:
    void  Updated(bool PrimariesChanged);
    void  PictureAttributeChanged(PictureAttribute Attribute, int Value);

  protected:
    ~VideoColourSpace();

  private:
    struct ColourPrimaries
    {
        float primaries[3][2];
        float whitepoint[2];
    };

    void  SetFullRange(bool FullRange);
    void  SetBrightness(int Value);
    void  SetContrast(int Value);
    void  SetHue(int Value);
    void  SetSaturation(int Value);
    void  SaveValue(PictureAttribute Attribute, int Value);
    void  Update(void);
    void  Debug(void);
    QMatrix4x4 GetPrimaryConversion(int Source, int Dest);
    void  GetPrimaries(int Primary, ColourPrimaries &Out, float &Gamma);
    QMatrix4x4 RGBtoXYZ(ColourPrimaries Primaries);

  private:
    static constexpr ColourPrimaries BT709     = {{{0.640f, 0.330f}, {0.300f, 0.600f}, {0.150f, 0.060f}}, {0.3127f, 0.3290f}};
    static constexpr ColourPrimaries BT610_525 = {{{0.640f, 0.340f}, {0.310f, 0.595f}, {0.155f, 0.070f}}, {0.3127f, 0.3290f}};
    static constexpr ColourPrimaries BT610_625 = {{{0.640f, 0.330f}, {0.290f, 0.600f}, {0.150f, 0.060f}}, {0.3127f, 0.3290f}};
    static constexpr ColourPrimaries BT2020    = {{{0.708f, 0.292f}, {0.170f, 0.797f}, {0.131f, 0.046f}}, {0.3127f, 0.3290f}};

    PictureAttributeSupported  m_supportedAttributes;
    QMap<PictureAttribute,int> m_dbSettings;

    bool      m_fullRange;
    float     m_brightness;
    float     m_contrast;
    float     m_saturation;
    float     m_hue;
    float     m_alpha;
    int       m_colourSpace;
    int       m_colourSpaceDepth;
    int       m_range;
    bool      m_updatesDisabled;
    int       m_colourShifted;
    PrimariesMode m_primariesMode;
    int       m_colourPrimaries;
    int       m_displayPrimaries;
    float     m_colourGamma;
    float     m_displayGamma;
    QMatrix4x4 m_primaryMatrix;
    float     m_customDisplayGamma;
    ColourPrimaries* m_customDisplayPrimaries;
    VideoColourSpace *m_parent;
};

#endif
