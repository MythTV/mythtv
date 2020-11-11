#ifndef VIDEOVISUALSPECTRUM_H
#define VIDEOVISUALSPECTRUM_H

#include <QVector>
#include "videovisual.h"

#define SPECTRUM_NAME QString("Spectrum")

class VideoVisualSpectrum : public VideoVisual
{
  public:
    VideoVisualSpectrum(AudioPlayer* Audio, MythRender* Render);
    ~VideoVisualSpectrum() override;

    void    Draw    (QRect Area, MythPainter* Painter, QPaintDevice* Device) override;
    QString Name    () override { return SPECTRUM_NAME; }
    void    prepare () override;

  protected:
    virtual bool Initialise     (QRect Area);
    virtual bool InitialisePriv ();
    virtual void DrawPriv       (MythPainter* Painter, QPaintDevice* Device);

    int                m_numSamples  { 64 };
    QVector<double>    m_magnitudes;
    double             m_range       { 1.0 };
    LogScale           m_scale;
    double             m_scaleFactor { 2.0 };
    double             m_falloff     { 3.0 };

    fftw_plan          m_lplan;
    fftw_plan          m_rplan;
    myth_fftw_float*   m_lin         { nullptr };
    myth_fftw_float*   m_rin         { nullptr };
    myth_fftw_complex* m_lout        { nullptr };
    myth_fftw_complex* m_rout        { nullptr };

  private:
    QVector<QRect>     m_rects;
    int                m_barWidth    { 1 };
};

#endif
