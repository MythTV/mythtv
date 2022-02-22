/*
        visualize.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
        VERY closely based on code from mq3 by Brad Hughes

        Part of the mythTV project

        music visualizers
*/

// C
#include <cmath>

// C++
#include <iostream>

// Qt
#include <QCoreApplication>
#include <QApplication>
#include <QPainter>
#include <QImage>

// MythTV
#include <mythdbcon.h>
#include <mythcontext.h>
#include <mythmainwindow.h>
#include <mythuihelper.h>
#include <remotefile.h>
#include <musicmetadata.h>

// mythmusic
#include "mainvisual.h"
#include "visualize.h"
#include "inlines.h"
#include "decoder.h"
#include "musicplayer.h"

static constexpr int FFTW_N { 512 };
// static_assert(FFTW_N==SAMPLES_DEFAULT_SIZE)


VisFactory* VisFactory::g_pVisFactories = nullptr;

VisualBase::VisualBase(bool screensaverenable)
    : m_xscreensaverenable(screensaverenable)
{
    if (!m_xscreensaverenable)
        MythMainWindow::DisableScreensaver();
}

VisualBase::~VisualBase()
{
    //
    //    This is only here so
    //    that derived classes
    //    can destruct properly
    //
    if (!m_xscreensaverenable)
        MythMainWindow::RestoreScreensaver();
}


void VisualBase::drawWarning(QPainter *p, const QColor &back, const QSize size, const QString& warning, int fontSize)
{
    p->fillRect(0, 0, size.width(), size.height(), back);
    p->setPen(Qt::white);

    // Taken from removed MythUIHelper::GetMediumFont
    QFont font = QApplication::font();

#ifdef _WIN32
    // logicalDpiY not supported in Windows.
    int logicalDpiY = 100;
    HDC hdc = GetDC(nullptr);
    if (hdc)
    {
        logicalDpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
    }
#else
    int logicalDpiY = GetMythMainWindow()->logicalDpiY();
#endif

    // adjust for screen resolution relative to 100 dpi
    float floatSize = (16 * 100.0F) / logicalDpiY;
    // adjust for myth GUI size relative to 800x600
    float dummy = 0.0;
    float hmult = 0.0;
    GetMythMainWindow()->GetScalingFactors(hmult, dummy);
    floatSize = floatSize * hmult;
    // round and set
    font.setPointSize(lroundf(floatSize));
    font.setWeight(QFont::Bold);
    font.setPointSizeF(fontSize * (size.width() / 800.0));
    p->setFont(font);

    p->drawText(0, 0, size.width(), size.height(), Qt::AlignVCenter | Qt::AlignHCenter | Qt::TextWordWrap, warning);
}

///////////////////////////////////////////////////////////////////////////////
// LogScale

LogScale::LogScale(int maxscale, int maxrange)
{
    setMax(maxscale, maxrange);
}

LogScale::~LogScale()
{
    delete [] m_indices;
}

void LogScale::setMax(int maxscale, int maxrange)
{
    if (maxscale == 0 || maxrange == 0)
        return;

    m_s = maxscale;
    m_r = maxrange;

    delete [] m_indices;

    auto domain = (long double) maxscale;
    auto range  = (long double) maxrange;
    long double x  = 1.0;
    long double dx = 1.0;
    long double e4 = 1.0E-8;

    m_indices = new int[maxrange];
    for (int i = 0; i < maxrange; i++)
        m_indices[i] = 0;

    // initialize log scale
    for (uint i=0; i<10000 && (std::abs(dx) > e4); i++)
    {
        double t = std::log((domain + x) / x);
        double y = (x * t) - range;
        double yy = t - (domain / (x + domain));
        dx = y / yy;
        x -= dx;
    }

    double alpha = x;
    for (int i = 1; i < (int) domain; i++)
    {
        int scaled = (int) floor(0.5 + (alpha * log((double(i) + alpha) / alpha)));
        if (scaled < 1)
            scaled = 1;
        if (m_indices[scaled - 1] < i)
            m_indices[scaled - 1] = i;
    }
}

int LogScale::operator[](int index)
{
    return m_indices[index];
}

///////////////////////////////////////////////////////////////////////////////
// StereoScope

StereoScope::StereoScope()
{
    m_fps = 45;
}

void StereoScope::resize( const QSize &newsize )
{
    m_size = newsize;

    auto os = m_magnitudes.size();
    m_magnitudes.resize( m_size.width() * 2 );
    for ( ; os < m_magnitudes.size(); os++ )
        m_magnitudes[os] = 0.0;
}

