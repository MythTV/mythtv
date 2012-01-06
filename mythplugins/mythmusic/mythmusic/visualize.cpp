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
using namespace std;

// Qt
#include <QPainter>
#include <QImage>

// MythTV
#include <mythdbcon.h>
#include <mythcontext.h>
#include <mythuihelper.h>

// mythmusic
#include "mainvisual.h"
#include "visualize.h"
#include "inlines.h"
#include "decoder.h"
#include "metadata.h"
#include "musicplayer.h"

#define FFTW_N 512
// static_assert(FFTW_N==SAMPLES_DEFAULT_SIZE)


VisFactory* VisFactory::g_pVisFactories = 0;

VisualBase::VisualBase(bool screensaverenable)
    : m_fps(20), m_xscreensaverenable(screensaverenable)
{
    if (!m_xscreensaverenable)
        GetMythUI()->DoDisableScreensaver();
}

VisualBase::~VisualBase()
{
    //
    //    This is only here so
    //    that derived classes
    //    can destruct properly
    //
    if (!m_xscreensaverenable)
        GetMythUI()->DoRestoreScreensaver();
}


void VisualBase::drawWarning(QPainter *p, const QColor &back, const QSize &size, QString warning)
{
    p->fillRect(0, 0, size.width(), size.height(), back);
    p->setPen(Qt::white);
    p->setFont(GetMythUI()->GetMediumFont());

    QFontMetrics fm(p->font());
    int width = fm.width(warning);
    int height = fm.height() * (warning.contains("\n") ? 2 : 1);
    int x = size.width() / 2 - width / 2;
    int y = size.height() / 2 - height / 2;

    for (int offset = 0; offset < height; offset += fm.height()) {
        QString l = warning.left(warning.indexOf("\n"));
        p->drawText(x, y + offset, width, height, Qt::AlignCenter, l);
        warning.remove(0, l.length () + 1);
    }
}

///////////////////////////////////////////////////////////////////////////////
// LogScale

LogScale::LogScale(int maxscale, int maxrange)
    : indices(0), s(0), r(0)
{
    setMax(maxscale, maxrange);
}

LogScale::~LogScale()
{
    if (indices)
        delete [] indices;
}

void LogScale::setMax(int maxscale, int maxrange)
{
    if (maxscale == 0 || maxrange == 0)
        return;

    s = maxscale;
    r = maxrange;

    if (indices)
        delete [] indices;

    double alpha;
    int i, scaled;
    long double domain = (long double) maxscale;
    long double range  = (long double) maxrange;
    long double x  = 1.0;
    long double dx = 1.0;
    long double y  = 0.0;
    long double yy = 0.0;
    long double t  = 0.0;
    long double e4 = 1.0E-8;

    indices = new int[maxrange];
    for (i = 0; i < maxrange; i++)
        indices[i] = 0;

    // initialize log scale
    for (uint i=0; i<10000 && (std::abs(dx) > e4); i++)
    {
        t = std::log((domain + x) / x);
        y = (x * t) - range;
        yy = t - (domain / (x + domain));
        dx = y / yy;
        x -= dx;
    }

    alpha = x;
    for (i = 1; i < (int) domain; i++)
    {
        scaled = (int) floor(0.5 + (alpha * log((double(i) + alpha) / alpha)));
        if (scaled < 1)
            scaled = 1;
        if (indices[scaled - 1] < i)
            indices[scaled - 1] = i;
    }
}

int LogScale::operator[](int index)
{
    return indices[index];
}

///////////////////////////////////////////////////////////////////////////////
// StereoScope

#define RUBBERBAND 0
#define TWOCOLOUR 0
StereoScope::StereoScope() :
    startColor(Qt::green), targetColor(Qt::red),
    rubberband(RUBBERBAND), falloff(1.0)
{
    m_fps = 45;
}

StereoScope::~StereoScope()
{
}

void StereoScope::resize( const QSize &newsize )
{
    size = newsize;

    uint os = magnitudes.size();
    magnitudes.resize( size.width() * 2 );
    for ( ; os < magnitudes.size(); os++ )
        magnitudes[os] = 0.0;
}

