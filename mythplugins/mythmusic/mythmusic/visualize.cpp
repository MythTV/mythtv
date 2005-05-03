/*
        visualize.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
        VERY closely based on code from mq3 by Brad Hughes
                
        Part of the mythTV project
        
        music visualizers
*/

#include "mainvisual.h"
#include "visualize.h"
#include "math.h"
#include "inlines.h"
#include "decoder.h"

#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qdir.h>

#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>

Spectrum::Spectrum()
{
    // Setup the "magical" audio data transformations
    // provided by the Fast Fourier Transforms library
    analyzerBarWidth = 6;
    scaleFactor = 2.0;
    falloff = 3.0;
    fps = 20;
        
#ifdef FFTW_SUPPORT
    plan =  rfftw_create_plan(512, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif
    startColor = QColor(0,0,255);
    targetColor = QColor(255,0,0); 
}

Spectrum::~Spectrum()
{
#ifdef FFTW_SUPPORT
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
    int i = 0, w = 0;
    for (; (unsigned) i < rects.count(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; (unsigned) os < magnitudes.size(); os++)
    {
        magnitudes[os] = 0.0;
    }

    scaleFactor = double( size.height() / 2 ) / log( 512.0 );
}


bool Spectrum::process(VisualNode *node)
{
    // Take a bunch of data in *node
    // and break it down into spectrum
    // values
    bool allZero = TRUE;
#ifdef FFTW_SUPPORT        // Don't do any real processing if libfftw isn't available
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

    fast_reals_set(lin + i, rin + i, 0, 512 - i);

    rfftw_one(plan, lin, lout);
    rfftw_one(plan, rin, rout);

    index = 1;
    for (i = 0; i < rects.count(); i++, w += analyzerBarWidth)
    {
        magL = (log(lout[index] * lout[index] + 
                    lout[512 - index] * lout[512 - index]) - 22.0) * 
               scaleFactor;
        magR = (log(rout[index] * rout[index] + 
                    rout[512 - index] * rout[512 - index]) - 22.0) * 
               scaleFactor;

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

#ifdef FFTW_SUPPORT
    QRect *rectsp = rects.data();
    double r, g, b, per;

    p->fillRect(0, 0, size.width(), size.height(), back);
    for (uint i = 0; i < rects.count(); i++)
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
    p->fillRect(0, 0, size.width(), size.height(), back);
    p->setPen(Qt::white);
    p->setFont(gContext->GetMediumFont());
    p->drawText(size.width() / 2 - 200, size.height() / 2 - 20, 400, 20, 
                Qt::AlignCenter, QObject::tr("Visualization requires FFT library"));
    p->drawText(size.width() / 2 - 200, size.height() / 2, 400, 20, 
                Qt::AlignCenter, QObject::tr("Did you run configure?"));
#endif
    
    return true;
}

const QString &SpectrumFactory::name(void) const
{
    static QString name("Spectrum");
    return name;
}

const QString &SpectrumFactory::description(void) const
{
    static QString name(QObject::tr("Spectrum"));
    return name;
}

VisualBase *SpectrumFactory::create(MainVisual *parent, long int winid)
{
    (void)parent;
    (void)winid;
    return new Spectrum();
}

AlbumArt::AlbumArt(MainVisual *parent)
{
    pParent = parent;
    Decoder *dec = pParent->decoder();
    if (dec)
    {
        filename = dec->getFilename();
        directory = filename.left(filename.findRev("/"));
    }

    fps = 20;
}

AlbumArt::~AlbumArt()
{
}

void AlbumArt::resize(const QSize &newsize)
{
    size = newsize;
}

bool AlbumArt::process(VisualNode *node)
{
    node = node;
    return true;
}

bool AlbumArt::draw(QPainter *p, const QColor &back)
{
    if (!pParent->decoder())
        return false;

    QString curfile = pParent->decoder()->getFilename();
    QString curdir = curfile.left(curfile.findRev("/"));
    QImage art;
 
    // If the directory has changed (new album) or the size, reload
    if ((directory.compare(curdir) != 0) || (cursize != size))
    {
        // Directory has changed
        directory = curdir;
        // Get filter
        QString namefilter = gContext->GetSetting("AlbumArtFilter",
                                              "*.png;*.jpg;*.jpeg;*.gif;*.bmp");
        // Get directory contents based on filter
        QDir folder(curdir, namefilter, QDir::Name | QDir::IgnoreCase, 
                    QDir::Files | QDir::Hidden);

        QString fileart = "";

        if (folder.count())
            fileart = folder[rand() % folder.count()];

        curdir.append("/");
        curdir.append(fileart);
        art.load(curdir);
        if (art.isNull())
        {
            p->fillRect(0, 0, size.width(), size.height(), back);
            p->setPen(Qt::white);
            p->setFont(gContext->GetMediumFont());
            p->drawText(size.width() / 2 - 200, size.height() / 2 - 10, 400, 20, 
                        Qt::AlignCenter, QObject::tr("?"));
            return true;
        }

        QSize artsize = art.scale(size, QImage::ScaleMin).size();

        // Paint the image
        p->fillRect(0, 0, size.width(), size.height(), back);
        p->drawPixmap((size.width() - artsize.width()) / 2,
                      (size.height() - artsize.height()) / 2,
                      art.smoothScale(size, QImage::ScaleMin));
        // Store our new size
        cursize = size;
        return true;
    }
    art.reset();
    return true;
}

const QString &AlbumArtFactory::name(void) const
{
    static QString name("AlbumArt");
    return name;
}

const QString &AlbumArtFactory::description(void) const
{
    static QString name("Displays album art from .folder.png during playback");
    return name;
}

VisualBase *AlbumArtFactory::create(MainVisual *parent, long int winid)
{
    (void)winid;
    return new AlbumArt(parent);
}


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

const QString &BlankFactory::name(void) const
{
    static QString name("Blank");
    return name;
}

const QString &BlankFactory::description(void) const
{
    static QString name(QObject::tr("Blank"));
    return name;
}

VisualBase *BlankFactory::create(MainVisual *parent, long int winid)
{
    (void)parent;
    (void)winid;
    return new Blank();
}


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

Gears::Gears(QWidget *parent, const char *name)
     : QGLWidget(parent, name)
{
    falloff = 4.0;
    analyzerBarWidth = 10;
    fps = 60;

    // Slightly trick bit: This *is* a Qt Qidget 
    // (unlike spectrum, above) so we just use
    // the Qt GL class.

    //int screenwidth = 0, screenheight = 0;
    //float wmult = 0.0, hmult = 0.0;

    //gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    setGeometry(0, 0, parent->width(), parent->height());
    //setGeometry(0, 0, 400, 300);
    //setFixedSize(QSize(screenwidth, screenheight));

    angle = 0.0;
    view_roty = 30.0;

#ifdef FFTW_SUPPORT
    plan =  rfftw_create_plan(512, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif
    startColor = QColor(0,0,255);
    targetColor = QColor(255,0,0); 

    show();
}

Gears::~Gears()
{
#ifdef FFTW_SUPPORT
    rfftw_destroy_plan(plan);
#endif
}

void Gears::resize(const QSize &newsize)
{
    //cout << "Gears is resizing to width of " << newsize.width() << endl;
    size = newsize;
    scale.setMax(192, size.width() / analyzerBarWidth);

    rects.resize( scale.range() );
    int i = 0, w = 0;
    for (; (unsigned) i < rects.count(); i++, w += analyzerBarWidth)
    {
        rects[i].setRect(w, size.height() / 2, analyzerBarWidth - 1, 1);
    }

    int os = magnitudes.size();
    magnitudes.resize( scale.range() * 2 );
    for (; (unsigned) os < magnitudes.size(); os++)
    {
        magnitudes[os] = 0.0;
    }

    scaleFactor = double( size.height() / 2 ) / log( 512.0 );
    //resizeGL(newsize.width(), newsize.height());
    setGeometry(0, 0, newsize.width(), newsize.height());
}


bool Gears::process(VisualNode *node)
{
    bool allZero = TRUE;
#ifdef FFTW_SUPPORT
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

    fast_reals_set(lin + i, rin + i, 0, 512 - i);

    rfftw_one(plan, lin, lout);
    rfftw_one(plan, rin, rout);

    index = 1;
    for (i = 0; i < rects.count(); i++, w += analyzerBarWidth)
    {
        magL = (log(lout[index] * lout[index] + lout[512 - index] * lout[512 - index]) - 22.0) * scaleFactor;
        magR = (log(rout[index] * rout[index] + rout[512 - index] * rout[512 - index]) - 22.0) * scaleFactor;

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

const QString &GearsFactory::name(void) const
{
    static QString name("Gears");
    return name;
}

const QString &GearsFactory::description(void) const
{
    static QString name(QObject::tr("Gears"));
    return name;
}

VisualBase *GearsFactory::create(MainVisual *parent, long int winid)
{
    (void)winid;
    return new Gears(parent);
}

#endif