bool StereoScope::process( VisualNode *node )
{
    bool allZero = true;


    if (node)
    {
        double index = 0;
        double const step = (double)SAMPLES_DEFAULT_SIZE / m_size.width();
        for ( int i = 0; i < m_size.width(); i++)
        {
            auto indexTo = (unsigned long)(index + step);
            if (indexTo == (unsigned long)(index))
                indexTo = (unsigned long)(index + 1);

            double valL = 0;
            double valR = 0;
#if RUBBERBAND
            if ( m_rubberband ) {
                valL = m_magnitudes[ i ];
                valR = m_magnitudes[ i + m_size.width() ];
                if (valL < 0.) {
                    valL += m_falloff;
                    if ( valL > 0. )
                        valL = 0.;
                }
                else
                {
                    valL -= m_falloff;
                    if ( valL < 0. )
                        valL = 0.;
                }
                if (valR < 0.)
                {
                    valR += m_falloff;
                    if ( valR > 0. )
                        valR = 0.;
                }
                else
                {
                    valR -= m_falloff;
                    if ( valR < 0. )
                        valR = 0.;
                }
            }
#endif
            for (auto s = (unsigned long)index; s < indexTo && s < node->m_length; s++)
            {
                double adjHeight = static_cast<double>(m_size.height()) / 4.0;
                double tmpL = ( ( node->m_left ? static_cast<double>(node->m_left[s]) : 0.) *
                                adjHeight ) / 32768.0;
                double tmpR = ( ( node->m_right ? static_cast<double>(node->m_right[s]) : 0.) *
                                adjHeight ) / 32768.0;
                if (tmpL > 0)
                    valL = (tmpL > valL) ? tmpL : valL;
                else
                    valL = (tmpL < valL) ? tmpL : valL;
                if (tmpR > 0)
                    valR = (tmpR > valR) ? tmpR : valR;
                else
                    valR = (tmpR < valR) ? tmpR : valR;
            }

            if (valL != 0. || valR != 0.)
                allZero = false;

            m_magnitudes[ i ] = valL;
            m_magnitudes[ i + m_size.width() ] = valR;

            index = index + step;
        }
#if RUBBERBAND
    }
    else if (m_rubberband)
    {
        for ( int i = 0; i < m_size.width(); i++)
        {
            double valL = m_magnitudes[ i ];
            if (valL < 0) {
                valL += 2;
                if (valL > 0.)
                    valL = 0.;
            } else {
                valL -= 2;
                if (valL < 0.)
                    valL = 0.;
            }

            double valR = m_magnitudes[ i + m_size.width() ];
            if (valR < 0.) {
                valR += m_falloff;
                if (valR > 0.)
                    valR = 0.;
            }
            else
            {
                valR -= m_falloff;
                if (valR < 0.)
                    valR = 0.;
            }

            if (valL != 0. || valR != 0.)
                allZero = false;

            m_magnitudes[ i ] = valL;
            m_magnitudes[ i + m_size.width() ] = valR;
        }
#endif
    }
    else
    {
        for ( int i = 0; (unsigned) i < m_magnitudes.size(); i++ )
            m_magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool StereoScope::draw( QPainter *p, const QColor &back )
{
    p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    for ( int i = 1; i < m_size.width(); i++ )
    {
#if TWOCOLOUR
    double r, g, b, per;

    // left
    per = ( static_cast<double>(m_magnitudes[i]) * 2.0 ) /
          ( static_cast<double>(m_size.height()) / 4.0 );
    if (per < 0.0)
        per = -per;
    if (per > 1.0)
        per = 1.0;
    else if (per < 0.0)
        per = 0.0;

    r = m_startColor.red() + (m_targetColor.red() -
                m_startColor.red()) * (per * per);
    g = m_startColor.green() + (m_targetColor.green() -
                  m_startColor.green()) * (per * per);
    b = m_startColor.blue() + (m_targetColor.blue() -
                 m_startColor.blue()) * (per * per);

    if (r > 255.0)
        r = 255.0;
    else if (r < 0.0)
        r = 0;

    if (g > 255.0)
        g = 255.0;
    else if (g < 0.0)
        g = 0;

    if (b > 255.0)
        b = 255.0;
    else if (b < 0.0)
        b = 0;

    p->setPen( QColor( int(r), int(g), int(b) ) );
#else
    p->setPen(Qt::red);
#endif
    double adjHeight = static_cast<double>(m_size.height()) / 4.0;
    p->drawLine( i - 1,
                 (int)(adjHeight + m_magnitudes[i - 1]),
                 i,
                 (int)(adjHeight + m_magnitudes[i]));

#if TWOCOLOUR
    // right
    per = ( static_cast<double>(m_magnitudes[ i + m_size.width() ]) * 2 ) /
          adjHeight;
    if (per < 0.0)
        per = -per;
    if (per > 1.0)
        per = 1.0;
    else if (per < 0.0)
        per = 0.0;

    r = m_startColor.red() + (m_targetColor.red() -
                m_startColor.red()) * (per * per);
    g = m_startColor.green() + (m_targetColor.green() -
                  m_startColor.green()) * (per * per);
    b = m_startColor.blue() + (m_targetColor.blue() -
                 m_startColor.blue()) * (per * per);

    if (r > 255.0)
        r = 255.0;
    else if (r < 0.0)
        r = 0;

    if (g > 255.0)
        g = 255.0;
    else if (g < 0.0)
        g = 0;

    if (b > 255.0)
        b = 255.0;
    else if (b < 0.0)
        b = 0;

    p->setPen( QColor( int(r), int(g), int(b) ) );
#else
    p->setPen(Qt::red);
#endif
    adjHeight = static_cast<double>(m_size.height()) * 3.0 / 4.0;
    p->drawLine( i - 1,
                 (int)(adjHeight + m_magnitudes[i + m_size.width() - 1]),
                 i,
                 (int)(adjHeight + m_magnitudes[i + m_size.width()]));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// MonoScope

bool MonoScope::process( VisualNode *node )
{
    bool allZero = true;

    if (node)
    {
        double index = 0;
        double const step = (double)SAMPLES_DEFAULT_SIZE / m_size.width();
        for (int i = 0; i < m_size.width(); i++)
        {
            auto indexTo = (unsigned long)(index + step);
            if (indexTo == (unsigned long)index)
                indexTo = (unsigned long)(index + 1);

            double val = 0;
#if RUBBERBAND
            if ( m_rubberband )
            {
                val = m_magnitudes[ i ];
                if (val < 0.)
                {
                    val += m_falloff;
                    if ( val > 0. )
                    {
                        val = 0.;
                    }
                }
                else
                {
                    val -= m_falloff;
                    if ( val < 0. )
                    {
                        val = 0.;
                    }
                }
            }
#endif
            for (auto s = (unsigned long)index; s < indexTo && s < node->m_length; s++)
            {
                double tmp = ( static_cast<double>(node->m_left[s]) +
                               (node->m_right ? static_cast<double>(node->m_right[s]) : 0.0) *
                               ( static_cast<double>(m_size.height()) / 2.0 ) ) / 65536.0;
                if (tmp > 0)
                {
                    val = (tmp > val) ? tmp : val;
                }
                else
                {
                    val = (tmp < val) ? tmp : val;
                }
            }

            if ( val != 0. )
            {
                allZero = false;
            }
            m_magnitudes[ i ] = val;
            index = index + step;
        }
    }
#if RUBBERBAND
    else if (m_rubberband)
    {
        for (int i = 0; i < m_size.width(); i++) {
            double val = m_magnitudes[ i ];
            if (val < 0) {
                val += 2;
                if (val > 0.)
                    val = 0.;
            } else {
                val -= 2;
                if (val < 0.)
                    val = 0.;
            }

            if ( val != 0. )
                allZero = false;
            m_magnitudes[ i ] = val;
        }
    }
#endif
    else
    {
        for (int i = 0; i < m_size.width(); i++ )
            m_magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool MonoScope::draw( QPainter *p, const QColor &back )
{
    p->fillRect( 0, 0, m_size.width(), m_size.height(), back );
    for ( int i = 1; i < m_size.width(); i++ ) {
#if TWOCOLOUR
        double r, g, b, per;

        per = double( m_magnitudes[ i ] ) /
              double( m_size.height() / 4 );
        if (per < 0.0)
            per = -per;
        if (per > 1.0)
            per = 1.0;
        else if (per < 0.0)
            per = 0.0;

        r = m_startColor.red() + (m_targetColor.red() -
                                m_startColor.red()) * (per * per);
        g = m_startColor.green() + (m_targetColor.green() -
                                  m_startColor.green()) * (per * per);
        b = m_startColor.blue() + (m_targetColor.blue() -
                                 m_startColor.blue()) * (per * per);

        if (r > 255.0)
            r = 255.0;
        else if (r < 0.0)
            r = 0;

        if (g > 255.0)
            g = 255.0;
        else if (g < 0.0)
            g = 0;

        if (b > 255.0)
            b = 255.0;
        else if (b < 0.0)
            b = 0;

        p->setPen(QColor(int(r), int(g), int(b)));
#else
        p->setPen(Qt::red);
#endif
        double adjHeight = static_cast<double>(m_size.height()) / 2.0;
        p->drawLine( i - 1,
                     (int)(adjHeight + m_magnitudes[ i - 1 ]),
                     i,
                     (int)(adjHeight + m_magnitudes[ i ] ));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// StereoScopeFactory

static class StereoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "StereoScope");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new StereoScope();
    }
}StereoScopeFactory;


///////////////////////////////////////////////////////////////////////////////
// MonoScopeFactory

static class MonoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "MonoScope");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new MonoScope();
    }
}MonoScopeFactory;

///////////////////////////////////////////////////////////////////////////////
// Spectrum
//

Spectrum::Spectrum()
{
    LOG(VB_GENERAL, LOG_INFO, QString("Spectrum : Being Initialised"));

    m_fps = 15;

    m_dftL = static_cast<FFTComplex*>(av_malloc(sizeof(FFTComplex) * FFTW_N));
    m_dftR = static_cast<FFTComplex*>(av_malloc(sizeof(FFTComplex) * FFTW_N));

    m_fftContextForward = av_fft_init(std::log2(FFTW_N), 0);
}

Spectrum::~Spectrum()
{
    av_freep(&m_dftL);
    av_freep(&m_dftR);
    av_fft_end(m_fftContextForward);
}

void Spectrum::resize(const QSize &newsize)
{
    // Just change internal data about the
    // size of the pixmap to be drawn (ie. the
    // size of the screen) and the logically
    // ensuing number of up/down bars to hold
    // the audio magnitudes

    m_size = newsize;

    m_analyzerBarWidth = m_size.width() / 64;

    if (m_analyzerBarWidth < 6)
        m_analyzerBarWidth = 6;

    m_scale.setMax(192, m_size.width() / m_analyzerBarWidth);

    m_rects.resize( m_scale.range() );
    int w = 0;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (uint i = 0; i < (uint)m_rects.size(); i++, w += m_analyzerBarWidth)
    {
        m_rects[i].setRect(w, m_size.height() / 2, m_analyzerBarWidth - 1, 1);
    }

    m_magnitudes.resize( m_scale.range() * 2 );
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (uint os = m_magnitudes.size(); os < (uint)m_magnitudes.size(); os++)
    {
        m_magnitudes[os] = 0.0;
    }

    m_scaleFactor = ( static_cast<double>(m_size.height()) / 2.0 ) /
                    log( static_cast<double>(FFTW_N) );
}

template<typename T> T sq(T a) { return a*a; };

bool Spectrum::process(VisualNode *node)
{
    // Take a bunch of data in *node
    // and break it down into spectrum
    // values
    bool allZero = true;

    uint i = 0;
    long w = 0;
    QRect *rectsp = m_rects.data();
    double *magnitudesp = m_magnitudes.data();

    if (node)
    {
        i = node->m_length;
        if (i > FFTW_N)
            i = FFTW_N;
        for (unsigned long k = 0; k < node->m_length; k++)
        {
            m_dftL[k] = (FFTComplex){ .re = (FFTSample)node->m_left[k], .im = 0 };
            if (node->m_right)
                m_dftR[k] = (FFTComplex){ .re = (FFTSample)node->m_right[k], .im = 0 };
        }
    }

    for (auto k = i; k < FFTW_N; k++)
    {
        m_dftL[k] = (FFTComplex){ .re = 0, .im = 0 };
        m_dftL[k] = (FFTComplex){ .re = 0, .im = 0 };
    }
    av_fft_permute(m_fftContextForward, m_dftL);
    av_fft_calc(m_fftContextForward, m_dftL);

    av_fft_permute(m_fftContextForward, m_dftR);
    av_fft_calc(m_fftContextForward, m_dftR);

    long index = 1;

    for (i = 0; (int)i < m_rects.size(); i++, w += m_analyzerBarWidth)
    {
        // The 1D output is Hermitian symmetric (Yk = Yn-k) so Yn = Y0 etc.
        // The dft_r2c_1d plan doesn't output these redundant values
        // and furthermore they're not allocated in the ctor
        double tmp = 2 * sq(m_dftL[index].re);
        double magL = (tmp > 1.) ? (log(tmp) - 22.0) * m_scaleFactor : 0.;

        tmp = 2 * sq(m_dftR[index].re);
        double magR = (tmp > 1.) ? (log(tmp) - 22.0) * m_scaleFactor : 0.;

        double adjHeight = static_cast<double>(m_size.height()) / 2.0;
        if (magL > adjHeight)
        {
            magL = adjHeight;
        }
        if (magL < magnitudesp[i])
        {
            tmp = magnitudesp[i] - m_falloff;
            if ( tmp < magL )
            {
                tmp = magL;
            }
            magL = tmp;
        }
        if (magL < 1.)
        {
            magL = 1.;
        }

        if (magR > adjHeight)
        {
            magR = adjHeight;
        }
        if (magR < magnitudesp[i + m_scale.range()])
        {
            tmp = magnitudesp[i + m_scale.range()] - m_falloff;
            if ( tmp < magR )
            {
                tmp = magR;
            }
            magR = tmp;
        }
        if (magR < 1.)
        {
            magR = 1.;
        }

        if (magR != 1 || magL != 1)
        {
            allZero = false;
        }

        magnitudesp[i] = magL;
        magnitudesp[i + m_scale.range()] = magR;
        rectsp[i].setTop( m_size.height() / 2 - int( magL ) );
        rectsp[i].setBottom( m_size.height() / 2 + int( magR ) );

        index = m_scale[i];
    }

    Q_UNUSED(allZero);
    return false;
}

double Spectrum::clamp(double cur, double max, double min)
{
    if (cur > max)
        cur = max;
    if (cur < min)
        cur = min;
    return cur;
}

bool Spectrum::draw(QPainter *p, const QColor &back)
{
    // This draws on a pixmap owned by MainVisual.
    //
    // In other words, this is not a Qt Widget, it
    // just uses some Qt methods to draw on a pixmap.
    // MainVisual then bitblts that onto the screen.

    QRect *rectsp = m_rects.data();

    p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    for (uint i = 0; i < (uint)m_rects.size(); i++)
    {
        double per = double( rectsp[i].height() - 2 ) / double( m_size.height() );

        per = clamp(per, 1.0, 0.0);

        double r = m_startColor.red() +
            (m_targetColor.red() - m_startColor.red()) * (per * per);
        double g = m_startColor.green() +
            (m_targetColor.green() - m_startColor.green()) * (per * per);
        double b = m_startColor.blue() +
            (m_targetColor.blue() - m_startColor.blue()) * (per * per);

        r = clamp(r, 255.0, 0.0);
        g = clamp(g, 255.0, 0.0);
        b = clamp(b, 255.0, 0.0);

        if(rectsp[i].height() > 4)
            p->fillRect(rectsp[i], QColor(int(r), int(g), int(b)));
    }

    return true;
}

static class SpectrumFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Spectrum");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new Spectrum();
    }
}SpectrumFactory;

