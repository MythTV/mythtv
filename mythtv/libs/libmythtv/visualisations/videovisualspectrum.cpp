// QT
#include <QPen>

// MythTV
#include "videovisualspectrum.h"

// FFmpeg
#define FFTW_N 512
extern "C" {
#include "libavutil/mem.h"
}

// Std
#include <algorithm>

VideoVisualSpectrum::VideoVisualSpectrum(AudioPlayer* Audio, MythRender* Render)
  : VideoVisual(Audio, Render)
{
    m_lin  = static_cast<myth_fftw_float*>(av_malloc(sizeof(myth_fftw_float)*FFTW_N));
    m_rin  = static_cast<myth_fftw_float*>(av_malloc(sizeof(myth_fftw_float)*FFTW_N));
    m_lout = static_cast<myth_fftw_complex*>(av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1)));
    m_rout = static_cast<myth_fftw_complex*>(av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1)));

    m_lplan = fftw_plan_dft_r2c_1d(FFTW_N, m_lin, reinterpret_cast<myth_fftw_complex_cast*>(m_lout), FFTW_MEASURE);
    m_rplan = fftw_plan_dft_r2c_1d(FFTW_N, m_rin, reinterpret_cast<myth_fftw_complex_cast*>(m_rout), FFTW_MEASURE);
}

VideoVisualSpectrum::~VideoVisualSpectrum()
{
    av_freep(&m_lin);
    av_freep(&m_rin);
    av_freep(&m_lout);
    av_freep(&m_rout);
    fftw_destroy_plan(m_lplan);
    fftw_destroy_plan(m_rplan);
}

template<typename T> T sq(T a) { return a*a; };

void VideoVisualSpectrum::Draw(const QRect Area, MythPainter* Painter, QPaintDevice* Device)
{
    if (m_disabled)
        return;

    uint i = 0;
    {
        QMutexLocker locker(mutex());
        VisualNode* node = GetNode();
        if (Area.isEmpty() || !Painter)
            return;

        if (!Initialise(Area))
            return;

        if (node)
        {
            i = static_cast<uint>(node->m_length);
            fast_real_set_from_short(m_lin, node->m_left, node->m_length);
            if (node->m_right)
                fast_real_set_from_short(m_rin, node->m_right, node->m_length);
        }
    }

    fast_reals_set(m_lin + i, m_rin + i, 0, FFTW_N - i);
    fftw_execute(m_lplan);
    fftw_execute(m_rplan);

    double falloff = std::clamp(((static_cast<double>(SetLastUpdate().count())) / 40.0) * m_falloff, 0.0, 2048.0);
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

    DrawPriv(Painter, Device);
}

void VideoVisualSpectrum::prepare()
{
    std::fill(m_magnitudes.begin(), m_magnitudes.end(), 0.0);
    VideoVisual::prepare();
}

void VideoVisualSpectrum::DrawPriv(MythPainter* Painter, QPaintDevice* Device)
{
    static const QBrush kBrush(QColor(0, 0, 200, 180));
    static const QPen   kPen(QColor(255, 255, 255, 255));
    double range = m_area.top() + m_area.height() / 2.0;
    int count = m_scale.range();
    Painter->Begin(Device);
    for (int i = 0; i < count; i++)
    {
        m_rects[i].setTop(static_cast<int>(range - static_cast<int>(m_magnitudes[i])));
        m_rects[i].setBottom(static_cast<int>(range + static_cast<int>(m_magnitudes[i + count])));
        if (m_rects[i].height() > 4)
            Painter->DrawRect(m_rects[i], kBrush, kPen, 255);
    }
    Painter->End();
}

bool VideoVisualSpectrum::Initialise(const QRect Area)
{
    if (Area == m_area)
        return true;

    m_area = Area;
    m_barWidth = m_area.width() / m_numSamples;
    if (m_barWidth < 6)
        m_barWidth = 6;
    m_scale.setMax(192, m_area.width() / m_barWidth);

    m_magnitudes.resize(m_scale.range() * 2);
    std::fill(m_magnitudes.begin(), m_magnitudes.end(), 0.0);
    InitialisePriv();
    return true;
}

bool VideoVisualSpectrum::InitialisePriv()
{
    m_range = m_area.height() / 2.0;
    m_rects.resize(m_scale.range());
    int y = m_area.top() + static_cast<int>(m_range);
    for (int i = 0, x = m_area.left(); i < m_rects.size(); i++, x+= m_barWidth)
        m_rects[i].setRect(x, y, m_barWidth - 1, 1);

    m_scaleFactor = (static_cast<double>(m_area.height()) / 2.0) / log(static_cast<double>(FFTW_N));
    m_falloff = static_cast<double>(m_area.height()) / 150.0;

    LOG(VB_GENERAL, LOG_INFO, DESC + QString("Initialised Spectrum with %1 bars").arg(m_scale.range()));
    return true;
}

static class VideoVisualSpectrumFactory : public VideoVisualFactory
{
  public:
    const QString &name() const override;
    VideoVisual *Create(AudioPlayer* Audio, MythRender* Render) const override;
    bool SupportedRenderer(RenderType /*Type*/) override { return true; }
} VideoVisualSpectrumFactory;

const QString& VideoVisualSpectrumFactory::name() const
{
    static QString s_name(SPECTRUM_NAME);
    return s_name;
}

VideoVisual* VideoVisualSpectrumFactory::Create(AudioPlayer* Audio, MythRender* Render) const
{
    return new VideoVisualSpectrum(Audio, Render);
}
