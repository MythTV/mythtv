#ifndef VIDEOVISUALSPECTRUM_H
#define VIDEOVISUALSPECTRUM_H

#include <QVector>
#include "videovisual.h"

class VideoVisualSpectrum : public VideoVisual
{
  public:
    VideoVisualSpectrum(AudioPlayer *audio, MythRender *render);
    virtual ~VideoVisualSpectrum();

    virtual void Draw(const QRect &area, MythPainter *painter,
                      QPaintDevice* device);

  protected:
    virtual bool Initialise(const QRect &area);
    virtual bool InitialisePriv(void);
    virtual void DrawPriv(MythPainter *painter, QPaintDevice* device);

    int             m_numSamples;
    QVector<double> m_magnitudes;
    double          m_range;
    LogScale        m_scale;
    double          m_scaleFactor;
    double          m_falloff;

    fftw_plan lplan;
    fftw_plan rplan;
    myth_fftw_float *lin;
    myth_fftw_float *rin;
    myth_fftw_complex *lout;
    myth_fftw_complex *rout;

  private:
    QVector<QRect>  m_rects;
    int             m_barWidth;
};

#endif // VIDEOVISUALSPECTRUM_H