bool StereoScope::process( VisualNode *node )
{
    bool allZero = true;


    if (node) 
    {
        double index = 0;
        double const step = (double)SAMPLES_DEFAULT_SIZE / size.width();
        for ( int i = 0; i < size.width(); i++) 
        {
            unsigned long indexTo = (unsigned long)(index + step);
            if (indexTo == (unsigned long)(index))
                indexTo = (unsigned long)(index + 1);

            double valL = 0, valR = 0;
#if RUBBERBAND
            if ( rubberband ) {
                valL = magnitudes[ i ];
                valR = magnitudes[ i + size.width() ];
                if (valL < 0.) {
                    valL += falloff;
                    if ( valL > 0. )
                        valL = 0.;
                }
                else
                {
                    valL -= falloff;
                    if ( valL < 0. )
                        valL = 0.;
                }
                if (valR < 0.) 
                {
                    valR += falloff;
                    if ( valR > 0. )
                        valR = 0.;
                } 
                else 
                {
                    valR -= falloff;
                    if ( valR < 0. )
                        valR = 0.;
                }
            }
#endif
            for (unsigned long s = (unsigned long)index; s < indexTo && s < node->length; s++) 
            {
                double tmpL = ( ( node->left ?
                       double( node->left[s] ) : 0.) *
                     double( size.height() / 4 ) ) / 32768.;
                double tmpR = ( ( node->right ?
                       double( node->right[s]) : 0.) *
                     double( size.height() / 4 ) ) / 32768.;
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

            magnitudes[ i ] = valL;
            magnitudes[ i + size.width() ] = valR;

            index = index + step;
        }
#if RUBBERBAND
    } 
    else if (rubberband) 
    {
        for ( int i = 0; i < size.width(); i++) 
        {
            double valL = magnitudes[ i ];
            if (valL < 0) {
                valL += 2;
                if (valL > 0.)
                    valL = 0.;
            } else {
                valL -= 2;
                if (valL < 0.)
                    valL = 0.;
            }

            double valR = magnitudes[ i + size.width() ];
            if (valR < 0.) {
                valR += falloff;
                if (valR > 0.)
                    valR = 0.;
            }
            else
            {
                valR -= falloff;
                if (valR < 0.)
                    valR = 0.;
            }

            if (valL != 0. || valR != 0.)
                allZero = false;

            magnitudes[ i ] = valL;
            magnitudes[ i + size.width() ] = valR;
        }
#endif
    } 
    else 
    {
        for ( int i = 0; (unsigned) i < magnitudes.size(); i++ )
            magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool StereoScope::draw( QPainter *p, const QColor &back )
{
    p->fillRect(0, 0, size.width(), size.height(), back);
    for ( int i = 1; i < size.width(); i++ ) 
    {
#if TWOCOLOUR
    double r, g, b, per;

    // left
    per = double( magnitudes[ i ] * 2 ) /
          double( size.height() / 4 );
    if (per < 0.0)
        per = -per;
    if (per > 1.0)
        per = 1.0;
    else if (per < 0.0)
        per = 0.0;

    r = startColor.red() + (targetColor.red() -
                startColor.red()) * (per * per);
    g = startColor.green() + (targetColor.green() -
                  startColor.green()) * (per * per);
    b = startColor.blue() + (targetColor.blue() -
                 startColor.blue()) * (per * per);

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
    p->drawLine( i - 1, (int)((size.height() / 4) + magnitudes[i - 1]),
             i, (int)((size.height() / 4) + magnitudes[i]));

#if TWOCOLOUR
    // right
    per = double( magnitudes[ i + size.width() ] * 2 ) /
          double( size.height() / 4 );
    if (per < 0.0)
        per = -per;
    if (per > 1.0)
        per = 1.0;
    else if (per < 0.0)
        per = 0.0;

    r = startColor.red() + (targetColor.red() -
                startColor.red()) * (per * per);
    g = startColor.green() + (targetColor.green() -
                  startColor.green()) * (per * per);
    b = startColor.blue() + (targetColor.blue() -
                 startColor.blue()) * (per * per);

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
    p->drawLine( i - 1, (int)((size.height() * 3 / 4) +
             magnitudes[i + size.width() - 1]),
             i, (int)((size.height() * 3 / 4) +
                     magnitudes[i + size.width()]));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// MonoScope

MonoScope::MonoScope()
{
}

MonoScope::~MonoScope()
{
}

bool MonoScope::process( VisualNode *node )
{
    bool allZero = true;

    if (node)
    {
        double index = 0;
        double const step = (double)SAMPLES_DEFAULT_SIZE / size.width();
        for (int i = 0; i < size.width(); i++)
        {
            unsigned long indexTo = (unsigned long)(index + step);
            if (indexTo == (unsigned long)index)
                indexTo = (unsigned long)(index + 1);

            double val = 0;
#if RUBBERBAND
            if ( rubberband )
            {
                val = magnitudes[ i ];
                if (val < 0.)
                {
                    val += falloff;
                    if ( val > 0. )
                    {
                        val = 0.;
                    }
                }
                else
                {
                    val -= falloff;
                    if ( val < 0. )
                    {
                        val = 0.;
                    }
                }
            }
#endif
            for (unsigned long s = (unsigned long)index; s < indexTo && s < node->length; s++)
            {
                double tmp = ( double( node->left[s] ) +
                        (node->right ? double( node->right[s] ) : 0) *
                        double( size.height() / 2 ) ) / 65536.;
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
            magnitudes[ i ] = val;
            index = index + step;
        }
    }
#if RUBBERBAND
    else if (rubberband)
    {
        for (int i = 0; i < size.width(); i++) {
            double val = magnitudes[ i ];
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
            magnitudes[ i ] = val;
        }
    }
#endif
    else
    {
        for (int i = 0; i < size.width(); i++ )
            magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool MonoScope::draw( QPainter *p, const QColor &back )
{
    p->fillRect( 0, 0, size.width(), size.height(), back );
    for ( int i = 1; i < size.width(); i++ ) {
#if TWOCOLOUR
        double r, g, b, per;

        per = double( magnitudes[ i ] ) /
              double( size.height() / 4 );
        if (per < 0.0)
            per = -per;
        if (per > 1.0)
            per = 1.0;
        else if (per < 0.0)
            per = 0.0;

        r = startColor.red() + (targetColor.red() -
                                startColor.red()) * (per * per);
        g = startColor.green() + (targetColor.green() -
                                  startColor.green()) * (per * per);
        b = startColor.blue() + (targetColor.blue() -
                                 startColor.blue()) * (per * per);

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
        p->drawLine( i - 1, (int)(size.height() / 2 + magnitudes[ i - 1 ]),
                     i, (int)(size.height() / 2 + magnitudes[ i ] ));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// StereoScopeFactory

static class StereoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("StereoScope");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new StereoScope();
    }
}StereoScopeFactory;


///////////////////////////////////////////////////////////////////////////////
// MonoScopeFactory

static class MonoScopeFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("MonoScope");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new MonoScope();
    }
}MonoScopeFactory;

Spectrum::Spectrum()
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    : lin(NULL), rin(NULL), lout(NULL), rout(NULL)
#endif
{
    // Setup the "magical" audio data transformations
    // provided by the Fast Fourier Transforms library
    analyzerBarWidth = 6;
    scaleFactor = 2.0;
    falloff = 10.0;
    m_fps = 15;

#ifdef FFTW3_SUPPORT
    lin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    memset(lin, 0, sizeof(myth_fftw_float)*FFTW_N);

    rin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    memset(rin, 0, sizeof(myth_fftw_float)*FFTW_N);

    lout = (myth_fftw_complex*) av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    memset(lout, 0, sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    rout = (myth_fftw_complex*) av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    memset(rout, 0, sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    lplan = fftw_plan_dft_r2c_1d(FFTW_N, lin, (myth_fftw_complex_cast*)lout, FFTW_MEASURE);
    rplan = fftw_plan_dft_r2c_1d(FFTW_N, rin, (myth_fftw_complex_cast*)rout, FFTW_MEASURE);
#elif FFTW2_SUPPORT
    lin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    memset(lin, 0, sizeof(fftw_real)*FFTW_N);

    rin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    memset(rin, 0, sizeof(fftw_real)*FFTW_N);

    lout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);
    memset(lout, 0, sizeof(fftw_real)*FFTW_N*2);

    rout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);
    memset(rout, 0, sizeof(fftw_real)*FFTW_N*2);

    plan = rfftw_create_plan(FFTW_N, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif // FFTW2_SUPPORT

    startColor = QColor(0,0,255);
    targetColor = QColor(255,0,0); 
}

Spectrum::~Spectrum()
{
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    if (lin)
        av_free(lin);
    if (rin)
        av_free(rin);
    if (lout)
        av_free(lout);
    if (rout)
        av_free(rout);
#endif
#ifdef FFTW3_SUPPORT
    fftw_destroy_plan(lplan);
    fftw_destroy_plan(rplan);
#elif FFTW2_SUPPORT
    rfftw_destroy_plan(plan);
#endif
}

void Spectrum::resize(const QSize &newsize)
{
    // Just change internal data about the
    // size of the pixmap to be drawn (ie. the
    // size of the screen) and the logically
    // ensuing number of up/down bars to hold
    // the audio magnitudes

    size = newsize;

    analyzerBarWidth = size.width() / 64;

    if (analyzerBarWidth < 6)
        analyzerBarWidth = 6;

    scale.setMax(192, size.width() / analyzerBarWidth);

    rects.resize( scale.range() );
    unsigned int i = 0;
    int w = 0;
    for (; i < (uint)rects.size(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    unsigned int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; os < (uint)magnitudes.size(); os++)
    {
        magnitudes[os] = 0.0;
    }

    scaleFactor = double( size.height() / 2 ) / log( (double)(FFTW_N) );
}

template<typename T> T sq(T a) { return a*a; };

bool Spectrum::process(VisualNode *node)
{
    // Take a bunch of data in *node
    // and break it down into spectrum
    // values
    bool allZero = TRUE;
    // Don't do any real processing if libfftw isn't available
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    uint i;
    long w = 0, index;
    QRect *rectsp = rects.data();
    double *magnitudesp = magnitudes.data();
    double magL, magR, tmp;

    if (node) 
    {
        i = node->length;
        fast_real_set_from_short(lin, node->left, node->length);
        if (node->right)
            fast_real_set_from_short(rin, node->right, node->length);
    } 
    else
        i = 0;

    fast_reals_set(lin + i, rin + i, 0, FFTW_N - i);

#ifdef FFTW3_SUPPORT
    fftw_execute(lplan);
    fftw_execute(rplan);
#elif FFTW2_SUPPORT
    rfftw_one(plan, lin, lout);
    rfftw_one(plan, rin, rout);
#endif

    index = 1;

    for (i = 0; (int)i < rects.size(); i++, w += analyzerBarWidth)
    {
#ifdef FFTW3_SUPPORT
        magL = (log(sq(real(lout[index])) + sq(real(lout[FFTW_N - index]))) - 22.0) * 
               scaleFactor;
        magR = (log(sq(real(rout[index])) + sq(real(rout[FFTW_N - index]))) - 22.0) * 
               scaleFactor;
#elif FFTW2_SUPPORT
        magL = (log(sq(lout[index]) + sq(lout[FFTW_N - index])) - 22.0) * 
               scaleFactor;
        magR = (log(sq(rout[index]) + sq(rout[FFTW_N - index])) - 22.0) * 
               scaleFactor;
#endif

        if (magL > size.height() / 2)
        {
            magL = size.height() / 2;
        }
        if (magL < magnitudesp[i])
        {
            tmp = magnitudesp[i] - falloff;
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

        if (magR > size.height() / 2)
        {
            magR = size.height() / 2;
        }
        if (magR < magnitudesp[i + scale.range()])
        {
            tmp = magnitudesp[i + scale.range()] - falloff;
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
        magnitudesp[i + scale.range()] = magR;
        rectsp[i].setTop( size.height() / 2 - int( magL ) );
        rectsp[i].setBottom( size.height() / 2 + int( magR ) );

        index = scale[i];
    }
#else
    node = node;
#endif
    return allZero;
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

#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    QRect *rectsp = rects.data();
    double r, g, b, per;

    p->fillRect(0, 0, size.width(), size.height(), back);
    for (uint i = 0; i < (uint)rects.size(); i++)
    {
        per = double( rectsp[i].height() - 2 ) / double( size.height() );

        per = clamp(per, 1.0, 0.0);

        r = startColor.red() + 
            (targetColor.red() - startColor.red()) * (per * per);
        g = startColor.green() + 
            (targetColor.green() - startColor.green()) * (per * per);
        b = startColor.blue() + 
            (targetColor.blue() - startColor.blue()) * (per * per);

        r = clamp(r, 255.0, 0.0);
        g = clamp(g, 255.0, 0.0);
        b = clamp(b, 255.0, 0.0);

        if(rectsp[i].height() > 4)
            p->fillRect(rectsp[i], QColor(int(r), int(g), int(b)));
    }

#else
    // Oops ... user doesn't have a Fast Fourier Library
    drawWarning(p, back, size,
                QObject::tr("Visualization requires FFT library") + "\n" + 
                QObject::tr("Did you run configure?"));
#endif

    return true;
}

static class SpectrumFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Spectrum");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Spectrum();
    }
}SpectrumFactory;

Piano::Piano()
    : piano_data(NULL), audio_data(NULL)
{
    // Setup the "magical" audio coefficients
    // required by the Goetzel Algorithm

    LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Being Initialised"));

    piano_data = (piano_key_data *) malloc(sizeof(piano_key_data) * PIANO_N);
    audio_data = (piano_audio *) malloc(sizeof(piano_audio) * PIANO_AUDIO_SIZE);

    double sample_rate = 44100.0;  // TODO : This should be obtained from gPlayer (likely candidate...)

    m_fps = 20; // This is the display frequency.   We're capturing all audio chunks by defining .process_undisplayed() though.

    double concert_A   =   440.0;
    double semi_tone   = pow(2.0, 1.0/12.0);

    /* Lowest note on piano is 4 octaves below concert A */
    double bottom_A = concert_A / 2.0 / 2.0 / 2.0 / 2.0;

    unsigned int key;
    double current_freq = bottom_A, samples_required;
    for (key = 0; key < PIANO_N; key++)
    {
        // This is constant through time
        piano_data[key].coeff = (goertzel_data)(2.0 * cos(2.0 * M_PI * current_freq / sample_rate));

        samples_required = sample_rate/current_freq * 20.0; // Want 20 whole cycles of the current waveform at least
        if (samples_required > sample_rate/4.0)
        {
            // For the really low notes, 4 updates a second is good enough...
            samples_required = sample_rate/4.0;
        }
        if (samples_required < sample_rate/(double)m_fps * 0.75)
        {   // For the high notes, use as many samples as we need in a display_fps
            samples_required = sample_rate/(double)m_fps * 0.75;
        }
        piano_data[key].samples_process_before_display_update = (int)samples_required;
        piano_data[key].is_black_note = false; // Will be put right in .resize()

        current_freq *= semi_tone;
    }

    zero_analysis();

    whiteStartColor = QColor(245,245,245);
    whiteTargetColor = Qt::red;

    blackStartColor = QColor(10,10,10);
    blackTargetColor = Qt::red;
}

Piano::~Piano()
{
    if (piano_data)
        free(piano_data);
    if (audio_data)
        free(audio_data);
}

void Piano::zero_analysis(void)
{
    unsigned int key;
    for (key = 0; key < PIANO_N; key++)
    {
        // These get updated continously, and must be stored between chunks of audio data
        piano_data[key].q2 = (goertzel_data)0.0f;
        piano_data[key].q1 = (goertzel_data)0.0f;
        piano_data[key].magnitude = (goertzel_data)0.0f;
        piano_data[key].max_magnitude_seen =
            (goertzel_data)(PIANO_RMS_NEGLIGIBLE*PIANO_RMS_NEGLIGIBLE); // This is a guess - will be quickly overwritten

        piano_data[key].samples_processed = 0;
    }
    offset_processed = 0;

    return;
}

void Piano::resize(const QSize &newsize)
{
    // Just change internal data about the
    // size of the pixmap to be drawn (ie. the
    // size of the screen) and the logically
    // ensuing number of up/down bars to hold
    // the audio magnitudes

    size = newsize;

    LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Being Resized"));

    zero_analysis();

    // There are 88-36=52 white notes on piano keyboard
    double key_unit_size = (double)size.width() / 54.0;  // One white key extra spacing, if possible
    if (key_unit_size < 10.0) // Keys have to be at least this many pixels wide
        key_unit_size = 10.0;

    double white_width_pct = .8;
    double black_width_pct = .6;
    double black_offset_pct = .05;

    double white_height_pct = 6;
    double black_height_pct = 4;

    // This is the starting position of the keyboard (may be beyond LHS)
    // - actually position of C below bottom A (will be added to...).  This is 4 octaves below middle C.
    double left =  (double)size.width() / 2.0 - (4.0*7.0 + 3.5) * key_unit_size; // The extra 3.5 centers 'F' inthe middle of the screen
    double top_of_keys = (double)size.height() / 2.0 - key_unit_size * white_height_pct / 2.0; // Vertically center keys

    double width, height, center, offset;

    rects.resize(PIANO_N);

    unsigned int key;
    int note;
    bool is_black = false;
    for (key = 0; key < PIANO_N; key++)
    {
        note = ((int)key - 3 + 12) % 12;  // This means that C=0, C#=1, D=2, etc (since lowest note is bottom A)
        if (note == 0) // If we're on a 'C', move the left 'cursor' over an octave
        {
            left += key_unit_size*7.0;
        }

        center = 0.0;
        offset = 0.0;
        is_black = false;

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
        piano_data[key].is_black_note = is_black;

        width = (is_black ? black_width_pct:white_width_pct) * key_unit_size;
        height = (is_black? black_height_pct:white_height_pct) * key_unit_size;

        rects[key].setRect(
            left + center * key_unit_size //  Basic position of left side of key
                - width / 2.0  // Less half the width
                + (is_black ? (offset * black_offset_pct * key_unit_size):0.0), // And jiggle the positions of the black keys for aethetic reasons
            top_of_keys, // top
            width, // width
            height // height
        );
    }

    magnitude.resize(PIANO_N);
    for (key = 0; key < (uint)magnitude.size(); key++)
    {
        magnitude[key] = 0.0;
    }

    return;
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

bool Piano::process_all_types(VisualNode *node, bool this_will_be_displayed)
{
    (void) this_will_be_displayed;

    // Take a bunch of data in *node and break it down into piano key spectrum values
    // NB: Remember the state data between calls, so as to accumulate more accurate results.
    bool allZero = true;

    uint i, n, key;

    goertzel_data q0, q1, q2, coeff;
    goertzel_data magnitude2, magnitude_av;
    piano_audio short_to_bounded = 32768.0f;
    int n_samples;

    if (node)
    {
        // Detect start of new song (current node more than 10s earlier than already seen)
        if (node->offset + 10000 < offset_processed)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Node offset=%1 too far backwards : NEW SONG").arg(node->offset));
            zero_analysis();
        }

        // Check whether we've seen this node (more recently than 10secs ago)
        if (node->offset <= offset_processed)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Already seen node offset=%1, returning without processing").arg(node->offset));
            return allZero; // Nothing to see here - the server can stop if it wants to
        }
    }

    if (node)
    {
        //LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Processing node offset=%1, size=%2").arg(node->offset).arg(node->length));
        n = node->length;

        if (node->right) // Preprocess the data into a combined middle channel, if we have stereo data
        {
            for (i = 0; i < n; i++)
            {
                audio_data[i] = (piano_audio)(((piano_audio)node->left[i] + (piano_audio)node->right[i]) / 2.0 / short_to_bounded);
            }
        }
        else // This is only one channel of data
        {
            for (i = 0; i < n; i++)
            {
                audio_data[i] = (piano_audio)node->left[i] / short_to_bounded;
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Hit an empty node, and returning empty-handed"));
        return allZero; // Nothing to see here - the server can stop if it wants to
    }

    for (key = 0; key < PIANO_N; key++)
    {
        coeff = piano_data[key].coeff;

        q2 = piano_data[key].q2;
        q1 = piano_data[key].q1;

        for (i = 0; i < n; i++)
        {
            q0 = coeff * q1 - q2 + audio_data[i];
            q2 = q1;
            q1 = q0;
        }
        piano_data[key].q2 = q2;
        piano_data[key].q1 = q1;

        piano_data[key].samples_processed += n;

        n_samples = piano_data[key].samples_processed;

        // Only do this update if we've processed enough chunks for this key...
        if (n_samples > piano_data[key].samples_process_before_display_update)
        {
            magnitude2 = q1*q1 + q2*q2 - q1*q2*coeff;

            if (false) // This is RMS of signal
            {
                magnitude_av = sqrt(magnitude2)/(goertzel_data)n_samples; // Should be 0<magnitude_av<.5
            }
            if (true) // This is pure magnitude of signal
            {
                magnitude_av = magnitude2/(goertzel_data)n_samples/(goertzel_data)n_samples; // Should be 0<magnitude_av<.25
            }

            if (false) // Take logs everywhere, and shift up to [0, ??]
            {
                if(magnitude_av > 0.0)
                {
                    magnitude_av = log(magnitude_av);
                }
                else
                {
                    magnitude_av = PIANO_MIN_VOL;
                }
                magnitude_av -= PIANO_MIN_VOL;

                if (magnitude_av < 0.0)
                {
                    magnitude_av = 0.0;
                }
            }

            if (magnitude_av > (goertzel_data)0.01)
            {
                allZero = false;
            }

            piano_data[key].magnitude = magnitude_av; // Store this for later : We'll do the colours from this...
            if ( piano_data[key].max_magnitude_seen < magnitude_av)
            {
                piano_data[key].max_magnitude_seen = magnitude_av;
            }
            LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Updated Key %1 from %2 samples, magnitude=%3")
                    .arg(key).arg(n_samples).arg(magnitude_av));

            piano_data[key].samples_processed = 0; // Reset the counts, now that we've set the magnitude...
            piano_data[key].q1 = (goertzel_data)0.0;
            piano_data[key].q2 = (goertzel_data)0.0;
        }
    }

    // All done now - record that we've done this offset
    offset_processed = node->offset;
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

    QRect *rectsp = rects.data();
    double *magnitudep = magnitude.data();

    unsigned int key, n = PIANO_N;
    double r, g, b, per;

    p->fillRect(0, 0, size.width(), size.height(), back);

    // Protect maximum array length
    if(n > (uint)rects.size())
        n = (uint)rects.size();

    // Sweep up across the keys, making sure the max_magnitude_seen is at minimum X% of its neighbours
    double mag = PIANO_RMS_NEGLIGIBLE;
    for (key = 0; key < n; key++)
    {
        if (piano_data[key].max_magnitude_seen < mag)
        {
            // Spread the previous value to this key
            piano_data[key].max_magnitude_seen = mag;
        }
        else
        {
            // This key has seen better peaks, use this for the next one
            mag = piano_data[key].max_magnitude_seen;
        }
        mag *= PIANO_SPECTRUM_SMOOTHING;
    }

    // Similarly, down, making sure the max_magnitude_seen is at minimum X% of its neighbours
    mag = PIANO_RMS_NEGLIGIBLE;
    for (int key_i = n - 1; key_i >= 0; key_i--)
    {
        key = key_i; // Wow, this is to avoid a zany error for ((unsigned)0)--
        if (piano_data[key].max_magnitude_seen < mag)
        {
            // Spread the previous value to this key
            piano_data[key].max_magnitude_seen = mag;
        }
        else
        {
            // This key has seen better peaks, use this for the next one
            mag = piano_data[key].max_magnitude_seen;
        }
        mag *= PIANO_SPECTRUM_SMOOTHING;
    }

    // Now find the key that has been hit the hardest relative to its experience, and renormalize...
    // Set a minimum, to prevent divide-by-zero (and also all-pressed when music very quiet)
    double magnitude_max = PIANO_RMS_NEGLIGIBLE;
    for (key = 0; key < n; key++)
    {
        mag = piano_data[key].magnitude / piano_data[key].max_magnitude_seen;
        if (magnitude_max < mag)
            magnitude_max = mag;

        magnitudep[key] = mag;
    }

    // Deal with all the white keys first
    for (key = 0; key < n; key++)
    {
        if (piano_data[key].is_black_note)
            continue;

        per = magnitudep[key] / magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < PIANO_KEYPRESS_TOO_LIGHT)
            per = 0.0; // Clamp to zero for lightly detected keys
        LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Display key %1, magnitude=%2, seen=%3")
                .arg(key).arg(per*100.0).arg(piano_data[key].max_magnitude_seen));

        r = whiteStartColor.red() + (whiteTargetColor.red() - whiteStartColor.red()) * per;
        g = whiteStartColor.green() + (whiteTargetColor.green() - whiteStartColor.green()) * per;
        b = whiteStartColor.blue() + (whiteTargetColor.blue() - whiteStartColor.blue()) * per;

        p->fillRect(rectsp[key], QColor(int(r), int(g), int(b)));
    }

    // Then overlay the black keys
    for (key = 0; key < n; key++)
    {
        if (!piano_data[key].is_black_note)
            continue;

        per = magnitudep[key]/magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < PIANO_KEYPRESS_TOO_LIGHT)
            per = 0.0; // Clamp to zero for lightly detected keys

        r = blackStartColor.red() + (blackTargetColor.red() - blackStartColor.red()) * per;
        g = blackStartColor.green() + (blackTargetColor.green() - blackStartColor.green()) * per;
        b = blackStartColor.blue() + (blackTargetColor.blue() - blackStartColor.blue()) * per;

        p->fillRect(rectsp[key], QColor(int(r), int(g), int(b)));
    }

    if (0)
    {
        drawWarning(p, back, size,
                QObject::tr("Piano Visualization Message") + "\n" +
                QObject::tr("Are you sane running this code?"));
    }
    return true;
}

static class PianoFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Piano");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Piano();
    }
}PianoFactory;

AlbumArt::AlbumArt(void)
{
    findFrontCover();

    m_lastCycle = QDateTime::currentDateTime();

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
            m_currImageType = albumArt->getImageAt(0)->imageType;
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

    if (albumArt->getImageCount() > 0)
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

AlbumArt::~AlbumArt()
{
}

void AlbumArt::resize(const QSize &newsize)
{
    m_size = newsize;
}

bool AlbumArt::process(VisualNode *node)
{
    (void) node;
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
        drawWarning(p, back, m_size, QObject::tr("?"));
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
    const QString &name(void) const
    {
        static QString name("AlbumArt");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void) parent;
        (void)pluginName;
        return new AlbumArt();
    }
}AlbumArtFactory;

Blank::Blank()
    : VisualBase(true)
{
    m_fps = 20;
}

Blank::~Blank()
{
}

void Blank::resize(const QSize &newsize)
{
    size = newsize;
}


bool Blank::process(VisualNode *node)
{
    node = node; // Sometimes I hate -Wall
    return true;
}

bool Blank::draw(QPainter *p, const QColor &back)
{
    // Took me hours to work out this algorithm
    p->fillRect(0, 0, size.width(), size.height(), back);
    return true;
}

static class BlankFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Blank");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Blank();
    }
}BlankFactory;

Squares::Squares() :
    size(0,0), pParent(NULL), fake_height(0), number_of_squares(16)
{
    fake_height = number_of_squares * analyzerBarWidth;
}

Squares::~Squares()
{
}

void Squares::resize (const QSize &newsize) {
    // Trick the spectrum analyzer into calculating 16 rectangles
    Spectrum::resize (QSize (fake_height, fake_height));
    // We have our own copy, Spectrum has it's own...
    size = newsize;
}

void Squares::drawRect(QPainter *p, QRect *rect, int i, int c, int w, int h) 
{
    double r, g, b, per;
    int correction = (size.width() % rects.size ()) / 2;
    int x = ((i / 2) * w) + correction;
    int y;

    if (i % 2 == 0) 
    {
        y = c - h;
        per = double(fake_height - rect->top()) / double(fake_height);
    }
    else
    {
        y = c;
        per = double(rect->bottom()) / double(fake_height);
    }

    per = clamp(per, 1.0, 0.0);        
    
    r = startColor.red() + 
        (targetColor.red() - startColor.red()) * (per * per);
    g = startColor.green() + 
        (targetColor.green() - startColor.green()) * (per * per);
    b = startColor.blue() + 
        (targetColor.blue() - startColor.blue()) * (per * per);
    
    r = clamp(r, 255.0, 0.0);
    g = clamp(g, 255.0, 0.0);
    b = clamp(b, 255.0, 0.0);

    p->fillRect (x, y, w, h, QColor (int(r), int(g), int(b)));
}

bool Squares::draw(QPainter *p, const QColor &back)
{
    p->fillRect (0, 0, size.width (), size.height (), back);
    int w = size.width () / (rects.size () / 2);
    int h = w;
    int center = size.height () / 2;

    (void) h;
    (void) center;
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    QRect *rectsp = rects.data();
    for (uint i = 0; i < (uint)rects.size(); i++)
        drawRect(p, &(rectsp[i]), i, center, w, h);

#else
    // Oops ... user doesn't have a Fast Fourier Library
    drawWarning(p, back, size,
                QObject::tr("Visualization requires FFT library") + "\n" + 
                QObject::tr("Did you run configure?"));
#endif

    return true;
}

static class SquaresFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Squares");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent, const QString &pluginName) const
    {
        (void)parent;
        (void)pluginName;
        return new Squares();
    }
}SquaresFactory;
#if 0
#ifdef OPENGL_SUPPORT

