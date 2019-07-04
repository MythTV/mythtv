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

    bool          UpdateColourSpace(const VideoFrame *Frame);
    PictureAttributeSupported SupportedAttributes(void) const;
    void          SetSupportedAttributes(PictureAttributeSupported Supported);
    int           GetPictureAttribute(PictureAttribute Attribute);
    void          SetAlpha(int Value);
    QMatrix4x4    GetPrimaryMatrix(void);
    QStringList   GetColourMappingDefines(void);
    float         GetColourGamma(void);
    float         GetDisplayGamma(void);
    PrimariesMode GetPrimariesMode(void);

    struct ColourPrimaries
    {
        float primaries[3][2];
        float whitepoint[2];
    };

    static const ColourPrimaries BT709;
    static const ColourPrimaries BT610_525;
    static const ColourPrimaries BT610_625;
    static const ColourPrimaries BT2020;

  public slots:
    int   SetPictureAttribute(PictureAttribute Attribute, int Value);
    void  SetPrimariesMode(PrimariesMode Mode);

  signals:
    void  Updated(bool PrimariesChanged);
    void  PictureAttributeChanged(PictureAttribute Attribute, int Value);

  protected:
    ~VideoColourSpace();

  private:
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
    int       m_colourTransfer;
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
