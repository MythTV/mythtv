/*
        visualize.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
        VERY closely based on code from mq3 by Brad Hughes

        Part of the mythTV project

        music visualizers
*/

// C++
#include <algorithm>
#include <cmath>
#include <iostream>

// Qt
#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QDir>

// MythTV
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/remotefile.h>
#include <libmythbase/sizetliteral.h>
#include <libmythbase/mythdirs.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuihelper.h>

// mythmusic
#include "decoder.h"
#include "inlines.h"
#include "mainvisual.h"
#include "musicplayer.h"
#include "visualize.h"

extern "C" {
    #include <libavutil/mem.h>
    #include <libavutil/tx.h>
}

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

void LogScale::setMax(int maxscale, int maxrange)
{
    if (maxscale == 0 || maxrange == 0)
        return;

    m_scale = maxscale;
    m_range = maxrange;

    auto domain = (long double) maxscale;
    auto range  = (long double) maxrange;
    long double x  = 1.0;
    long double dx = 1.0;
    long double e4 = 1.0E-8;

    m_indices.clear();
    m_indices.resize(maxrange, 0);

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
        scaled = std::max(scaled, 1);
        m_indices[scaled - 1] = std::max(m_indices[scaled - 1], i);
    }
}

int LogScale::operator[](int index)
{
    return m_indices[index];
}

///////////////////////////////////////////////////////////////////////////////
// MelScale, example: (8192 fft, 1920 pixels, 22050 hz)

MelScale::MelScale(int maxscale, int maxrange, int maxfreq)
{
    setMax(maxscale, maxrange, maxfreq);
}

void MelScale::setMax(int maxscale, int maxrange, int maxfreq)
{
    if (maxscale == 0 || maxrange == 0 || maxfreq == 0)
        return;

    m_scale = maxscale;
    m_range = maxrange;

    m_indices.clear();
    m_indices.resize(maxrange, 0);

    int note = 0;               // first note is C0
    double freq = 16.35;        // frequency of first note
    double next = pow(2.0, 1.0 / 12.0); // factor separating notes

    double maxmel = hz2mel(maxfreq);
    double hzperbin = (double) maxfreq / (double) maxscale;

    for (int i = 0; i < maxrange; i++)
    {
        double mel = maxmel * i / maxrange;
        double hz = mel2hz(mel);
        int bin = int(hz / hzperbin);
        m_indices[i] = bin;
        // LOG(VB_PLAYBACK, LOG_INFO, QString("Mel maxmel=%1, hzperbin=%2, hz=%3, i=%4, bin=%5")
        //     .arg(maxmel).arg(hzperbin).arg(hz).arg(i).arg(bin));

        if (hz > freq) { // map note to pixel location for note labels
            m_freqs[note] = lround(freq);
            m_pixels[note++] = i;
            freq *= next;
        }
    }
}

int MelScale::operator[](int index)
{
    return m_indices[index];
}

QString MelScale::note(int note)
{
    if (note < 0 || note > 125)
        return {};
    return m_notes[note % 12];
}

int MelScale::pixel(int note)
{
    if (note < 0 || note > 125)
        return 0;
    return m_pixels[note];
}

int MelScale::freq(int note)
{
    if (note < 0 || note > 125)
        return 0;
    return m_freqs[note];
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
    m_magnitudes.resize( m_size.width() * 2_UZ );
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
    if (back != Qt::green)      // hack!!! for WaveForm
    {
        p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    }
    for ( int i = 1; i < m_size.width(); i++ )
    {
#if TWOCOLOUR
    // left
    double per = ( static_cast<double>(m_magnitudes[i]) * 2.0 ) /
                 ( static_cast<double>(m_size.height()) / 4.0 );
    if (per < 0.0)
        per = -per;
    if (per > 1.0)
        per = 1.0;
    else if (per < 0.0)
        per = 0.0;

    double r = m_startColor.red() +
        ((m_targetColor.red() - m_startColor.red()) * (per * per));
    double g = m_startColor.green() +
        ((m_targetColor.green() - m_startColor.green()) * (per * per));
    double b = m_startColor.blue() +
        ((m_targetColor.blue() - m_startColor.blue()) * (per * per));

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
                 (int)(adjHeight - m_magnitudes[i - 1]),
                 i,
                 (int)(adjHeight - m_magnitudes[i]));

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

    r = m_startColor.red() + ((m_targetColor.red() -
                m_startColor.red()) * (per * per));
    g = m_startColor.green() + ((m_targetColor.green() -
                  m_startColor.green()) * (per * per));
    b = m_startColor.blue() + ((m_targetColor.blue() -
                 m_startColor.blue()) * (per * per));

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
                 (int)(adjHeight - m_magnitudes[i + m_size.width() - 1]),
                 i,
                 (int)(adjHeight - m_magnitudes[i + m_size.width()]));
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
                               (node->m_right ? static_cast<double>(node->m_right[s])
                                : static_cast<double>(node->m_left[s])) *
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
    if (back != Qt::green)      // hack!!! for WaveForm
    {
        p->fillRect( 0, 0, m_size.width(), m_size.height(), back );
    }
    for ( int i = 1; i < m_size.width(); i++ ) {
#if TWOCOLOUR
        double per = ( static_cast<double>(m_magnitudes[i]) * 2.0 ) /
                     ( static_cast<double>(m_size.height()) / 4.0 );
        if (per < 0.0)
            per = -per;
        if (per > 1.0)
            per = 1.0;
        else if (per < 0.0)
            per = 0.0;

        double r = m_startColor.red() +
            ((m_targetColor.red() - m_startColor.red()) * (per * per));
        double g = m_startColor.green() +
            ((m_targetColor.green() - m_startColor.green()) * (per * per));
        double b = m_startColor.blue() +
            ((m_targetColor.blue() - m_startColor.blue()) * (per * per));

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
                     (int)(adjHeight - m_magnitudes[ i - 1 ]),
                     i,
                     (int)(adjHeight - m_magnitudes[ i ] ));
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// WaveForm - see whole track - by twitham@sbcglobal.net, 2023/01

// As/of v34, switching from small preview to fullscreen destroys and
// recreates this object.  So I use this static to survive which works
// only becasue there is 1 instance per process.  If mythfrontend ever
// decides to display more than one visual at a time, it should also
// be fixed to not destroy the instance but rather to resize it
// dynamically.  At that time, this could return to an m_image class
// member.  But more of this file might also need refactored to enable
// long-life resizable visuals.