///////////////////////////////////////////////////////////////////////////////
// Squares
//

Squares::Squares()
{
    m_fakeHeight = m_numberOfSquares * m_analyzerBarWidth;
}

void Squares::resize (const QSize &newsize) {
    // Trick the spectrum analyzer into calculating 16 rectangles
    Spectrum::resize (QSize (m_fakeHeight, m_fakeHeight));
    // We have our own copy, Spectrum has it's own...
    m_actualSize = newsize;
}

void Squares::drawRect(QPainter *p, QRect *rect, int i, int c, int w, int h)
{
    double per = NAN;
    int correction = (m_actualSize.width() % m_rects.size ()) / 2;
    int x = ((i / 2) * w) + correction;
    int y = 0;

    if (i % 2 == 0)
    {
        y = c - h;
        per = double(m_fakeHeight - rect->top()) / double(m_fakeHeight);
    }
    else
    {
        y = c;
        per = double(rect->bottom()) / double(m_fakeHeight);
    }

    per = clamp(per, 1.0, 0.0);

    double r = m_startColor.red() +
        (m_targetColor.red() - m_startColor.red()) * (per * per);
    double g = m_startColor.green() +
        (m_targetColor.green() - m_startColor.green()) * (per * per);
    double b = m_startColor.blue() +
        (m_targetColor.blue() - m_startColor.blue()) * (per * per);

    r = clamp(r, 255.0, 0.0);
    g = clamp(g, 255.0, 0.0);
    b = clamp(b, 255.0, 0.0);

    p->fillRect (x, y, w, h, QColor (int(r), int(g), int(b)));
}