//        Need this for the Gears Object (below)
static void gear( GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
                  GLint teeth, GLfloat tooth_depth )
{
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth/2.0;
    r2 = outer_radius + tooth_depth/2.0;

    const double pi = 3.14159264;
    da = 2.0*pi / teeth / 4.0;

    glShadeModel( GL_FLAT );

    glNormal3f( 0.0, 0.0, 1.0 );

    /* draw front face */
    glBegin( GL_QUAD_STRIP );
    for (i=0;i<=teeth;i++) {
        angle = i * 2.0*pi / teeth;
        glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
        glVertex3f( r1*cos(angle), r1*sin(angle), width*0.5 );
        glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5 );
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin( GL_QUADS );
    da = 2.0*pi / teeth / 4.0;
    for (i=0;i<teeth;i++) {
        angle = i * 2.0*pi / teeth;

        glVertex3f( r1*cos(angle),      r1*sin(angle),          width*0.5 );
        glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),          width*0.5 );
        glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), width*0.5 );
        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5 );
    }
    glEnd();


    glNormal3f( 0.0, 0.0, -1.0 );

    /* draw back face */
    glBegin( GL_QUAD_STRIP );
    for (i=0;i<=teeth;i++) {
        angle = i * 2.0*pi / teeth;
        glVertex3f( r1*cos(angle), r1*sin(angle), -width*0.5 );
        glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
        glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin( GL_QUADS );
    da = 2.0*pi / teeth / 4.0;
    for (i=0;i<teeth;i++) {
        angle = i * 2.0*pi / teeth;

        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
        glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5 );
        glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),          -width*0.5 );
        glVertex3f( r1*cos(angle),      r1*sin(angle),          -width*0.5 );
    }
    glEnd();


    /* draw outward faces of teeth */
    glBegin( GL_QUAD_STRIP );
    for (i=0;i<teeth;i++) {
        angle = i * 2.0*pi / teeth;

        glVertex3f( r1*cos(angle),      r1*sin(angle),           width*0.5 );
        glVertex3f( r1*cos(angle),      r1*sin(angle),          -width*0.5 );
        u = r2*cos(angle+da) - r1*cos(angle);
        v = r2*sin(angle+da) - r1*sin(angle);
        len = sqrt( u*u + v*v );
        u /= len;
        v /= len;
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),           width*0.5 );
        glVertex3f( r2*cos(angle+da),   r2*sin(angle+da),          -width*0.5 );
        glNormal3f( cos(angle), sin(angle), 0.0 );
        glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da),  width*0.5 );
        glVertex3f( r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5 );
        u = r1*cos(angle+3*da) - r2*cos(angle+2*da);
        v = r1*sin(angle+3*da) - r2*sin(angle+2*da);
        glNormal3f( v, -u, 0.0 );
        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da),  width*0.5 );
        glVertex3f( r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5 );
        glNormal3f( cos(angle), sin(angle), 0.0 );
    }

    glVertex3f( r1*cos(0.0), r1*sin(0.0), width*0.5 );
    glVertex3f( r1*cos(0.0), r1*sin(0.0), -width*0.5 );

    glEnd();


    glShadeModel( GL_SMOOTH );

    /* draw inside radius cylinder */
    glBegin( GL_QUAD_STRIP );
    for (i=0;i<=teeth;i++) {
        angle = i * 2.0*pi / teeth;
        glNormal3f( -cos(angle), -sin(angle), 0.0 );
        glVertex3f( r0*cos(angle), r0*sin(angle), -width*0.5 );
        glVertex3f( r0*cos(angle), r0*sin(angle), width*0.5 );
    }
    glEnd();

}