QImage WaveForm::s_image {nullptr}; // picture of full track

WaveForm::~WaveForm()
{
    saveload(nullptr);
    LOG(VB_PLAYBACK, LOG_INFO, QString("WF going down"));
}

// cache current track, if any, before loading cache of given track
void WaveForm::saveload(MusicMetadata *meta)
{
    QString cache = GetConfDir() + "/MythMusic/WaveForm";
    QString filename;
    m_stream = gPlayer->getPlayMode() == MusicPlayer::PLAYMODE_RADIO;
    if (m_currentMetadata)     // cache work in progress for next time
    {
        QDir dir(cache);
        if (!dir.exists())
        {
            dir.mkpath(cache);
        }
        filename = QString("%1/%2.png").arg(cache)
            .arg(m_stream ? 0 : m_currentMetadata->ID());
        LOG(VB_PLAYBACK, LOG_INFO, QString("WF saving to %1").arg(filename));
        if (!s_image.save(filename))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("WF saving to %1 failed: " + ENO).arg(filename));
        }
    }
    m_currentMetadata = meta;
    if (meta)                   // load previous work from cache
    {
        filename = QString("%1/%2.png").arg(cache).arg(m_stream ? 0 : meta->ID());
        LOG(VB_PLAYBACK, LOG_INFO, QString("WF loading from %1").arg(filename));
        if (!s_image.load(filename))
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("WF loading %1 failed, recreating").arg(filename));
        }
        // 60 seconds skips pixels with < 44100 streams like 22050,
        // but this is now compensated for by drawing wider "pixels"
        m_duration = m_stream ? 60000 : meta->Length().count(); // millisecs
    }

    // A track length of zero milliseconds is most likely wrong but
    // can accidentally happen.  Rather than crash on divide by zero
    // later, let's pretend it is 1 minute long.  If the file plays
    // successfully, then it should record the correct track length
    // and be more correct next time.
    if (m_duration <= 0)
        m_duration = 60000;

    if (s_image.isNull())
    {
        s_image = QImage(m_wfsize.width(), m_wfsize.height(), QImage::Format_RGB32);
        s_image.fill(Qt::black);
    }
    m_minl = 0;                 // drop last pixel, prepare for first
    m_maxl = 0;
    m_sqrl = 0;
    m_minr = 0;
    m_maxr = 0;
    m_sqrr = 0;
    m_position = 0;
    m_lastx = m_wfsize.width();
    m_font = QApplication::font();
    m_font.setPixelSize(18);    // small to be mostly unnoticed
}

unsigned long WaveForm::getDesiredSamples(void)
{
    // could be an adjustable class member, but this hard code works well
    return kWFAudioSize;    // maximum samples per update, may get less
}

bool WaveForm::process(VisualNode *node)
{
    // After 2023/01 bugfix below, processUndisplayed processes this
    // node again after this process!  If that is ever changed in
    // mainvisual.cpp, then this would need adjusted.

    // StereoScope overlay must process only the displayed nodes
    StereoScope::process(node);
    return false;               // update even when silent
}

bool WaveForm::processUndisplayed(VisualNode *node)
{
    // In 2023/01, v32 had a bug in mainvisual.cpp that this never
    // received any nodes. Once I fixed that:

    // <      m_vis->processUndisplayed(node);
    // >      m_vis->processUndisplayed(m_nodes.first());

    // now this receives *all* nodes.  So we don't need any processing
    // in ::process above as that would double process.

    MusicMetadata *meta = gPlayer->getCurrentMetadata();
    if (meta && meta != m_currentMetadata)
    {
        saveload(meta);
    }
    if (node && !s_image.isNull())
    {
        m_offset = node->m_offset.count() % m_duration; // for ::draw below
        m_right = node->m_right;
        uint n = node->m_length;
        // LOG(VB_PLAYBACK, LOG_DEBUG,
        //     QString("WF process %1 samples at %2, display=%3").
        //     arg(n).arg(m_offset).arg(displayed));

// TODO: interpolate timestamps to process correct samples per pixel
// rather than fitting all we get in 1 or more pixels

        for (uint i = 0; i < n; i++) // find min/max and sum of squares
        {
            short int val = node->m_left[i];
            m_maxl = std::max(val, m_maxl);
            m_minl = std::min(val, m_minl);
            m_sqrl += static_cast<long>(val) * static_cast<long>(val);
            if (m_right)
            {
                val = node->m_right[i];
                m_maxr = std::max(val, m_maxr);
                m_minr = std::min(val, m_minr);
                m_sqrr += static_cast<long>(val) * static_cast<long>(val);
            }
            m_position++;
        }
        uint xx = m_wfsize.width() * m_offset / m_duration;
        if (xx != m_lastx)   // draw one finished line of min/max/rms
        {
            if (m_lastx > xx - 1 || // REW seek or right to left edge wrap
                m_lastx < xx - 5)   // FFWD seek
            {
                m_lastx = xx - 1;
            }
            int h = m_wfsize.height() / 4;  // amplitude above or below zero
            int y = m_wfsize.height() / 4;  // left zero line
            int yr = m_wfsize.height() * 3 / 4; // right  zero line
            if (!m_right)
            {           // mono - drop full waveform below StereoScope
                y = yr;
            }
            // This "loop" runs only once except for short tracks or
            // low sample rates that need some of the virtual "lines"
            // to be drawn wider with more actual lines.  I'd rather
            // duplicate the vertical lines than draw rectangles since
            // lines are the more common case. -twitham

            QPainter painter(&s_image);
            for (uint x = m_lastx + 1; x <= xx; x++)
            {
                // LOG(VB_PLAYBACK, LOG_DEBUG,
                //     QString("WF painting at %1,%2/%3").arg(x).arg(y).arg(yr));

                // clear prior content of this column
                if (m_stream)  // clear 5 seconds of future, with wrap
                {
                    painter.fillRect(x, 0, 32 * 5,
                                     m_wfsize.height(), Qt::black);
                    painter.fillRect(x - m_wfsize.width(), 0, 32 * 5,
                                     m_wfsize.height(), Qt::black);
                } else {        // preserve the future, if any
                    painter.fillRect(x, 0, 1,
                                     m_wfsize.height(), Qt::black);
                }

                // Audacity uses 50,50,200 and 100,100,220 - I'm going
                // darker to better contrast the StereoScope overlay
                painter.setPen(qRgb(25, 25, 150)); // peak-to-peak
                painter.drawLine(x, y - (h * m_maxl / 32768),
                                 x, y - (h * m_minl / 32768));
                if (m_right)
                {
                    painter.drawLine(x, yr - (h * m_maxr / 32768),
                                     x, yr - (h * m_minr / 32768));
                }
                painter.setPen(qRgb(150, 25, 25)); // RMS
                int rmsl = sqrt(m_sqrl / m_position) * y / 32768;
                painter.drawLine(x, y - rmsl, x, y + rmsl);
                if (m_right)
                {
                    int rmsr = sqrt(m_sqrr / m_position) * y / 32768;
                    painter.drawLine(x, yr - rmsr, x, yr + rmsr);
                    painter.drawLine(x, m_wfsize.height() / 2, // L / R delta
                                     x, (m_wfsize.height() / 2) - rmsl + rmsr);
                }
            }
            m_minl = 0;                 // reset metrics for next line
            m_maxl = 0;
            m_sqrl = 0;
            m_minr = 0;
            m_maxr = 0;
            m_sqrr = 0;
            m_position = 0;
            m_lastx = xx;
        }
    }
    return false;
}