bool Squares::draw(QPainter *p, const QColor &back)
{
    p->fillRect (0, 0, m_actualSize.width(), m_actualSize.height(), back);
    int w = m_actualSize.width() / (m_rects.size() / 2);
    int h = w;
    int center = m_actualSize.height() / 2;

    QRect *rectsp = m_rects.data();
    for (uint i = 0; i < (uint)m_rects.size(); i++)
        drawRect(p, &(rectsp[i]), i, center, w, h);

    return true;
}

static class SquaresFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Squares");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new Squares();
    }
}SquaresFactory;


Piano::Piano()
{
    // Setup the "magical" audio coefficients
    // required by the Goetzel Algorithm

    LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Being Initialised"));

    m_pianoData = (piano_key_data *) malloc(sizeof(piano_key_data) * PIANO_N);
    m_audioData = (piano_audio *) malloc(sizeof(piano_audio) * PIANO_AUDIO_SIZE);

    double sample_rate = 44100.0;  // TODO : This should be obtained from gPlayer (likely candidate...)

    m_fps = 20; // This is the display frequency.   We're capturing all audio chunks by defining .process_undisplayed() though.

    double concert_A   =   440.0;
    double semi_tone   = pow(2.0, 1.0/12.0);

    /* Lowest note on piano is 4 octaves below concert A */
    double bottom_A = concert_A / 2.0 / 2.0 / 2.0 / 2.0;

    double current_freq = bottom_A;
    for (uint key = 0; key < PIANO_N; key++)
    {
        // This is constant through time
        m_pianoData[key].coeff = (goertzel_data)(2.0 * cos(2.0 * M_PI * current_freq / sample_rate));

        // Want 20 whole cycles of the current waveform at least
        double samples_required = sample_rate/current_freq * 20.0;
        if (samples_required > sample_rate/4.0)
        {
            // For the really low notes, 4 updates a second is good enough...
            samples_required = sample_rate/4.0;
        }
        if (samples_required < sample_rate/(double)m_fps * 0.75)
        {   // For the high notes, use as many samples as we need in a display_fps
            samples_required = sample_rate/(double)m_fps * 0.75;
        }
        m_pianoData[key].samples_process_before_display_update = (int)samples_required;
        m_pianoData[key].is_black_note = false; // Will be put right in .resize()

        current_freq *= semi_tone;
    }

    zero_analysis();
}