//        Want to clean this up at some point,
//        but I need to get some CVS checked in
//        before we end up with a code fork

static GLfloat view_rotx=20.0, view_rotz=0.0;
static GLint gear1, gear2, gear3;

Gears::Gears(QWidget *parent, const char *name) :
    QGLWidget(parent)
{
    setObjectName(name);
    falloff = 4.0;
    analyzerBarWidth = 10;
    fps = 60;

    // Slightly trick bit: This *is* a Qt Qidget 
    // (unlike spectrum, above) so we just use
    // the Qt GL class.

    //int screenwidth = 0, screenheight = 0;
    //float wmult = 0.0, hmult = 0.0;

    //gCoreContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    setGeometry(0, 0, parent->width(), parent->height());
    //setGeometry(0, 0, 400, 300);
    //setFixedSize(QSize(screenwidth, screenheight));

    angle = 0.0;
    view_roty = 30.0;

#ifdef FFTW3_SUPPORT
    lin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    rin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    lout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    rout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    lplan = fftw_plan_dft_r2c_1d(FFTW_N, lin, (myth_fftw_complex_cast*) lout, FFTW_MEASURE);
    rplan = fftw_plan_dft_r2c_1d(FFTW_N, rin, (myth_fftw_complex_cast*) rout, FFTW_MEASURE);
#elif FFTW2_SUPPORT
    lin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    rin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    lout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);
    rout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);

    plan = rfftw_create_plan(FFTW_N, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif // FFTW2_SUPPORT


    startColor = QColor(0,0,255);
    targetColor = QColor(255,0,0); 

    show();
}

