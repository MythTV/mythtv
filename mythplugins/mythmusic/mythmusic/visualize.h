/*
	visualize.h 

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Closely based on code from mq3 by Brad Hughes
	
	Part of the mythTV project
	
	music visualizers
*/

#ifndef VISUALIZE_H
#define VISUALIZE_H

#include <mythtv/visual.h>
#include "mainvisual.h"
#include "constants.h"
#include "config.h"

#include <complex>
extern "C" {
#ifdef FFTW3_SUPPORT
#include <fftw3.h>
#define myth_fftw_float double /* need to use different plan function to change */
#define fftw_real myth_fftw_float
#define myth_fftw_complex std::complex<myth_fftw_float>
#if (myth_fftw_float == double)
#define myth_fftw_complex_cast fftw_complex
#elif (myth_fftw_float == float)
#define myth_fftw_complex_cast fftwf_complex
#elif (myth_fftw_float == long double)
#define myth_fftw_complex_cast fftwl_complex
#endif
#elif	FFTW2_SUPPORT
#include <rfftw.h>
#include <fftw.h>
#endif		
}

#ifdef OPENGL_SUPPORT
#include <qgl.h>
#endif

class Spectrum : public VisualBase
{
    // This class draws bars (up and down)
    // based on the magnitudes at various
    // frequencies in the audio data.
	
  public:
    Spectrum();
    virtual ~Spectrum();

    virtual void resize(const QSize &size);
    bool process(VisualNode *node);
    virtual bool draw(QPainter *p, const QColor &back = Qt::black);

  protected:
    inline double clamp(double cur, double max, double min);

    QColor startColor, targetColor;
    QMemArray<QRect> rects;
    QMemArray<double> magnitudes;
    QSize size;
    LogScale scale;
    double scaleFactor, falloff;
    int analyzerBarWidth;

#ifdef FFTW3_SUPPORT
    fftw_plan lplan, rplan;
    myth_fftw_float *lin, *rin;
    myth_fftw_complex *lout, *rout;
#elif FFTW2_SUPPORT
    rfftw_plan plan;
    fftw_real *lin, *rin, *lout, *rout;
#endif
};

class SpectrumFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

class AlbumArt : public VisualBase
{
  public:
    AlbumArt(MainVisual *parent);
    virtual ~AlbumArt();

    void resize(const QSize &size);
    bool process(VisualNode *node = 0);
    bool draw(QPainter *p, const QColor &back = Qt::black);
    bool needsUpdate();
    QString getImageFilename();

  private:
    QSize size, cursize;
    QString filename;
    QString directory;
    MainVisual *pParent;
    QImage image;
};

class AlbumArtFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

class Blank : public VisualBase
{
    // This draws ... well ... nothing	
  public:
    Blank();
    virtual ~Blank();

    void resize(const QSize &size);
    bool process(VisualNode *node = 0);
    bool draw(QPainter *p, const QColor &back = Qt::black);

  private:
    QSize size;
};

class BlankFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

class Squares : public Spectrum
{
  public:
    Squares();
    virtual ~Squares();
    
    void resize (const QSize &newsize);
    bool draw(QPainter *p, const QColor &back = Qt::black);

  private:
    void drawRect(QPainter *p, QRect *rect, int i, int c, int w, int h);
    QSize size;
    MainVisual *pParent;
    int fake_height;
    int number_of_squares;
};

class SquaresFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

#ifdef OPENGL_SUPPORT

class Gears : public QGLWidget, public VisualBase
{
    // Draws some OpenGL gears and manipulates
    // them based on audio data
  public:
    Gears(QWidget *parent = 0, const char * = 0);
    virtual ~Gears();

    void resize(const QSize &size);
    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);

  protected:
    void initializeGL();
    void resizeGL( int, int );
    void paintGL();
    void drawTheGears();
		
  private:
    QColor startColor, targetColor;
    QMemArray<QRect> rects;
    QMemArray<double> magnitudes;
    QSize size;
    LogScale scale;
    double scaleFactor, falloff;
    int analyzerBarWidth;
    GLfloat angle, view_roty;

#ifdef FFTW3_SUPPORT
    fftw_plan lplan, rplan;
    myth_fftw_float *lin, *rin;
    myth_fftw_complex *lout, *rout;
#elif FFTW2_SUPPORT
    rfftw_plan plan;
    fftw_real *lin, *rin, *lout, *rout;
#endif
};

class GearsFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};


#endif // opengl_support	

#endif // __visualize_h