Piano::~Piano()
{
    if (m_pianoData)
        free(m_pianoData);
    if (m_audioData)
        free(m_audioData);
}

void Piano::zero_analysis(void)
{
    for (uint key = 0; key < PIANO_N; key++)
    {
        // These get updated continously, and must be stored between chunks of audio data
        m_pianoData[key].q2 = 0.0F;
        m_pianoData[key].q1 = 0.0F;
        m_pianoData[key].magnitude = 0.0F;
        m_pianoData[key].max_magnitude_seen =
            (goertzel_data)(PIANO_RMS_NEGLIGIBLE*PIANO_RMS_NEGLIGIBLE); // This is a guess - will be quickly overwritten

        m_pianoData[key].samples_processed = 0;
    }
    m_offsetProcessed = 0ms;
}

void Piano::resize(const QSize &newsize)
{
    // Just change internal data about the
    // size of the pixmap to be drawn (ie. the
    // size of the screen) and the logically
    // ensuing number of up/down bars to hold
    // the audio magnitudes

    m_size = newsize;

    LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Being Resized"));

    zero_analysis();

    // There are 88-36=52 white notes on piano keyboard
    double key_unit_size = (double)m_size.width() / 54.0;  // One white key extra spacing, if possible
    if (key_unit_size < 10.0) // Keys have to be at least this many pixels wide
        key_unit_size = 10.0;

    double white_width_pct = .8;
    double black_width_pct = .6;
    double black_offset_pct = .05;

    double white_height_pct = 6;
    double black_height_pct = 4;

    // This is the starting position of the keyboard (may be beyond LHS)
    // - actually position of C below bottom A (will be added to...).  This is 4 octaves below middle C.
    double left =  (double)m_size.width() / 2.0 - (4.0*7.0 + 3.5) * key_unit_size; // The extra 3.5 centers 'F' inthe middle of the screen
    double top_of_keys = (double)m_size.height() / 2.0 - key_unit_size * white_height_pct / 2.0; // Vertically center keys

    m_rects.resize(PIANO_N);

    for (uint key = 0; key < PIANO_N; key++)
    {
        int note = ((int)key - 3 + 12) % 12;  // This means that C=0, C#=1, D=2, etc (since lowest note is bottom A)
        if (note == 0) // If we're on a 'C', move the left 'cursor' over an octave
        {
            left += key_unit_size*7.0;
        }

        double center = 0.0;
        double offset = 0.0;
        bool is_black = false;

        switch (note)
        {
            case 0:  center = 0.5; break;
            case 1:  center = 1.0; is_black = true; offset = -1; break;
            case 2:  center = 1.5; break;
            case 3:  center = 2.0; is_black = true; offset = +1; break;
            case 4:  center = 2.5; break;
            case 5:  center = 3.5; break;
            case 6:  center = 4.0; is_black = true; offset = -2; break;
            case 7:  center = 4.5; break;
            case 8:  center = 5.0; is_black = true; offset = 0; break;
            case 9:  center = 5.5; break;
            case 10: center = 6.0; is_black = true; offset = 2; break;
            case 11: center = 6.5; break;
        }
        m_pianoData[key].is_black_note = is_black;

        double width = (is_black ? black_width_pct:white_width_pct) * key_unit_size;
        double height = (is_black? black_height_pct:white_height_pct) * key_unit_size;

        m_rects[key].setRect(
            left + center * key_unit_size //  Basic position of left side of key
                - width / 2.0  // Less half the width
                + (is_black ? (offset * black_offset_pct * key_unit_size):0.0), // And jiggle the positions of the black keys for aethetic reasons
            top_of_keys, // top
            width, // width
            height // height
        );
    }

    m_magnitude.resize(PIANO_N);
    for (double & key : m_magnitude)
        key = 0.0;
}