bool WaveForm::draw( QPainter *p, const QColor &back )
{
    p->fillRect(0, 0, 0, 0, back); // no clearing, here to suppress warning
    if (!s_image.isNull())
    {                        // background, updated by ::process above
        p->drawImage(0, 0, s_image.scaled(m_size,
                                          Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation));
    }

    StereoScope::draw(p, Qt::green); // green == no clearing!

    p->fillRect(m_size.width() * m_offset / m_duration, 0,
                1, m_size.height(), Qt::darkGray);

    if (m_showtext && m_size.width() > 500) // metrics in corners
    {
        p->setPen(Qt::darkGray);
        p->setFont(m_font);
        QRect text(5, 5, m_size.width() - 10, m_size.height() - 10);
        p->drawText(text, Qt::AlignTop | Qt::AlignLeft,
                    QString("%1:%2")
                    .arg(m_offset / 1000 / 60)
                    .arg(m_offset / 1000 % 60, 2, 10, QChar('0')));
        p->drawText(text, Qt::AlignTop | Qt::AlignHCenter,
                    QString("%1%")
                    .arg(100.0 * m_offset / m_duration, 0, 'f', 0));
        p->drawText(text, Qt::AlignTop | Qt::AlignRight,
                    QString("%1:%2")
                    .arg(m_duration / 1000 / 60)
                    .arg(m_duration / 1000 % 60, 2, 10, QChar('0')));
        p->drawText(text, Qt::AlignBottom | Qt::AlignLeft,
                    QString("%1 lines/s")
                    .arg(1000.0 * m_wfsize.width() / m_duration, 0, 'f', 1));
        p->drawText(text, Qt::AlignBottom | Qt::AlignRight,
                    QString("%1 ms/line")
                    .arg(1.0 * m_duration / m_wfsize.width(), 0, 'f', 1));
    }
    return true;
}

void WaveForm::handleKeyPress(const QString &action)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("WF keypress = %1").arg(action));

    if (action == "SELECT" || action == "2")
    {
        m_showtext = ! m_showtext;
    }
    else if (action == "DELETE" && !s_image.isNull())
    {
        s_image.fill(Qt::black);
    }
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
// WaveFormFactory

static class WaveFormFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "WaveForm");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(
        MainVisual */*parent*/, const QString &/*pluginName*/) const override
    {
        return new WaveForm();
    }
}WaveFormFactory;

///////////////////////////////////////////////////////////////////////////////
// Spectrogram - by twitham@sbcglobal.net, 2023/05
//
// A spectrogram needs the entire sound signal.  A wide sliding window
// is needed to detect low frequencies, 2^14 = 16384 works great.  But
// then the detected frequencies can linger up to 16384/44100=372
// milliseconds!  We solve this by shrinking the signal amplitude over
// time within the window.  This way all frequencies are still there
// but only recent time has high amplitude.  This nicely displays even
// quick sounds of a few milliseconds.

// Static class members survive size changes for continuous display.
// See the comment about s_image in WaveForm
QImage Spectrogram::s_image {nullptr}; // picture of spectrogram
int    Spectrogram::s_offset {0};      // position on screen

Spectrogram::Spectrogram(bool hist)
{
    LOG(VB_GENERAL, LOG_INFO,
        QString("Spectrogram : Being Initialised, history=%1").arg(hist));

    m_history = hist;   // historical spectrogram?, else spectrum only

    m_fps = 40;         // getting 1152 samples / 44100 = 38.28125 fps

    m_color = gCoreContext->GetNumSetting("MusicSpectrogramColor", 0);

    if (s_image.isNull())  // static histogram survives resize/restart
    {
        s_image = QImage(
            m_sgsize.width(), m_sgsize.height(), QImage::Format_RGB32);
        s_image.fill(Qt::black);
    }
    if (m_history)
    {
        m_image = &s_image;
    }
    else
    {
        m_image = new QImage(
            m_sgsize.width(), m_sgsize.height(), QImage::Format_RGB32);
        m_image->fill(Qt::black);
    }

    m_dftL = static_cast<float*>(av_malloc(sizeof(float) * m_fftlen));
    m_dftR = static_cast<float*>(av_malloc(sizeof(float) * m_fftlen));
    m_rdftTmp = static_cast<float*>(av_malloc(sizeof(float) * (m_fftlen + 2)));

    // should probably check that this succeeds
    av_tx_init(&m_rdftContext, &m_rdft, AV_TX_FLOAT_RDFT, 0, m_fftlen, &kTxScale, 0x0);

    // hack!!! Should 44100 sample rate be queried or measured?
    // Likely close enough for most audio recordings...
    m_scale.setMax(m_fftlen / 2, m_history ?
                   m_sgsize.height() / 2 : m_sgsize.width(), 44100/2);
    m_sigL.resize(m_fftlen);
    m_sigR.resize(m_fftlen);

    // TODO: promote this to a separate ColorSpectrum class

    // cache a continuous color spectrum ([0-1535] = 256 colors * 6 ramps):
    // 0000ff blue              from here, G of RGB ramps UP to:
    // 00ffff cyan              then       B ramps down to:
    // 00ff00 green             then       R ramps  UP  to:
    // ffff00 yellow            then       G ramps down to:
    // ff0000 red               then       B ramps  UP  to:
    // ff00ff magenta           then       R ramps down to:
    // 0000ff blue              we end where we started!
    static constexpr int UP { 2 };
    static constexpr int DN { 3 };
    static const std::array<int,6> red   {  0,  0, UP,  1,  1, DN }; // 0=OFF, 1=ON
    static const std::array<int,6> green { UP,  1,  1, DN,  0,  0 };
    static const std::array<int,6> blue  {  1, DN,  0,  0, UP,  1 };

    auto updateColor = [](int color, int up, int down)
    {
        if (color == UP)
            return up;
        if (color == DN)
            return down;
        return color * 255;
    };

    for (int i = 0; i < 6; i++) // for 6 color transitions...
    {
        int r = red[i];         // 0=OFF, 1=ON, UP, or DN
        int g = green[i];
        int b = blue[i];
        for (int u = 0; u < 256; u++) { // u ramps up
            int d = 256 - u;            // d ramps down
            m_red[  (i * 256) + u] = updateColor(r, u, d);
            m_green[(i * 256) + u] = updateColor(g, u, d);
            m_blue[ (i * 256) + u] = updateColor(b, u, d);
        }
    }
}

