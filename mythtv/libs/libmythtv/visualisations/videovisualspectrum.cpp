#include <QPen>

#include "videovisualspectrum.h"

#define FFTW_N 512
extern "C" {
void *av_malloc(unsigned int size);
void av_free(void *ptr);
}

VideoVisualSpectrum::VideoVisualSpectrum(AudioPlayer *audio, MythRender *render)
  : VideoVisual(audio, render)
{
    m_lin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    m_rin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    m_lout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    m_rout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    m_lplan = fftw_plan_dft_r2c_1d(FFTW_N, m_lin, (myth_fftw_complex_cast*)m_lout, FFTW_MEASURE);
    m_rplan = fftw_plan_dft_r2c_1d(FFTW_N, m_rin, (myth_fftw_complex_cast*)m_rout, FFTW_MEASURE);
}

VideoVisualSpectrum::~VideoVisualSpectrum()
{
    if (m_lin)
        av_free(m_lin);
    if (m_rin)
        av_free(m_rin);
    if (m_lout)
        av_free(m_lout);
    if (m_rout)
        av_free(m_rout);
    fftw_destroy_plan(m_lplan);
    fftw_destroy_plan(m_rplan);
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
        i = node->m_length;
        fast_real_set_from_short(m_lin, node->m_left, node->m_length);
        if (node->m_right)
            fast_real_set_from_short(m_rin, node->m_right, node->m_length);
    }
    mutex()->unlock();

    fast_reals_set(m_lin + i, m_rin + i, 0, FFTW_N - i);
    fftw_execute(m_lplan);
    fftw_execute(m_rplan);

    double falloff = (((double)SetLastUpdate()) / 40.0) * m_falloff;
    if (falloff < 0.0)
        falloff = 0.0;
    if (falloff > 2048.0)
        falloff = 2048.0;
    for (int l = 0, r = m_scale.range(); l < m_scale.range(); l++, r++)
    {
        int index = m_scale[l];

        // The 1D output is Hermitian symmetric (Yk = Yn-k) so Yn = Y0 etc.
        // The dft_r2c_1d plan doesn't output these redundant values
        // and furthermore they're not allocated in the ctor
        double tmp = 2 * sq(real(m_lout[index]));
        double magL = (tmp > 1.) ? (log(tmp) - 22.0) * m_scaleFactor : 0.;

        tmp = 2 * sq(real(m_rout[index]));
        double magR = (tmp > 1.) ? (log(tmp) - 22.0) * m_scaleFactor : 0.;

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

void VideoVisualSpectrum::prepare(void)
{
    for (int i = 0; i < m_magnitudes.size(); i++)
        m_magnitudes[i] = 0.0;
    VideoVisual::prepare();
}

void VideoVisualSpectrum::DrawPriv(MythPainter *painter, QPaintDevice* device)
{
    static const QBrush kBrush(QColor(0, 0, 200, 180));
    static const QPen   kPen(QColor(255, 255, 255, 255));
    double range = m_area.height() / 2.0;
    int count = m_scale.range();
    painter->Begin(device);
    for (int i = 0; i < count; i++)
    {
        m_rects[i].setTop(range - int(m_magnitudes[i]));
        m_rects[i].setBottom(range + int(m_magnitudes[i + count]));
        if (m_rects[i].height() > 4)
            painter->DrawRect(m_rects[i], kBrush, kPen, 255);
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

    m_scaleFactor = (static_cast<double>(m_area.height()) / 2.0)
        / log((double)(FFTW_N));
    m_falloff = (double)m_area.height() / 150.0;

    LOG(VB_GENERAL, LOG_INFO, DESC +
        QString("Initialised Spectrum with %1 bars") .arg(m_scale.range()));
    return true;
}

static class VideoVisualSpectrumFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const override // VideoVisualFactory
    {
        static QString s_name("Spectrum");
        return s_name;
    }

    VideoVisual *Create(AudioPlayer *audio,
                        MythRender  *render) const override // VideoVisualFactory
    {
        return new VideoVisualSpectrum(audio, render);
    }

    bool SupportedRenderer(RenderType /*type*/) override { return true; } // VideoVisualFactory
} VideoVisualSpectrumFactory;