unsigned long Piano::getDesiredSamples(void)
{
    // We want all the data! (within reason)
    //   typical observed values are 882 -
    //   12.5 chunks of data per second from 44100Hz signal : Sampled at 50Hz, lots of 4, see :
    //   mythtv/libs/libmyth/audio/audiooutputbase.cpp :: AudioOutputBase::AddData
    //   See : mythtv/mythplugins/mythmusic/mythmusic/avfdecoder.cpp "20ms worth"
    return (unsigned long) PIANO_AUDIO_SIZE;  // Maximum we can be given
}

bool Piano::processUndisplayed(VisualNode *node)
{
    //LOG(VB_GENERAL, LOG_INFO, QString("Piano : Processing undisplayed node"));
    return process_all_types(node, false);
}

bool Piano::process(VisualNode *node)
{
    //LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Processing node for DISPLAY"));
//    return process_all_types(node, true);
    process_all_types(node, true);
    return false;
}

bool Piano::process_all_types(VisualNode *node, bool /*this_will_be_displayed*/)
{
    // Take a bunch of data in *node and break it down into piano key spectrum values
    // NB: Remember the state data between calls, so as to accumulate more accurate results.
    bool allZero = true;
    uint n = 0;

    if (node)
    {
        piano_audio short_to_bounded = 32768.0F;

        // Detect start of new song (current node more than 10s earlier than already seen)
        if (node->m_offset + 10s < m_offsetProcessed)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Node offset=%1 too far backwards : NEW SONG").arg(node->m_offset.count()));
            zero_analysis();
        }

        // Check whether we've seen this node (more recently than 10secs ago)
        if (node->m_offset <= m_offsetProcessed)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Already seen node offset=%1, returning without processing").arg(node->m_offset.count()));
            return allZero; // Nothing to see here - the server can stop if it wants to
        }

        //LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Processing node offset=%1, size=%2").arg(node->m_offset).arg(node->m_length));
        n = node->m_length;

        if (node->m_right) // Preprocess the data into a combined middle channel, if we have stereo data
        {
            for (uint i = 0; i < n; i++)
            {
                m_audioData[i] = ((piano_audio)node->m_left[i] + (piano_audio)node->m_right[i]) / 2.0F / short_to_bounded;
            }
        }
        else // This is only one channel of data
        {
            for (uint i = 0; i < n; i++)
            {
                m_audioData[i] = (piano_audio)node->m_left[i] / short_to_bounded;
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Hit an empty node, and returning empty-handed"));
        return allZero; // Nothing to see here - the server can stop if it wants to
    }

    for (uint key = 0; key < PIANO_N; key++)
    {
        goertzel_data coeff = m_pianoData[key].coeff;
        goertzel_data q2 = m_pianoData[key].q2;
        goertzel_data q1 = m_pianoData[key].q1;

        for (uint i = 0; i < n; i++)
        {
            goertzel_data q0 = coeff * q1 - q2 + m_audioData[i];
            q2 = q1;
            q1 = q0;
        }
        m_pianoData[key].q2 = q2;
        m_pianoData[key].q1 = q1;

        m_pianoData[key].samples_processed += n;

        int n_samples = m_pianoData[key].samples_processed;

        // Only do this update if we've processed enough chunks for this key...
        if (n_samples > m_pianoData[key].samples_process_before_display_update)
        {
            goertzel_data magnitude2 = q1*q1 + q2*q2 - q1*q2*coeff;

#if 0
            // This is RMS of signal
            goertzel_data magnitude_av =
                sqrt(magnitude2)/(goertzel_data)n_samples; // Should be 0<magnitude_av<.5
#else
            // This is pure magnitude of signal
            goertzel_data magnitude_av =
                magnitude2/(goertzel_data)n_samples/(goertzel_data)n_samples; // Should be 0<magnitude_av<.25
#endif

#if 0
            // Take logs everywhere, and shift up to [0, ??]
            if(magnitude_av > 0.0F)
            {
                magnitude_av = log(magnitude_av);
            }
            else
            {
                magnitude_av = PIANO_MIN_VOL;
            }
            magnitude_av -= PIANO_MIN_VOL;

            if (magnitude_av < 0.0F)
            {
                magnitude_av = 0.0;
            }
#endif

            if (magnitude_av > (goertzel_data)0.01)
            {
                allZero = false;
            }

            m_pianoData[key].magnitude = magnitude_av; // Store this for later : We'll do the colours from this...
            if ( m_pianoData[key].max_magnitude_seen < magnitude_av)
            {
                m_pianoData[key].max_magnitude_seen = magnitude_av;
            }
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Updated Key %1 from %2 samples, magnitude=%3")
                    .arg(key).arg(n_samples).arg(magnitude_av));

            m_pianoData[key].samples_processed = 0; // Reset the counts, now that we've set the magnitude...
            m_pianoData[key].q1 = (goertzel_data)0.0;
            m_pianoData[key].q2 = (goertzel_data)0.0;
        }
    }

    if (node)
    {
        // All done now - record that we've done this offset
        m_offsetProcessed = node->m_offset;
    }

    return allZero;
}