Spectrogram::~Spectrogram()
{
    av_freep(reinterpret_cast<void*>(&m_dftL));
    av_freep(reinterpret_cast<void*>(&m_dftR));
    av_freep(reinterpret_cast<void*>(&m_rdftTmp));
    av_tx_uninit(&m_rdftContext);
}

void Spectrogram::resize(const QSize &newsize)
{
    m_size = newsize;
}

// this moved up from Spectrum so both can use it
template<typename T> T sq(T a) { return a*a; };

unsigned long Spectrogram::getDesiredSamples(void)
{
    // maximum samples per update, may get less
    return (unsigned long)kSGAudioSize;
}

bool Spectrogram::process(VisualNode */*node*/)
{
    if (!m_showtext)
        return false;

    QPainter painter(m_image);
    painter.setPen(Qt::white);
    QFont font = QApplication::font();
    font.setPixelSize(16);
    painter.setFont(font);
    int half = m_sgsize.height() / 2;

    if (m_history)
    {
        for (auto h = m_sgsize.height(); h > 0; h -= half)
        {
            for (auto i = 0; i < half; i += 20)
            {
                painter.drawText(s_offset > m_sgsize.width() - 255
                                 ? s_offset - m_sgsize.width() : s_offset,
                                 h - i - 20, 255, 40,
                                 Qt::AlignRight|Qt::AlignVCenter,
                                 // QString("%1 %2").arg(m_scale[i])
                                 QString("%1")
                                 .arg(m_scale[i] * 22050 / (m_fftlen/2)));
            }
        }
    } else {
        // static std::array<int, 5> treble = {52, 55, 59, 62, 65};
        // for (auto i = 0; i < 5; i++) {
        //     painter.drawLine(m_scale.pixel(treble[i]), 0,
        //                      m_scale.pixel(treble[i]), m_sgsize.height());
        // }
        for (auto i = 0; i < 125; i++) // 125 notes fit in 22050 Hz
        {                  // let Qt center the note text on the pixel
            painter.drawText(m_scale.pixel(i) - 20, half - ((i % 12) * 15) - 40,
                             40, 40, Qt::AlignCenter, m_scale.note(i));
            if (i % 12 == 5)    // octave numbers
            {
                painter.drawText(m_scale.pixel(i) - 20, half - 220,
                                 40, 40, Qt::AlignCenter,
                                 QString("%1").arg(i / 12));
            }
        }
        painter.rotate(90);     // frequency in Hz draws down
        int prev = -30;
        for (auto i = 0; i < 125; i++) // 125 notes fit in 22050 Hz
        {
            if (i < 72 && m_scale.note(i) == ".") // skip low sharps
              continue;
            int now = m_scale.pixel(i);
            if (now >= prev + 20) { // skip until good spacing
                painter.drawText(half + 20, (-1 * now) - 40,
                                 80, 80, Qt::AlignVCenter|Qt::AlignLeft,
                                 QString("%1").arg(m_scale.freq(i)));
                if (m_scale.note(i) != ".")
                {
                    painter.drawText(half + 100, (-1 * now) - 40,
                                     80, 80, Qt::AlignVCenter|Qt::AlignLeft,
                                     QString("%1%2").arg(m_scale.note(i))
                                     .arg(i / 12));
                }
                prev = now;
            }
        }
    }
    return false;
}

