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

// FFmpeg
#include "libavutil/pixfmt.h" // For AVCOL_xxx defines

class VideoColourSpace : public QObject, public QMatrix4x4, public ReferenceCounter
{
    Q_OBJECT

    friend class MythVideoOutput;

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

    static const ColourPrimaries kBT709;
    static const ColourPrimaries kBT610_525;
    static const ColourPrimaries kBT610_625;
    static const ColourPrimaries kBT2020;

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
    static void GetPrimaries(int Primary, ColourPrimaries &Out, float &Gamma);
    static QMatrix4x4 RGBtoXYZ(ColourPrimaries Primaries);

  private:
    PictureAttributeSupported  m_supportedAttributes {kPictureAttributeSupported_None};
    QMap<PictureAttribute,int> m_dbSettings;

    bool              m_fullRange              {true};
    float             m_brightness             {0.0F};
    float             m_contrast               {1.0F};
    float             m_saturation             {1.0F};
    float             m_hue                    {0.0F};
    float             m_alpha                  {1.0F};
    int               m_colourSpace            {AVCOL_SPC_UNSPECIFIED};
    int               m_colourSpaceDepth       {8};
    int               m_range                  {AVCOL_RANGE_MPEG};
    bool              m_updatesDisabled        {true};
    int               m_colourShifted          {0};
    int               m_colourTransfer         {AVCOL_TRC_BT709};
    PrimariesMode     m_primariesMode          {PrimariesAuto};
    int               m_colourPrimaries        {AVCOL_PRI_BT709};
    int               m_displayPrimaries       {AVCOL_PRI_BT709};
    float             m_colourGamma            {2.2F};
    float             m_displayGamma           {2.2F};
    QMatrix4x4        m_primaryMatrix;
    float             m_customDisplayGamma     {2.2F};
    ColourPrimaries  *m_customDisplayPrimaries {nullptr};
    VideoColourSpace *m_parent                 {nullptr};
};

#endif
