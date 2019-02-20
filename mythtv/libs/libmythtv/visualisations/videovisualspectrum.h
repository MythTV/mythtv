#ifndef VIDEOVISUALSPECTRUM_H
#define VIDEOVISUALSPECTRUM_H

#include <QVector>
#include "videovisual.h"

class VideoVisualSpectrum : public VideoVisual
{
  public:
    VideoVisualSpectrum(AudioPlayer *audio, MythRender *render);
    virtual ~VideoVisualSpectrum();

    void Draw(const QRect &area, MythPainter *painter,
              QPaintDevice* device) override; // VideoVisual
    QString Name(void) override { return "Spectrum"; } // VideoVisual
    void prepare() override; // VideoVisual

  protected:
    virtual bool Initialise(const QRect &area);
    virtual bool InitialisePriv(void);
    virtual void DrawPriv(MythPainter *painter, QPaintDevice* device);

    int                m_numSamples  {64};
    QVector<double>    m_magnitudes;
    double             m_range       {1.0};
    LogScale           m_scale;
    double             m_scaleFactor {2.0};
    double             m_falloff     {3.0};

    fftw_plan          m_lplan;
    fftw_plan          m_rplan;
    myth_fftw_float   *m_lin         {nullptr};
    myth_fftw_float   *m_rin         {nullptr};
    myth_fftw_complex *m_lout        {nullptr};
    myth_fftw_complex *m_rout        {nullptr};

  private:
    QVector<QRect>     m_rects;
    int                m_barWidth    {1};
};

#endif // VIDEOVISUALSPECTRUM_H