Gears::~Gears()
{
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    if (lin)
        av_free(lin);
    if (rin)
        av_free(rin);
    if (lout)
        av_free(lout);
    if (rout)
        av_free(rout);
#endif
#ifdef FFTW3_SUPPORT
    fftw_destroy_plan(lplan);
    fftw_destroy_plan(rplan);
#elif FFTW2_SUPPORT
    rfftw_destroy_plan(plan);
#endif
}

void Gears::resize(const QSize &newsize)
{
    size = newsize;
    scale.setMax(192, size.width() / analyzerBarWidth);

    rects.resize( scale.range() );
    int i = 0, w = 0;
    for (; (unsigned) i < (uint)rects.size(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; (unsigned) os < (uint)magnitudes.size(); os++)
    {
        magnitudes[os] = 0.0;
    }

    scaleFactor = double( size.height() / 2 ) / log( (double)FFTW_N );
    //resizeGL(newsize.width(), newsize.height());
    setGeometry(0, 0, newsize.width(), newsize.height());
}


bool Gears::process(VisualNode *node)
{
    bool allZero = TRUE;
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    uint i;
    long w = 0, index;
    QRect *rectsp = rects.data();
    double *magnitudesp = magnitudes.data();
    double magL, magR, tmp;

    if (node) 
    {
        i = node->length;
        fast_real_set_from_short(lin, node->left, node->length);
        if (node->right)
        {
            fast_real_set_from_short(rin, node->right, node->length);
        }
    } 
    else
    {
        i = 0;
    }

    fast_reals_set(lin + i, rin + i, 0, FFTW_N - i);
#ifdef FFTW3_SUPPORT
    fftw_execute(lplan);
    fftw_execute(rplan);
#elif FFTW2_SUPPORT
    rfftw_one(plan, lin, lout);
    rfftw_one(plan, rin, rout);
#endif
    index = 1;
    for (i = 0; i < (uint)rects.size(); i++, w += analyzerBarWidth)
    {
#ifdef FFTW3_SUPPORT
        magL = (log(sq(real(lout[index])) + sq(real(lout[FFTW_N - index]))) - 22.0) * 
               scaleFactor;
        magR = (log(sq(real(rout[index])) + sq(real(rout[FFTW_N - index]))) - 22.0) * 
               scaleFactor;
#elif FFTW2_SUPPORT
        magL = (log(lout[index] * lout[index] + lout[FFTW_N - index] * lout[FFTW_N - index]) - 22.0) * scaleFactor;
        magR = (log(rout[index] * rout[index] + rout[FFTW_N - index] * rout[FFTW_N - index]) - 22.0) * scaleFactor;
#endif

        if (magL > size.height() / 2)
        {        
            magL = size.height() / 2;
        }
        if (magL < magnitudesp[i])
        {
            tmp = magnitudesp[i] - falloff;
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

        if (magR > size.height() / 2)
        {
            magR = size.height() / 2;
        }
        if (magR < magnitudesp[i + scale.range()])
        {
            tmp = magnitudesp[i + scale.range()] - falloff;
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
        magnitudesp[i + scale.range()] = magR;

        rectsp[i].setTop( size.height() / 2 - int( magL ) );
        rectsp[i].setBottom( size.height() / 2 + int( magR ) );

        index = scale[i];
    }
#else
    node = node;
#endif
    return allZero;

}

bool Gears::draw(QPainter *p, const QColor &back)
{
    updateGL();
    p->fillRect(0, 0, 1, 1, back);
    return false;
}

void Gears::drawTheGears()
{
    angle += 2.0;        
    view_roty += 1.0;
    //view_rotx += 1.0;

    float spreader = 3.0 - ((rects[2].top() / 255.0) * 3.0);
        
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glPushMatrix();
    glRotatef( view_rotx, 1.0, 0.0, 0.0 );
    glRotatef( view_roty, 0.0, 1.0, 0.0 );
    glRotatef( view_rotz, 0.0, 0.0, 1.0 );

    glTranslatef(0.0, 2.0, 0.0);

    glPushMatrix();
//    glTranslatef( -3.0, -2.0, 0.0 );
    glTranslatef(-3.0, -2.0, 0.0 );
    glRotatef( angle, 0.0, 0.0, 1.0 );
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( 3.1, -2.0, 0.0 );
//    glTranslatef( 3.1 + spreader, -2.0, 0.0 );
    glRotatef( -2.0*angle-9.0, 0.0, 0.0, 1.0 );
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
//  glTranslatef( -3.1, 2.2, -1.8 );
    glTranslatef( -3.1, 2.2 + spreader, -1.8 );
    glRotatef( 90.0, 1.0, 0.0, 0.0 );
    glRotatef( 2.0*angle-2.0, 0.0, 0.0, 1.0 );
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();
}

void Gears::initializeGL()
{
    static GLfloat pos[4] = {5.0, 5.0, 10.0, 1.0 };
    static GLfloat ared[4] = {0.8, 0.1, 0.0, 1.0 };
    static GLfloat agreen[4] = {0.0, 0.8, 0.2, 1.0 };
    static GLfloat ablue[4] = {0.2, 0.2, 1.0, 1.0 };

    glLightfv( GL_LIGHT0, GL_POSITION, pos );
    glEnable( GL_CULL_FACE );
    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );
    glEnable( GL_DEPTH_TEST );

    /* make the gears */
    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ared );
    gear( 1.0, 4.0, 1.0, 20, 0.7 );
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, agreen );
    gear( 0.5, 2.0, 2.0, 10, 0.7 );
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ablue );
    gear( 1.3, 2.0, 0.5, 10, 0.7 );
    glEndList();

    glEnable( GL_NORMALIZE );
}

void Gears::resizeGL( int width, int height )
{
    GLfloat w = (float) width / (float) height;
    GLfloat h = 1.0;

    glViewport( 0, 0, width, height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum( -w, w, -h, h, 5.0, 60.0 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef( 0.0, 0.0, -40.0 );
}

void Gears::paintGL()
{
    drawTheGears();
}

static class GearsFactory : public VisFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Gears");
        return name;
    }

    uint plugins(QStringList *list) const
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual *parent,  const QString &pluginName) const
    {
        (void)pluginName;
        return new Gears(parent);
    }
}GearsFactory;


#endif
#endif 
