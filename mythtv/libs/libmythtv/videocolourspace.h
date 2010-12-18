#ifndef VIDEOCOLOURSPACE_H
#define VIDEOCOLOURSPACE_H

#include <QMap>
#include "videoouttypes.h"

class Matrix
{
  public:
    Matrix(float m11, float m12, float m13, float m14,
           float m21, float m22, float m23, float m24,
           float m31, float m32, float m33, float m34);
    Matrix();

    void setToIdentity(void);
    void scale(float val1, float val2, float val3);
    void translate(float val1, float val2, float val3);
    Matrix & operator*=(const Matrix &r);
    void product(int row, const Matrix &r);
    void debug(void);
    float m[4][4];
};

typedef enum VideoCStd
{
    kCSTD_Unknown = 0,
    kCSTD_ITUR_BT_601,
    kCSTD_ITUR_BT_709,
    kCSTD_SMPTE_240M,
} VideoCStd;

class VideoColourSpace
{
  public:
    VideoColourSpace(VideoCStd colour_std = kCSTD_ITUR_BT_601);
   ~VideoColourSpace() { }

    PictureAttributeSupported SupportedAttributes(void)
        { return m_supported_attributes; }
    void  SetSupportedAttributes(PictureAttributeSupported supported);

    void* GetMatrix(void)  { return &m_matrix.m; }
    bool  HasChanged(void) { return m_changed;   }

    int   GetPictureAttribute(PictureAttribute attribute);
    int   SetPictureAttribute(PictureAttribute attribute, int value);
    void  SetColourSpace(VideoCStd csp = kCSTD_Unknown);

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
    VideoCStd m_colourSpace;
    Matrix    m_matrix;
};

#endif