bool Spectrogram::processUndisplayed(VisualNode *node)
{
    // as of v33, this processes *all* samples, see the comments in WaveForm

    int w = m_sgsize.width();   // drawing size
    int h = m_sgsize.height();
    if (m_image->isNull())
    {
        m_image = new QImage(w, h, QImage::Format_RGB32);
        m_image->fill(Qt::black);
        s_offset = 0;
    }
    if (node)     // shift previous samples left, then append new node
    {
        // LOG(VB_PLAYBACK, LOG_DEBUG, QString("SG got %1 samples").arg(node->m_length));
        int i = std::min((int)(node->m_length), m_fftlen);
        int start = m_fftlen - i;
        float mult = 0.8F;      // decay older sound by this much
        for (int k = 0; k < start; k++)
        {                       // prior set ramps from mult to 1.0
            if (k > start - i && start > i)
            {
                mult = mult + ((1 - mult) *
                    (1 - (float)(start - k) / (float)(start - i)));
            }
            m_sigL[k] = mult * m_sigL[i + k];
            m_sigR[k] = mult * m_sigR[i + k];
        }
        for (int k = 0; k < i; k++) // append current samples
        {
            m_sigL[start + k] = node->m_left[k] / 32768.0F; // +/- 1 peak-to-peak
            if (node->m_right)
                m_sigR[start + k] = node->m_right[k] / 32768.0F;
        }
        int end = m_fftlen / 40; // ramp window ends down to zero crossing
        for (int k = 0; k < m_fftlen; k++)
        {
            if (k < end)
                mult = (float)k / (float)end;
            else if (k > m_fftlen - end)
                mult = (float)(m_fftlen - k) / (float)end;
            else
                mult = 1;
            m_dftL[k] = m_sigL[k] * mult;
            m_dftR[k] = m_sigR[k] * mult;
        }
    }
    // run the real FFT!
    m_rdft(m_rdftContext, m_rdftTmp, m_dftL, sizeof(float));
    m_rdftTmp[1] = m_rdftTmp[m_fftlen];
    memcpy(m_dftL, m_rdftTmp, m_fftlen * sizeof(float));
    m_rdft(m_rdftContext, m_rdftTmp, m_dftR, sizeof(float));
    m_rdftTmp[1] = m_rdftTmp[m_fftlen];
    memcpy(m_dftR, m_rdftTmp, m_fftlen * sizeof(float));

    QPainter painter(m_image);
    painter.setPen(Qt::black);  // clear prior content
    if (m_history)
    {
        painter.fillRect(s_offset,     0, 256, h, Qt::black);
        painter.fillRect(s_offset - w, 0, 256, h, Qt::black);
    } else {
        painter.fillRect(0, 0, w, h, Qt::black);
    }

    int index = 1;              // frequency index of this pixel
    int prev = 0;               // frequency index of previous pixel
    float gain = 5.0;           // compensate for window function loss
    for (int i = 1; i < (m_history ? h / 2 : w); i++)
    {                           // for each pixel of the spectrogram...
        float left = 0;
        float right = 0;
        float tmp = 0;
        int count = 0;
        for (auto j = prev + 1; j <= index; j++) // log scale!
        {    // for the freqency bins of this pixel, find peak or mean
            tmp = sq(m_dftL[2 * j]) + sq(m_dftL[(2 * j) + 1]);
            left  = m_binpeak ? std::max(tmp, left) : left + tmp;
            tmp = sq(m_dftR[2 * j]) + sq(m_dftR[(2 * j) + 1]);
            right = m_binpeak ? std::max(tmp, right) : right + tmp;
            count++;
        }
        if (!m_binpeak  && count > 0)
        {                       // mean of the frequency bins
            left /= count;
            right /= count;
        }
        // linear magnitude:           sqrt(sq(real) + sq(im));
        // left = sqrt(left);
        // right = sqrt(right);
        // power spectrum (dBm): 10 * std::log10(sq(real) + sq(im));
        left = 10 * std::log10(left);
        right = 10 * std::log10(right);

        // float bw = 1. / (16384. / 44100.);
        // float freq = bw * index;
        // LOG(VB_PLAYBACK, LOG_DEBUG, // verbose - never use in production!!!
        //     QString("SG i=%1, index=%2 (%3) %4 hz \tleft=%5,\tright=%6")
        //     .arg(i).arg(index).arg(count).arg(freq).arg(left).arg(right));

        left *= gain;
        int mag = clamp(left, 255, 0);
        int z = mag * 6;
        left > 255 ? painter.setPen(Qt::white) :
            painter.setPen(qRgb(m_red[z], m_green[z], m_blue[z]));
        if (m_history)
        {
            h = m_sgsize.height() / 2;
            painter.drawLine(s_offset,     h - i, s_offset     + mag, h - i);
            painter.drawLine(s_offset - w, h - i, s_offset - w + mag, h - i);
            if (m_color & 0x01) // left in color?
            {
                if (left > 255)
                    painter.setPen(Qt::white);
                if (mag == 0)
                    painter.setPen(Qt::black);
            } else {
                left > 255 ? painter.setPen(Qt::yellow) :
                    painter.setPen(qRgb(mag, mag, mag));
            }
            painter.drawPoint(s_offset, h - i);
        } else {
            painter.drawLine(i, h / 2, i, (h / 2) - (h / 2 * mag / 256));
        }

        right *= gain;          // copy of above, s/left/right/g
        mag = clamp(right, 255, 0);
        z = mag * 6;
        right > 255 ? painter.setPen(Qt::white) :
            painter.setPen(qRgb(m_red[z], m_green[z], m_blue[z]));
        if (m_history)
        {
            h = m_sgsize.height();
            painter.drawLine(s_offset,     h - i, s_offset     + mag, h - i);
            painter.drawLine(s_offset - w, h - i, s_offset - w + mag, h - i);
            if (m_color & 0x02) // right in color?
            {
                if (left > 255)
                    painter.setPen(Qt::white);
                if (mag == 0)
                    painter.setPen(Qt::black);
            } else {
                right > 255 ? painter.setPen(Qt::yellow) :
                    painter.setPen(qRgb(mag, mag, mag));
            }
            painter.drawPoint(s_offset, h - i);
        } else {
            painter.drawLine(i, h / 2, i, (h / 2) + (h / 2 * mag / 256));
        }

        prev = index;           // next pixel is FFT bins from here
        index = m_scale[i];     // to the next bin by LOG scale
        prev = std::min(prev, index - 1);
    }
    if (m_history && ++s_offset >= w)
    {
        s_offset = 0;
    }
    return false;
}

double Spectrogram::clamp(double cur, double max, double min)
{
    if (isnan(cur)) return 0;
    return std::clamp(cur, min, max);
}

bool Spectrogram::draw(QPainter *p, const QColor &back)
{
    p->fillRect(0, 0, 0, 0, back); // no clearing, here to suppress warning
    if (!m_image->isNull())
    {                        // background, updated by ::process above
        p->drawImage(0, 0, m_image->scaled(m_size,
                                           Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation));
    }
    // // DEBUG: see the whole signal going into the FFT:
    // if (m_size.height() > 1000) {
    //  p->setPen(Qt::yellow);
    //  float h = m_size.height() / 10;
    //  for (int j = 0; j < m_fftlen; j++) {
    //      p->drawPoint(j % m_size.width(), int(j / m_size.width()) * h + h - h * m_sigL[j]);
    //  }
    // }
    return true;
}

void Spectrogram::handleKeyPress(const QString &action)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, QString("SG keypress = %1").arg(action));

    if (action == "SELECT")
    {
        if (m_history)
        {
            m_color = (m_color + 1) & 0x03; // left and right color bits
            gCoreContext->SaveSetting("MusicSpectrogramColor",
                                      QString("%1").arg(m_color));
        }
        else
        {
            m_showtext = ! m_showtext;
        }
    }
    if (action == "2")          // 1/3 is slower/faster, 2 should be unused
    {
        m_showtext = ! m_showtext;
    }
}

// passing true to the above constructor gives the spectrum with full history:

static class SpectrogramFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "Spectrogram");
        return s_name;
    }

    uint plugins(QStringList *list) const override // VisFactory
    {
        *list << name();
        return 1;
    }

    VisualBase *create(MainVisual */*parent*/, const QString &/*pluginName*/) const override // VisFactory
    {
        return new Spectrogram(true); // history
    }
}SpectrogramFactory;

// passing false to the above constructor gives the spectrum with no history:

static class SpectrumDetailFactory : public VisFactory
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
        return new Spectrogram(false); // no history
    }
}SpectrumDetailFactory;

///////////////////////////////////////////////////////////////////////////////
// Spectrum
//

Spectrum::Spectrum()
{
    LOG(VB_GENERAL, LOG_INFO, QString("Spectrum : Being Initialised"));

    m_fps = 40;         // getting 1152 samples / 44100 = 38.28125 fps

    m_dftL = static_cast<float*>(av_malloc(sizeof(float) * m_fftlen));
    m_dftR = static_cast<float*>(av_malloc(sizeof(float) * m_fftlen));
    m_rdftTmp = static_cast<float*>(av_malloc(sizeof(float) * (m_fftlen + 2)));

    // should probably check that this succeeds
    av_tx_init(&m_rdftContext, &m_rdft, AV_TX_FLOAT_RDFT, 0, m_fftlen, &kTxScale, 0x0);
}

Spectrum::~Spectrum()
{
    av_freep(reinterpret_cast<void*>(&m_dftL));
    av_freep(reinterpret_cast<void*>(&m_dftR));
    av_freep(reinterpret_cast<void*>(&m_rdftTmp));
    av_tx_uninit(&m_rdftContext);
}

void Spectrum::resize(const QSize &newsize)
{
    // Just change internal data about the
    // size of the pixmap to be drawn (ie. the
    // size of the screen) and the logically
    // ensuing number of up/down bars to hold
    // the audio magnitudes

    m_size = newsize;

    m_analyzerBarWidth = m_size.width() / 128;

    m_analyzerBarWidth = std::max(m_analyzerBarWidth, 6);

    m_scale.setMax(m_fftlen/2, m_size.width() / m_analyzerBarWidth, 44100/2);
    m_sigL.resize(m_fftlen);
    m_sigR.resize(m_fftlen);

    m_rectsL.resize( m_scale.range() );
    m_rectsR.resize( m_scale.range() );
    int w = 0;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (uint i = 0; i < (uint)m_rectsL.size(); i++, w += m_analyzerBarWidth)
    {
        m_rectsL[i].setRect(w, m_size.height() / 2, m_analyzerBarWidth - 1, 1);
        m_rectsR[i].setRect(w, m_size.height() / 2, m_analyzerBarWidth - 1, 1);
    }

    m_magnitudes.resize( m_scale.range() * 2 );
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (uint os = m_magnitudes.size(); os < (uint)m_magnitudes.size(); os++)
    {
        m_magnitudes[os] = 0.0;
    }

    m_scaleFactor = m_size.height() / 2.0F / 42.0F;
}

// this moved up to Spectrogram so both can use it
// template<typename T> T sq(T a) { return a*a; };

bool Spectrum::process(VisualNode */*node*/)
{
    return false;
}

bool Spectrum::processUndisplayed(VisualNode *node)
{
    // copied from Spectrogram for better FFT window

    if (node)     // shift previous samples left, then append new node
    {
        // LOG(VB_PLAYBACK, LOG_DEBUG, QString("SG got %1 samples").arg(node->m_length));
        int i = std::min((int)(node->m_length), m_fftlen);
        int start = m_fftlen - i;
        float mult = 0.8F;      // decay older sound by this much
        for (int k = 0; k < start; k++)
        {                       // prior set ramps from mult to 1.0
            if (k > start - i && start > i)
            {
                mult = mult + ((1 - mult) *
                    (1 - (float)(start - k) / (float)(start - i)));
            }
            m_sigL[k] = mult * m_sigL[i + k];
            m_sigR[k] = mult * m_sigR[i + k];
        }
        for (int k = 0; k < i; k++) // append current samples
        {
            m_sigL[start + k] = node->m_left[k] / 32768.0F; // +/- 1 peak-to-peak
            if (node->m_right)
                m_sigR[start + k] = node->m_right[k] / 32768.0F;
        }
        int end = m_fftlen / 40; // ramp window ends down to zero crossing
        for (int k = 0; k < m_fftlen; k++)
        {
            if (k < end)
                mult = static_cast<float>(k) / end;
            else if (k > m_fftlen - end)
                mult = static_cast<float>(m_fftlen - k) / end;
            else
                mult = 1;
            m_dftL[k] = m_sigL[k] * mult;
            m_dftR[k] = m_sigR[k] * mult;
        }
    }
    // run the real FFT!
    m_rdft(m_rdftContext, m_rdftTmp, m_dftL, sizeof(float));
    m_rdftTmp[1] = m_rdftTmp[m_fftlen];
    memcpy(m_dftL, m_rdftTmp, m_fftlen * sizeof(float));
    m_rdft(m_rdftContext, m_rdftTmp, m_dftR, sizeof(float));
    m_rdftTmp[1] = m_rdftTmp[m_fftlen];
    memcpy(m_dftR, m_rdftTmp, m_fftlen * sizeof(float));

    long w = 0;
    QRect *rectspL = m_rectsL.data();
    QRect *rectspR = m_rectsR.data();
    float *magnitudesp = m_magnitudes.data();

    int index = 1;              // frequency index of this pixel
    int prev = 0;               // frequency index of previous pixel
    float adjHeight = m_size.height() / 2.0F;

    for (int i = 0; i < m_rectsL.size(); i++, w += m_analyzerBarWidth)
    {
        float magL = 0;         // modified from Spectrogram
        float magR = 0;
        float tmp = 0;
        for (auto j = prev + 1; j <= index; j++) // log scale!
        {    // for the freqency bins of this pixel, find peak or mean
            tmp = sq(m_dftL[2 * j]) + sq(m_dftL[(2 * j) + 1]);
            magL  = tmp > magL  ? tmp : magL;
            tmp = sq(m_dftR[2 * j]) + sq(m_dftR[(2 * j) + 1]);
            magR = tmp > magR ? tmp : magR;
        }
        magL = 10 * std::log10(magL) * m_scaleFactor;
        magR = 10 * std::log10(magR) * m_scaleFactor;

        magL = std::min(magL, adjHeight);
        if (magL < magnitudesp[i])
        {
            tmp = magnitudesp[i] - m_falloff;
            tmp = std::max(tmp, magL);
            magL = tmp;
        }
        magL = std::max<float>(magL, 1);

        magR = std::min(magR, adjHeight);
        if (magR < magnitudesp[i + m_scale.range()])
        {
            tmp = magnitudesp[i + m_scale.range()] - m_falloff;
            tmp = std::max(tmp, magR);
            magR = tmp;
        }
        magR = std::max<float>(magR, 1);

        magnitudesp[i] = magL;
        magnitudesp[i + m_scale.range()] = magR;
        rectspL[i].setTop( (m_size.height() / 2) - int( magL ) );
        rectspR[i].setBottom( (m_size.height() / 2) + int( magR ) );

        prev = index;           // next pixel is FFT bins from here
        index = m_scale[i];     // to the next bin by LOG scale
        (prev < index) || (prev = index -1);
    }

    return false;
}

