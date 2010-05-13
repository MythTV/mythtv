// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

// C
#include <cmath>
#include <cstdio>

// C++
#include <algorithm>
#include <iostream>
using namespace std;

// Qt
#include <QResizeEvent>
#include <QPaintEvent>
#include <QHideEvent>
#include <QPainter>
#include <QCursor>
#include <QPixmap>
#include <QEvent>

// mythtv
#include <audiooutput.h>
#include <mythcontext.h>
#include <mythdialogs.h>
#include <mythuihelper.h>

// mythmusic
#include "visualize.h"
#include "mainvisual.h"
#include "constants.h"

// fast inlines
#include "inlines.h"

VisFactory* VisFactory::g_pVisFactories = 0;

VisualBase::VisualBase(bool screensaverenable)
    : xscreensaverenable(screensaverenable)
{
    if (!xscreensaverenable)
        GetMythUI()->DoDisableScreensaver();
}

VisualBase::~VisualBase()
{
    //
    //    This is only here so
    //    that derived classes
    //    can destruct properly
    //
    if (!xscreensaverenable)
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

MainVisual::MainVisual(QWidget *parent, const char *name)
    : QWidget(parent), vis(0), meta(0), playing(FALSE), fps(20),
      timer (0), bannerTimer(0), info_widget(0)
{
    setObjectName(name);
    int screenwidth = 0, screenheight = 0;
    float wmult = 0, hmult = 0;

    GetMythUI()->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, parent->width(), parent->height());

    setFont(GetMythUI()->GetBigFont());
    setCursor(QCursor(Qt::BlankCursor));

    info_widget = new InfoWidget(this);

    bannerTimer = new QTimer(this);
    connect(bannerTimer, SIGNAL(timeout()), this, SLOT(bannerTimeout()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / fps);
}

MainVisual::~MainVisual()
{
    if (vis)
    {
        delete vis;
        vis = 0;
    }

    delete info_widget;
    info_widget = 0;

    delete timer;
    timer = 0;

    delete bannerTimer;
    bannerTimer = 0;

    while (!nodes.empty())
    {
        delete nodes.back();
        nodes.pop_back();
    }
}

void MainVisual::setVisual(const QString &name)
{
    pixmap.fill(Qt::black);

    QString visName, pluginName;

    if (name.contains("-"))
    {
        visName = name.section('-', 0, 0);
        pluginName = name.section('-', 1, 1);
    }
    else
    {
        visName = name;
        pluginName.clear();
    }

    if (vis)
    {
        delete vis;
        vis = NULL;
    }

    for (const VisFactory* pVisFactory = VisFactory::VisFactories();
         pVisFactory; pVisFactory = pVisFactory->next())
    {
        if (pVisFactory->name() == visName)
        {
            vis = pVisFactory->create(this, (long int) winId(), pluginName);
            vis->resize(size());
            fps = vis->getDesiredFPS();
            break;
        }
    }

    // force an update
    timer->stop();
    timer->start( 1000 / fps );
}

void MainVisual::prepare()
{
    while (!nodes.empty())
    {
        delete nodes.back();
        nodes.pop_back();
    }
}

void MainVisual::add(uchar *b, unsigned long b_len, unsigned long w, int c, int p)
{
    long len = b_len, cnt;
    short *l = 0, *r = 0;

    len /= c;
    len /= (p / 8);

    if (len > 512)
        len = 512;

    cnt = len;

    if (c == 2)
    {
        l = new short[len];
        r = new short[len];

        if (p == 8)
            stereo16_from_stereopcm8(l, r, b, cnt);
        else if (p == 16)
            stereo16_from_stereopcm16(l, r, (short *) b, cnt);
    }
    else if (c == 1)
    {
        l = new short[len];

        if (p == 8)
            mono16_from_monopcm8(l, b, cnt);
        else if (p == 16)
            mono16_from_monopcm16(l, (short *) b, cnt);
    }
    else
        len = 0;

    nodes.append(new VisualNode(l, r, len, w));
}

void MainVisual::timeout()
{
    bool process = true;
    if (parent() != GetMythMainWindow()->currentWidget())
    {
        process = false;
        return;
    }

    VisualNode *node = NULL;
    if (playing && output())
    {
        long synctime = output()->GetAudiotime();
        mutex()->lock();
        VisualNode *prev = NULL;
        while (!nodes.empty())
        {
            node = nodes.front();
            if (node->offset > synctime)
                break;
            nodes.pop_front();

            if (prev)
                delete prev;
            prev = node;
        }
        mutex()->unlock();
        node = prev;
    }

    bool stop = TRUE;
    if (vis && process)
        stop = vis->process(node);
    if (node)
        delete node;

    if (vis && process)
    {
        QPainter p(&pixmap);
        if (vis->draw(&p, Qt::black))
            update();
    }

    if (!playing && stop)
        timer->stop();
}

void MainVisual::paintEvent(QPaintEvent *)
{
    bitBlt(this, 0, 0, &pixmap);
}

void MainVisual::resizeEvent( QResizeEvent *event )
{
    pixmap.resize(event->size());
    pixmap.fill(backgroundColor());
    QWidget::resizeEvent( event );

    if ( vis )
        vis->resize( size() );

    info_widget->setDisplayRect(QRect((int)(pixmap.width() * 0.1),
                                      (int)(pixmap.height() * 0.75),
                                      (int)(pixmap.width() * 0.8),
                                      (int)(pixmap.height() * 0.18)));
}

void MainVisual::customEvent(QEvent *event)
{
    if ((event->type() == OutputEvent::Playing)   ||
        (event->type() == OutputEvent::Info)      ||
        (event->type() == OutputEvent::Buffering) ||
        (event->type() == OutputEvent::Paused))
    {
        playing = true;

        if (!timer->isActive())
            timer->start(1000 / fps);
    }
    else if ((event->type() == OutputEvent::Stopped) ||
             (event->type() == OutputEvent::Error))
    {
        playing = false;
    }
}

void MainVisual::hideEvent(QHideEvent *e)
{
    delete vis;
    vis = 0;
    emit hidingVisualization();
    QWidget::hideEvent(e);
}

void MainVisual::showBanner(const QString &text, int showTime)
{
    bannerTimer->start(showTime);
    info_widget->showInformation(text);
}

void MainVisual::showBanner(Metadata *metadata, bool fullScreen, int visMode, int showTime)
{
    bannerTimer->start(showTime);
    info_widget->showMetadata(metadata, fullScreen, visMode);
}

void MainVisual::hideBanner(void)
{
    bannerTimer->stop();
    info_widget->showInformation("");
}

void MainVisual::bannerTimeout(void)
{
    hideBanner();
}

// static member function
QStringList MainVisual::Visualizations()
{
    QStringList visualizations;
    for (const VisFactory* pVisFactory = VisFactory::VisFactories();
         pVisFactory; pVisFactory = pVisFactory->next())
    {
        pVisFactory->plugins(&visualizations);
    }

    return visualizations;
}

InfoWidget::InfoWidget(QWidget *parent)
    : QWidget( parent)
{
    hide();
}

void InfoWidget::showMetadata(Metadata *mdata, bool fullScreen, int visMode)
{
    if (!mdata)
        return;

    QString  text = "\"" + mdata->Title() + "\"\n" +  mdata->Artist() + "\n" + mdata->Album();
    QImage albumArt = mdata->getAlbumArt();

    if (text == info)
        return;

    info = text;
    if (info.isEmpty())
    {
        hide();
        return;
    }

    // only show the banner in embeded mode if asked to...
    if (visMode != 2 && !fullScreen)
    {
        hide();
        return;
    }

    // ...and only then when we have an album art image to show
    if (visMode != 2 && fullScreen && albumArt.isNull())
    {
        hide();
        return;
    }

    if (fullScreen && ! albumArt.isNull())
    {
        resize(parentWidget()->width(), parentWidget()->height());
        move(0, 0);
    }
    else
    {
        resize(displayRect.width(), displayRect.height());
        move(displayRect.x(), displayRect.y());
    }

    info_pixmap = QPixmap(width(), height());
    QPainter p(&info_pixmap);

    int indent = int(info_pixmap.width() * 0.02);

    p.setFont(GetMythUI()->GetMediumFont());

    QFontMetrics fm(p.font());
    int textWidth = fm.width(info);
    int textHeight = fm.height() * (info.contains("\n") ? 2 : 1);
    int x = indent;
    int y = indent;

    if (fullScreen && ! albumArt.isNull())
    {
        p.fillRect(0, 0, info_pixmap.width(), info_pixmap.height(), QColor ("black"));

        // draw the albumArt image
        QImage image(albumArt);
        image = image.scaled(width(), height(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage(QPoint(width() / 2 - image.width() / 2, height() / 2 - image.height() / 2), image);

        x += displayRect.x();
        y += displayRect.y();
        // only show the text box if the visualiser is actually fullscreen
        if (visMode == 2)
            p.fillRect(displayRect, QColor ("darkblue"));
    }
    else
    {
        p.fillRect(0, 0, info_pixmap.width(), info_pixmap.height(), QColor ("darkblue"));

        if (! albumArt.isNull())
        {
            // draw the albumArt image

            QImage image(albumArt);
            image = image.scaled(height(), height(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage(QPoint(0, 0), image);
            x += height();
        }
    }

    // only show the text if the visualiser is in fullscreen mode
    if (!fullScreen || visMode == 2)
    {
        QString info_copy = info;
        for (int offset = 0; offset < textHeight; offset += fm.height())
        {
            QString l = info_copy.left(info_copy.indexOf("\n"));
            p.setPen(Qt::black);
            p.drawText(x + 2, y + offset + 2, textWidth, textHeight, Qt::AlignLeft, l);
            p.setPen(Qt::white);
            p.drawText(x, y + offset, textWidth, textHeight, Qt::AlignLeft, l);
            info_copy.remove(0, l.length () + 1);
        }
    }

    show();
    repaint();
}

void InfoWidget::showInformation(const QString &text)
{
    if (text == info)
        return;

    info = text;
    if (info.isEmpty())
    {
        hide();
        return;
    }

    resize(displayRect.width(), displayRect.height());
    move(displayRect.x(), displayRect.y());

    info_pixmap = QPixmap(width(), height());
    QPainter p(&info_pixmap);

    int indent = int(info_pixmap.width() * 0.02);

    p.setFont(GetMythUI()->GetMediumFont());

    QFontMetrics fm(p.font());
    int textWidth = fm.width(info);
    int textHeight = fm.height() * (info.contains("\n") ? 2 : 1);
    int x = indent;
    int y = indent;

    p.fillRect(0, 0, info_pixmap.width(), info_pixmap.height(), QColor ("darkblue"));

    QString info_copy = info;
    for (int offset = 0; offset < textHeight; offset += fm.height())
    {
        QString l = info_copy.left(info_copy.indexOf("\n"));
        p.setPen(Qt::black);
        p.drawText(x + 2, y + offset + 2, textWidth, textHeight, Qt::AlignLeft, l);
        p.setPen(Qt::white);
        p.drawText(x, y + offset, textWidth, textHeight, Qt::AlignLeft, l);
        info_copy.remove(0, l.length () + 1);
    }

    show();
    repaint();
}

void InfoWidget::paintEvent( QPaintEvent * )
{
    bitBlt(this, 0, 0, &info_pixmap);
}

StereoScope::StereoScope()
{
    fps = 45;
    rubberband = false;
    falloff = 1.0;

    startColor = Qt::green;
    targetColor = Qt::red;
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
    bool allZero = TRUE;
    int i;
    long s, indexTo;
    double valL, valR, tmpL, tmpR;
    double index, step = 512.0 / size.width();

    if (node) {
    index = 0;
    for ( i = 0; i < size.width(); i++) {
        indexTo = (int)(index + step);
            if (indexTo == (int)(index))
                indexTo = (int)(index + 1);

        if ( rubberband ) {
        valL = magnitudes[ i ];
        valR = magnitudes[ i + size.width() ];
        if (valL < 0.) {
            valL += falloff;
            if ( valL > 0. )
            valL = 0.;
        } else {
            valL -= falloff;
            if ( valL < 0. )
            valL = 0.;
        }
        if (valR < 0.) {
            valR += falloff;
            if ( valR > 0. )
            valR = 0.;
        } else {
            valR -= falloff;
            if ( valR < 0. )
            valR = 0.;
        }
        } else
        valL = valR = 0.;

        for (s = (int)index; s < indexTo && s < node->length; s++) {
        tmpL = ( ( node->left ?
               double( node->left[s] ) : 0.) *
             double( size.height() / 4 ) ) / 32768.;
        tmpR = ( ( node->right ?
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
        allZero = FALSE;

        magnitudes[ i ] = valL;
        magnitudes[ i + size.width() ] = valR;

        index = index + step;
    }
    } else if (rubberband) {
    for ( i = 0; i < size.width(); i++) {
        valL = magnitudes[ i ];
        if (valL < 0) {
        valL += 2;
        if (valL > 0.)
            valL = 0.;
        } else {
        valL -= 2;
        if (valL < 0.)
            valL = 0.;
        }

        valR = magnitudes[ i + size.width() ];
        if (valR < 0.) {
        valR += falloff;
        if (valR > 0.)
            valR = 0.;
        } else {
        valR -= falloff;
        if (valR < 0.)
            valR = 0.;
        }

        if (valL != 0. || valR != 0.)
        allZero = FALSE;

        magnitudes[ i ] = valL;
        magnitudes[ i + size.width() ] = valR;
    }
    } else {
    for ( i = 0; (unsigned) i < magnitudes.size(); i++ )
        magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool StereoScope::draw( QPainter *p, const QColor &back )
{
    double r, g, b, per;

    p->fillRect(0, 0, size.width(), size.height(), back);
    for ( int i = 1; i < size.width(); i++ ) {
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
        p->setPen(Qt::red);
    p->drawLine( i - 1, (int)((size.height() / 4) + magnitudes[i - 1]),
             i, (int)((size.height() / 4) + magnitudes[i]));

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
        p->setPen(Qt::red);
    p->drawLine( i - 1, (int)((size.height() * 3 / 4) +
             magnitudes[i + size.width() - 1]),
             i, (int)((size.height() * 3 / 4) +
                     magnitudes[i + size.width()]));
    }

    return true;
}

MonoScope::MonoScope()
{
}

MonoScope::~MonoScope()
{
}

bool MonoScope::process( VisualNode *node )
{
    bool allZero = TRUE;
    int i;
    long s, indexTo;
    double val, tmp;

    double index, step = 512.0 / size.width();

    if (node)
    {
        index = 0;
        for ( i = 0; i < size.width(); i++)
        {
            indexTo = (int)(index + step);
            if (indexTo == (int)index)
                indexTo = (int)(index + 1);

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
            else
            {
                val = 0.;
            }

            for (s = (int)index; s < indexTo && s < node->length; s++)
            {
                tmp = ( double( node->left[s] ) +
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
                allZero = FALSE;
            }
            magnitudes[ i ] = val;
            index = index + step;
        }
    }
    else if (rubberband)
    {
        for ( i = 0; i < size.width(); i++) {
            val = magnitudes[ i ];
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
                allZero = FALSE;
            magnitudes[ i ] = val;
        }
    }
    else
    {
        for ( i = 0; i < size.width(); i++ )
            magnitudes[ i ] = 0.;
    }

    return allZero;
}

bool MonoScope::draw( QPainter *p, const QColor &back )
{
    double r, g, b, per;

    p->fillRect( 0, 0, size.width(), size.height(), back );
    for ( int i = 1; i < size.width(); i++ ) {
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

        p->setPen(Qt::red);
        //p->setPen(QColor(int(r), int(g), int(b)));
        p->drawLine( i - 1, (int)(size.height() / 2 + magnitudes[ i - 1 ]),
                     i, (int)(size.height() / 2 + magnitudes[ i ] ));
    }

    return true;
}

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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)winid;
        (void)pluginName;
        return new StereoScope();
    }
}StereoScopeFactory;


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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)winid;
        (void)pluginName;
        return new MonoScope();
    }
}MonoScopeFactory;

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

