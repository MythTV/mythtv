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

// mythmusic
#include "mainvisual.h"
#include "visualize.h"
#include "inlines.h"
#include "decoder.h"
#include "metadata.h"
#include "musicplayer.h"


#define FFTW_N 512
extern "C" {
void *av_malloc(unsigned int size);
void av_free(void *ptr);
}

Spectrum::Spectrum()
#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    : lin(NULL), rin(NULL), lout(NULL), rout(NULL)
#endif
{
    // Setup the "magical" audio data transformations
    // provided by the Fast Fourier Transforms library
    analyzerBarWidth = 6;
    scaleFactor = 2.0;
    falloff = 3.0;
    fps = 20;
        
#ifdef FFTW3_SUPPORT
    lin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    rin = (myth_fftw_float*) av_malloc(sizeof(myth_fftw_float)*FFTW_N);
    lout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));
    rout = (myth_fftw_complex*)
        av_malloc(sizeof(myth_fftw_complex)*(FFTW_N/2+1));

    lplan = fftw_plan_dft_r2c_1d(FFTW_N, lin, (myth_fftw_complex_cast*)lout, FFTW_MEASURE);
    rplan = fftw_plan_dft_r2c_1d(FFTW_N, rin, (myth_fftw_complex_cast*)rout, FFTW_MEASURE);
#elif FFTW2_SUPPORT
    lin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    rin = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N);
    lout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);
    rout = (fftw_real*) av_malloc(sizeof(fftw_real)*FFTW_N*2);

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

    scale.setMax(192, size.width() / analyzerBarWidth);

    rects.resize( scale.range() );
    unsigned int i = 0;
    int w = 0;
    for (; i < rects.size(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    unsigned int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; os < magnitudes.size(); os++)
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
    for (i = 0; i < rects.size(); i++, w += analyzerBarWidth)
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
            allZero = FALSE;
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
    for (uint i = 0; i < rects.size(); i++)
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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)winid;
        (void)pluginName;
        return new Spectrum();
    }
}SpectrumFactory;

AlbumArt::AlbumArt(void)
{
    findFrontCover();

    if (gPlayer->getDecoder())
        m_filename = gPlayer->getDecoder()->getFilename();

    fps = 1;
}

void AlbumArt::findFrontCover(void)
{
    // if a front cover image is available show that first
    AlbumArtImages albumArt(gPlayer->getCurrentMetadata());
    if (albumArt.getImage(IT_FRONTCOVER))
        m_currImageType = IT_FRONTCOVER;
    else
    {
        // not available so just show the first image available
        if (albumArt.getImageCount() > 0)
            m_currImageType = albumArt.getImageAt(0)->imageType;
        else
            m_currImageType = IT_UNKNOWN;
    }
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
    node = node;
    return true;
}

void AlbumArt::handleKeyPress(const QString &action)
{
    if (action == "SELECT")
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

bool AlbumArt::needsUpdate() 
{
    if (m_cursize != m_size)
        return true;

    if (m_filename != gPlayer->getDecoder()->getFilename()) 
    {
        m_filename = gPlayer->getDecoder()->getFilename();
        findFrontCover();
        return true;
    }

    return false;
}

bool AlbumArt::draw(QPainter *p, const QColor &back)
{
    if (!gPlayer->getDecoder())
        return false;

    // If the directory has changed (new album) or the size, reload
    if (needsUpdate())
    {
        QImage art(gPlayer->getCurrentMetadata()->getAlbumArt(m_currImageType));
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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void) parent;
        (void)winid;
        (void)pluginName;
        return new AlbumArt();
    }
}AlbumArtFactory;

Blank::Blank()
    : VisualBase(true)
{
    fps = 20;
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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)winid;
        (void)pluginName;
        return new Blank();
    }
}BlankFactory;

Squares::Squares()
{
    number_of_squares = 16;
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

#if defined(FFTW3_SUPPORT) || defined(FFTW2_SUPPORT)
    QRect *rectsp = rects.data();
    for (uint i = 0; i < rects.size(); i++)
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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)parent;
        (void)winid;
        (void)pluginName;
        return new Squares();
    }
}SquaresFactory;

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
    for (; (unsigned) i < rects.size(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; (unsigned) os < magnitudes.size(); os++)
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
    for (i = 0; i < rects.size(); i++, w += analyzerBarWidth)
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
            allZero = FALSE;
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

    VisualBase *create(MainVisual *parent, long int winid, const QString &pluginName) const
    {
        (void)winid;
        (void)pluginName;
        return new Gears(parent);
    }
}GearsFactory;


#endif