double Spectrum::clamp(double cur, double max, double min)
{
    return std::clamp(cur, min, max);
}

bool Spectrum::draw(QPainter *p, const QColor &back)
{
    // This draws on a pixmap owned by MainVisual.
    //
    // In other words, this is not a Qt Widget, it
    // just uses some Qt methods to draw on a pixmap.
    // MainVisual then bitblts that onto the screen.

    QRect *rectspL = m_rectsL.data();
    QRect *rectspR = m_rectsR.data();

    p->fillRect(0, 0, m_size.width(), m_size.height(), back);
    for (uint i = 0; i < (uint)m_rectsL.size(); i++)
    {
        double per = ( rectspL[i].height() - 2. ) / (m_size.height() / 2.);

        per = clamp(per, 1.0, 0.0);

        double r = m_startColor.red() +
            ((m_targetColor.red() - m_startColor.red()) * (per * per));
        double g = m_startColor.green() +
            ((m_targetColor.green() - m_startColor.green()) * (per * per));
        double b = m_startColor.blue() +
            ((m_targetColor.blue() - m_startColor.blue()) * (per * per));

        r = clamp(r, 255.0, 0.0);
        g = clamp(g, 255.0, 0.0);
        b = clamp(b, 255.0, 0.0);

        if(rectspL[i].height() > 4)
            p->fillRect(rectspL[i], QColor(int(r), int(g), int(b)));

        per = ( rectspR[i].height() - 2. ) / (m_size.height() / 2.);

        per = clamp(per, 1.0, 0.0);

        r = m_startColor.red() +
            ((m_targetColor.red() - m_startColor.red()) * (per * per));
        g = m_startColor.green() +
            ((m_targetColor.green() - m_startColor.green()) * (per * per));
        b = m_startColor.blue() +
            ((m_targetColor.blue() - m_startColor.blue()) * (per * per));

        r = clamp(r, 255.0, 0.0);
        g = clamp(g, 255.0, 0.0);
        b = clamp(b, 255.0, 0.0);

        if(rectspR[i].height() > 4)
            p->fillRect(rectspR[i], QColor(int(r), int(g), int(b)));
    }

    return true;
}

