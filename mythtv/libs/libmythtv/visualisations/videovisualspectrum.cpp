#include <QPen>

#include "videovisualspectrum.h"

#define FFTW_N 512
extern "C" {
void *av_malloc(unsigned int size);
void av_free(void *ptr);
}

VideoVisualSpectrum::VideoVisualSpectrum(AudioPlayer *audio, MythRender *render)
  : VideoVisual(audio, render), m_range(1.0), m_scaleFactor(2.0),
    m_falloff(3.0), m_barWidth(1)
{
    m_numSamples = 64;
    lin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    rin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    lout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    rout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    lplan = fftw_plan_dft_r2c_1d(FFTW_N, lin, (myth_fftw_complex_cast*)lout, FFTW_MEASURE);
    rplan = fftw_plan_dft_r2c_1d(FFTW_N, rin, (myth_fftw_complex_cast*)rout, FFTW_MEASURE);
}

VideoVisualSpectrum::~VideoVisualSpectrum()
{
    if (lin)
        av_free(lin);
    if (rin)
        av_free(rin);
    if (lout)
        av_free(lout);
    if (rout)
        av_free(rout);
    fftw_destroy_plan(lplan);
    fftw_destroy_plan(rplan);
}

template<typename T> T sq(T a) { return a*a; };

void VideoVisualSpectrum::Draw(const QRect &area, MythPainter *painter,
                               QPaintDevice* device)
{
    if (m_disabled)
        return;

    mutex()->lock();
    VisualNode *node = GetNode();

    if (area.isEmpty() || !painter)
    {
        mutex()->unlock();
        return;
    }

    if (!Initialise(area))
    {
        mutex()->unlock();
        return;
    }

    uint i = 0;
    if (node)
    {
        i = node->length;
        fast_real_set_from_short(lin, node->left, node->length);
        if (node->right)
            fast_real_set_from_short(rin, node->right, node->length);
    }
    mutex()->unlock();

    fast_reals_set(lin + i, rin + i, 0, FFTW_N - i);
    fftw_execute(lplan);
    fftw_execute(rplan);

    double magL, magR, tmp;
    double falloff = (((double)SetLastUpdate()) / 40.0) * m_falloff;
    for (int l = 0, r = m_scale.range(); l < m_scale.range(); l++, r++)
    {
        int index = m_scale[l];
        magL = (log(sq(real(lout[index])) + sq(real(lout[FFTW_N - index]))) - 22.0) *
               m_scaleFactor;
        magR = (log(sq(real(rout[index])) + sq(real(rout[FFTW_N - index]))) - 22.0) *
               m_scaleFactor;

        if (magL > m_range)
            magL = 1.0;

        if (magL < m_magnitudes[l])
        {
            tmp = m_magnitudes[l] - falloff;
            if (tmp < magL)
                tmp = magL;
            magL = tmp;
        }

        if (magL < 1.0)
            magL = 1.0;

        if (magR > m_range)
            magR = 1.0;

        if (magR < m_magnitudes[r])
        {
            tmp = m_magnitudes[r] - falloff;
            if (tmp < magR)
                tmp = magR;
            magR = tmp;
        }

        if (magR < 1.0)
            magR = 1.0;

        m_magnitudes[l] = magL;
        m_magnitudes[r] = magR;
    }

    DrawPriv(painter, device);
}

void VideoVisualSpectrum::DrawPriv(MythPainter *painter, QPaintDevice* device)
{
    static const QBrush brush(QColor(0, 0, 200, 180));
    static const QPen   pen(QColor(255, 255, 255, 255));
    double range = m_area.height() / 2.0;
    int count = m_scale.range();
    painter->Begin(device);
    for (int i = 0; i < count; i++)
    {
        m_rects[i].setTop(range - int(m_magnitudes[i]));
        m_rects[i].setBottom(range + int(m_magnitudes[i + count]));
        if (m_rects[i].height() > 4)
            painter->DrawRect(m_rects[i], brush, pen, 255);
    }
    painter->End();
}

bool VideoVisualSpectrum::Initialise(const QRect &area)
{
    if (area == m_area)
        return true;

    m_area = area;
    m_barWidth = m_area.width() / m_numSamples;
    if (m_barWidth < 6)
        m_barWidth = 6;
    m_scale.setMax(192, m_area.width() / m_barWidth);

    m_magnitudes.resize(m_scale.range() * 2);
    for (int i = 0; i < m_magnitudes.size(); i++)
        m_magnitudes[i] = 0.0;

    InitialisePriv();
    return true;
}

bool VideoVisualSpectrum::InitialisePriv(void)
{
    m_range = m_area.height() / 2.0;
    m_rects.resize(m_scale.range());
    for (int i = 0, x = 0; i < m_rects.size(); i++, x+= m_barWidth)
        m_rects[i].setRect(x, m_area.height() / 2, m_barWidth - 1, 1);

    m_scaleFactor = double(m_area.height() / 2) / log((double)(FFTW_N));
    m_falloff = (double)m_area.height() / 150.0;

    VERBOSE(VB_GENERAL, DESC + QString("Initialised Spectrum with %1 bars")
        .arg(m_scale.range()));
    return true;
}