double Piano::clamp(double cur, double max, double min)
{
    if (cur > max)
        cur = max;
    if (cur < min)
        cur = min;
    return cur;
}

bool Piano::draw(QPainter *p, const QColor &back)
{
    // This draws on a pixmap owned by MainVisual.
    //
    // In other words, this is not a Qt Widget, it
    // just uses some Qt methods to draw on a pixmap.
    // MainVisual then bitblts that onto the screen.

    QRect *rectsp = &m_rects[0];
    double *magnitudep = &m_magnitude[0];

    unsigned int n = PIANO_N;

    p->fillRect(0, 0, m_size.width(), m_size.height(), back);

    // Protect maximum array length
    if(n > (uint)m_rects.size())
        n = (uint)m_rects.size();

    // Sweep up across the keys, making sure the max_magnitude_seen is at minimum X% of its neighbours
    double mag = PIANO_RMS_NEGLIGIBLE;
    for (uint key = 0; key < n; key++)
    {
        if (m_pianoData[key].max_magnitude_seen < static_cast<float>(mag))
        {
            // Spread the previous value to this key
            m_pianoData[key].max_magnitude_seen = mag;
        }
        else
        {
            // This key has seen better peaks, use this for the next one
            mag = m_pianoData[key].max_magnitude_seen;
        }
        mag *= PIANO_SPECTRUM_SMOOTHING;
    }

    // Similarly, down, making sure the max_magnitude_seen is at minimum X% of its neighbours
    mag = PIANO_RMS_NEGLIGIBLE;
    for (int key_i = n - 1; key_i >= 0; key_i--)
    {
        uint key = key_i; // Wow, this is to avoid a zany error for ((unsigned)0)--
        if (m_pianoData[key].max_magnitude_seen < static_cast<float>(mag))
        {
            // Spread the previous value to this key
            m_pianoData[key].max_magnitude_seen = mag;
        }
        else
        {
            // This key has seen better peaks, use this for the next one
            mag = m_pianoData[key].max_magnitude_seen;
        }
        mag *= PIANO_SPECTRUM_SMOOTHING;
    }

    // Now find the key that has been hit the hardest relative to its experience, and renormalize...
    // Set a minimum, to prevent divide-by-zero (and also all-pressed when music very quiet)
    double magnitude_max = PIANO_RMS_NEGLIGIBLE;
    for (uint key = 0; key < n; key++)
    {
        mag = m_pianoData[key].magnitude / m_pianoData[key].max_magnitude_seen;
        if (magnitude_max < mag)
            magnitude_max = mag;

        magnitudep[key] = mag;
    }

    // Deal with all the white keys first
    for (uint key = 0; key < n; key++)
    {
        if (m_pianoData[key].is_black_note)
            continue;

        double per = magnitudep[key] / magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < PIANO_KEYPRESS_TOO_LIGHT)
            per = 0.0; // Clamp to zero for lightly detected keys
        LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Display key %1, magnitude=%2, seen=%3")
                .arg(key).arg(per*100.0).arg(m_pianoData[key].max_magnitude_seen));

        double r = m_whiteStartColor.red() + (m_whiteTargetColor.red() - m_whiteStartColor.red()) * per;
        double g = m_whiteStartColor.green() + (m_whiteTargetColor.green() - m_whiteStartColor.green()) * per;
        double b = m_whiteStartColor.blue() + (m_whiteTargetColor.blue() - m_whiteStartColor.blue()) * per;

        p->fillRect(rectsp[key], QColor(int(r), int(g), int(b)));
    }

    // Then overlay the black keys
    for (uint key = 0; key < n; key++)
    {
        if (!m_pianoData[key].is_black_note)
            continue;

        double per = magnitudep[key]/magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < PIANO_KEYPRESS_TOO_LIGHT)
            per = 0.0; // Clamp to zero for lightly detected keys

        double r = m_blackStartColor.red() + (m_blackTargetColor.red() - m_blackStartColor.red()) * per;
        double g = m_blackStartColor.green() + (m_blackTargetColor.green() - m_blackStartColor.green()) * per;
        double b = m_blackStartColor.blue() + (m_blackTargetColor.blue() - m_blackStartColor.blue()) * per;

        p->fillRect(rectsp[key], QColor(int(r), int(g), int(b)));
    }

    return true;
}