static class SpectrumFactory : public VisFactory
{
  public:
    const QString &name(void) const override // VisFactory
    {
        static QString s_name = QCoreApplication::translate("Visualizers",
                                                            "SpectrumBars");
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
    int correction = (m_actualSize.width() % m_rectsL.size ());
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
        ((m_targetColor.red() - m_startColor.red()) * (per * per));
    double g = m_startColor.green() +
        ((m_targetColor.green() - m_startColor.green()) * (per * per));
    double b = m_startColor.blue() +
        ((m_targetColor.blue() - m_startColor.blue()) * (per * per));

    r = clamp(r, 255.0, 0.0);
    g = clamp(g, 255.0, 0.0);
    b = clamp(b, 255.0, 0.0);

    p->fillRect (x, y, w, h, QColor (int(r), int(g), int(b)));
}

bool Squares::draw(QPainter *p, const QColor &back)
{
    p->fillRect (0, 0, m_actualSize.width(), m_actualSize.height(), back);
    int w = m_actualSize.width() / (m_rectsL.size() / 2);
    int h = w;
    int center = m_actualSize.height() / 2;

    QRect *rectsp = m_rectsL.data();
    for (uint i = 0; i < (uint)m_rectsL.size() * 2; i += 2)
        drawRect(p, &(rectsp[i]), i, center, w, h);
    rectsp = m_rectsR.data();
    for (uint i = 1; i < ((uint)m_rectsR.size() * 2) + 1; i += 2)
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

    m_pianoData = (piano_key_data *) malloc(sizeof(piano_key_data) * kPianoNumKeys);
    m_audioData = (piano_audio *) malloc(sizeof(piano_audio) * kPianoAudioSize);

    double sample_rate = 44100.0;  // TODO : This should be obtained from gPlayer (likely candidate...)

    m_fps = 20; // This is the display frequency.   We're capturing all audio chunks by defining .process_undisplayed() though.

    double concert_A   =   440.0;
    double semi_tone   = pow(2.0, 1.0/12.0);

    /* Lowest note on piano is 4 octaves below concert A */
    double bottom_A = concert_A / 2.0 / 2.0 / 2.0 / 2.0;

    double current_freq = bottom_A;
    for (uint key = 0; key < kPianoNumKeys; key++)
    {
        // This is constant through time
        m_pianoData[key].coeff = (goertzel_data)(2.0 * cos(2.0 * M_PI * current_freq / sample_rate));

        // Want 20 whole cycles of the current waveform at least
        double samples_required = sample_rate/current_freq * 20.0;
        // For the really low notes, 4 updates a second is good enough...
        // For the high notes, use as many samples as we need in a display_fps
        samples_required = std::clamp(samples_required,
                                      sample_rate/(double)m_fps * 0.75,
                                      sample_rate/4.0);
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
    for (uint key = 0; key < kPianoNumKeys; key++)
    {
        // These get updated continously, and must be stored between chunks of audio data
        m_pianoData[key].q2 = 0.0F;
        m_pianoData[key].q1 = 0.0F;
        m_pianoData[key].magnitude = 0.0F;
        m_pianoData[key].max_magnitude_seen =
            (goertzel_data)(kPianoRmsNegligible * kPianoRmsNegligible); // This is a guess - will be quickly overwritten

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
    key_unit_size = std::max(key_unit_size, 10.0); // Keys have to be at least this many pixels wide

    double white_width_pct = .8;
    double black_width_pct = .6;
    double black_offset_pct = .05;

    double white_height_pct = 6;
    double black_height_pct = 4;

    // This is the starting position of the keyboard (may be beyond LHS)
    // - actually position of C below bottom A (will be added to...).  This is 4 octaves below middle C.
    double left =  ((double)m_size.width() / 2.0) - ((4.0*7.0 + 3.5) * key_unit_size); // The extra 3.5 centers 'F' inthe middle of the screen
    double top_of_keys = ((double)m_size.height() / 2.0) - (key_unit_size * white_height_pct / 2.0); // Vertically center keys

    m_rects.resize(kPianoNumKeys);

    for (uint key = 0; key < kPianoNumKeys; key++)
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
            left + (center * key_unit_size) //  Basic position of left side of key
                - (width / 2.0)  // Less half the width
                + (is_black ? (offset * black_offset_pct * key_unit_size):0.0), // And jiggle the positions of the black keys for aethetic reasons
            top_of_keys, // top
            width, // width
            height // height
        );
    }

    m_magnitude.resize(kPianoNumKeys);
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
    return (unsigned long) kPianoAudioSize;  // Maximum we can be given
}

bool Piano::processUndisplayed(VisualNode *node)
{
    //LOG(VB_GENERAL, LOG_INFO, QString("Piano : Processing undisplayed node"));
    return process_all_types(node, false);
}

bool Piano::process(VisualNode */*node*/)
{
    //LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Processing node for DISPLAY"));

    // See WaveForm::process* above
    // return process_all_types(node, true);

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

    for (uint key = 0; key < kPianoNumKeys; key++)
    {
        goertzel_data coeff = m_pianoData[key].coeff;
        goertzel_data q2 = m_pianoData[key].q2;
        goertzel_data q1 = m_pianoData[key].q1;

        for (uint i = 0; i < n; i++)
        {
            goertzel_data q0 = (coeff * q1) - q2 + m_audioData[i];
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
            goertzel_data magnitude2 = (q1*q1) + (q2*q2) - (q1*q2*coeff);

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
                magnitude_av = kPianoMinVol;
            }
            magnitude_av -= kPianoMinVol;

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
            m_pianoData[key].max_magnitude_seen =
                std::max(m_pianoData[key].max_magnitude_seen, magnitude_av);
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
    return std::clamp(cur, min, max);
}

bool Piano::draw(QPainter *p, const QColor &back)
{
    // This draws on a pixmap owned by MainVisual.
    //
    // In other words, this is not a Qt Widget, it
    // just uses some Qt methods to draw on a pixmap.
    // MainVisual then bitblts that onto the screen.

    QRect *rectsp = (m_rects).data();
    double *magnitudep = (m_magnitude).data();

    unsigned int n = kPianoNumKeys;

    p->fillRect(0, 0, m_size.width(), m_size.height(), back);

    // Protect maximum array length
    n = std::min(n, (uint)m_rects.size());

    // Sweep up across the keys, making sure the max_magnitude_seen is at minimum X% of its neighbours
    double mag = kPianoRmsNegligible;
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
        mag *= kPianoSpectrumSmoothing;
    }

    // Similarly, down, making sure the max_magnitude_seen is at minimum X% of its neighbours
    mag = kPianoRmsNegligible;
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
        mag *= kPianoSpectrumSmoothing;
    }

    // Now find the key that has been hit the hardest relative to its experience, and renormalize...
    // Set a minimum, to prevent divide-by-zero (and also all-pressed when music very quiet)
    double magnitude_max = kPianoRmsNegligible;
    for (uint key = 0; key < n; key++)
    {
        mag = m_pianoData[key].magnitude / m_pianoData[key].max_magnitude_seen;
        magnitude_max = std::max(magnitude_max, mag);

        magnitudep[key] = mag;
    }

    // Deal with all the white keys first
    for (uint key = 0; key < n; key++)
    {
        if (m_pianoData[key].is_black_note)
            continue;

        double per = magnitudep[key] / magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < kPianoKeypressTooLight)
            per = 0.0; // Clamp to zero for lightly detected keys
        LOG(VB_GENERAL, LOG_DEBUG, QString("Piano : Display key %1, magnitude=%2, seen=%3")
                .arg(key).arg(per*100.0).arg(m_pianoData[key].max_magnitude_seen));

        double r = m_whiteStartColor.red() + ((m_whiteTargetColor.red() - m_whiteStartColor.red()) * per);
        double g = m_whiteStartColor.green() + ((m_whiteTargetColor.green() - m_whiteStartColor.green()) * per);
        double b = m_whiteStartColor.blue() + ((m_whiteTargetColor.blue() - m_whiteStartColor.blue()) * per);

        p->fillRect(rectsp[key], QColor(int(r), int(g), int(b)));
    }

    // Then overlay the black keys
    for (uint key = 0; key < n; key++)
    {
        if (!m_pianoData[key].is_black_note)
            continue;

        double per = magnitudep[key]/magnitude_max;
        per = clamp(per, 1.0, 0.0);        // By construction, this should be unnecessary

        if (per < kPianoKeypressTooLight)
            per = 0.0; // Clamp to zero for lightly detected keys

        double r = m_blackStartColor.red() + ((m_blackTargetColor.red() - m_blackStartColor.red()) * per);
        double g = m_blackStartColor.green() + ((m_blackTargetColor.green() - m_blackStartColor.green()) * per);
        double b = m_blackStartColor.blue() + ((m_blackTargetColor.blue() - m_blackStartColor.blue()) * per);

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

static int nextType (int type)
{
    type++;
    return (type != IT_LAST) ? type : IT_UNKNOWN;
}

bool AlbumArt::cycleImage(void)
{
    if (!gPlayer->getCurrentMetadata())
        return false;

    AlbumArtImages *albumArt = gPlayer->getCurrentMetadata()->getAlbumArtImages();

    // If we only have one image there is nothing to cycle
    if (albumArt->getImageCount() < 2)
        return false;

    int newType = nextType(m_currImageType);
    while (!albumArt->getImage((ImageType) newType))
        newType = nextType(newType);

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
static constexpr qint64 ALBUMARTCYCLETIME { 10 };

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
            {
                art.load(imageFilename);
            }

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