static class PianoFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Piano");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new Piano();
    }
}PianoFactory;

AlbumArt::AlbumArt(void) :
    m_lastCycle(QDateTime::currentDateTime())
{
    findFrontCover();
    m_fps = 1;
}

void AlbumArt::findFrontCover(void)
{
    if (!gPlayer->getCurrentMetadata())
        return;

    // if a front cover image is available show that first
    AlbumArtImages *albumArt = gPlayer->getCurrentMetadata()->getAlbumArtImages();
    if (albumArt->getImage(IT_FRONTCOVER))
        m_currImageType = IT_FRONTCOVER;
    else
    {
        // not available so just show the first image available
        if (albumArt->getImageCount() > 0)
            m_currImageType = albumArt->getImageAt(0)->m_imageType;
        else
            m_currImageType = IT_UNKNOWN;
    }
}

bool AlbumArt::cycleImage(void)
{
    if (!gPlayer->getCurrentMetadata())
        return false;

    AlbumArtImages *albumArt = gPlayer->getCurrentMetadata()->getAlbumArtImages();
    int newType = m_currImageType;

    // If we only have one image there is nothing to cycle
    if (albumArt->getImageCount() > 1)
    {
        do
        {
            newType++;
            if (newType == IT_LAST)
                newType = IT_UNKNOWN;
        } while (!albumArt->getImage((ImageType) newType));
    }

    if (newType != m_currImageType)
    {
        m_currImageType = (ImageType) newType;
        m_lastCycle = QDateTime::currentDateTime();
        return true;
    }

    return false;
}

void AlbumArt::resize(const QSize &newsize)
{
    m_size = newsize;
}

bool AlbumArt::process(VisualNode */*node*/)
{
    return false;
}

void AlbumArt::handleKeyPress(const QString &action)
{
    if (action == "SELECT")
    {
        if (gPlayer->getCurrentMetadata())
        {
            AlbumArtImages albumArt(gPlayer->getCurrentMetadata());
            int newType = m_currImageType;

            if (albumArt.getImageCount() > 0)
            {
                newType++;

                while (!albumArt.getImage((ImageType) newType))
                {
                    newType++;
                    if (newType == IT_LAST)
                        newType = IT_UNKNOWN;
                }
            }

            if (newType != m_currImageType)
            {
                m_currImageType = (ImageType) newType;
                // force an update
                m_cursize = QSize(0, 0);
            }
        }
    }
}

/// this is the time an image is shown in the albumart visualizer
#define ALBUMARTCYCLETIME 10

bool AlbumArt::needsUpdate()
{
    // if the track has changed we need to update the image
    if (gPlayer->getCurrentMetadata() && m_currentMetadata != gPlayer->getCurrentMetadata())
    {
        m_currentMetadata = gPlayer->getCurrentMetadata();
        findFrontCover();
        return true;
    }

    // if it's time to cycle to the next image we need to update the image
    if (m_lastCycle.addSecs(ALBUMARTCYCLETIME) < QDateTime::currentDateTime())
    {
        if (cycleImage())
            return true;
    }

    return false;
}

bool AlbumArt::draw(QPainter *p, const QColor &back)
{
    if (needsUpdate())
    {
        QImage art;
        QString imageFilename = gPlayer->getCurrentMetadata()->getAlbumArtFile(m_currImageType);

        if (imageFilename.startsWith("myth://"))
        {
            auto *rf = new RemoteFile(imageFilename, false, false, 0ms);

            QByteArray data;
            bool ret = rf->SaveAs(data);

            delete rf;

            if (ret)
                art.loadFromData(data);
        }
        else
            if (!imageFilename.isEmpty())
                art.load(imageFilename);

        if (art.isNull())
        {
            m_cursize = m_size;
            m_image = QImage();
        }
        else
        {
            m_image = art.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (m_image.isNull())
    {
        drawWarning(p, back, m_size, tr("?"), 100);
        return true;
    }

    // Paint the image
    p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    p->drawImage((m_size.width() - m_image.width()) / 2,
                 (m_size.height() - m_image.height()) / 2,
                 m_image);

    // Store our new size
    m_cursize = m_size;

    return true;
}

static class AlbumArtFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "AlbumArt");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new AlbumArt();
    }
}AlbumArtFactory;

Blank::Blank()
    : VisualBase(true)
{
    m_fps = 1;
}

void Blank::resize(const QSize &newsize)
{
    m_size = newsize;
}


bool Blank::process(VisualNode */*node*/)
{
    return false;
}

bool Blank::draw(QPainter *p, const QColor &back)
{
    // Took me hours to work out this algorithm
    p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    return true;
}

static class BlankFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Blank");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new Blank();
    }
}BlankFactory;
