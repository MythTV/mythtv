/*
    visualize.h 

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Closely based on code from mq3 by Brad Hughes
    
    Part of the mythTV project
    
    music visualizers
*/

#ifndef VISUALIZE_H
#define VISUALIZE_H

// C++ headers
#include <QVector>
#include <vector>

// MythTV headers
#include <visual.h>
#include <musicmetadata.h>
#include <mythbaseexp.h>

// MythMusic headers
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
#endif
#endif
}

#define SAMPLES_DEFAULT_SIZE 512

class MainVisual;

class VisualNode
{
  public:
    VisualNode(short *l, short *r, unsigned long n, unsigned long o)
        : left(l), right(r), length(n), offset(o)
    {
        // left and right are allocated and then passed to this class
        // the code that allocated left and right should give up all ownership
    }

    ~VisualNode()
    {
        delete [] left;
        delete [] right;
    }

    short *left, *right;
    unsigned long length, offset;
};

class VisualBase
{
  public:
    VisualBase(bool screensaverenable = false);
    virtual ~VisualBase(void);

    // return true if the output should stop
    virtual bool process( VisualNode *node ) = 0;

    // this is called on nodes that will not be displayed :: Not needed for most visualizations
    // (i.e. between the displayed frames, if you need the whole audio stream)
    virtual bool processUndisplayed( VisualNode * )
    {
        return true; // By default this does nothing : Ignore the in-between chunks of audio data
    };

    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;
    virtual void handleKeyPress(const QString &action) = 0;
    virtual int getDesiredFPS(void) { return m_fps; }
    // Override this if you need the potential of capturing more data than the default
    virtual unsigned long getDesiredSamples(void) { return SAMPLES_DEFAULT_SIZE; }
    void drawWarning(QPainter *p, const QColor &back, const QSize &color, QString warning, int fontsize = 28);

  protected:
    int m_fps;
    bool m_xscreensaverenable;
};

class VisFactory
{
  public:
    VisFactory() {m_pNextVisFactory = g_pVisFactories; g_pVisFactories = this;}
    virtual ~VisFactory() {}
    const VisFactory* next() const {return m_pNextVisFactory;}
    virtual const QString &name(void) const = 0;
    virtual VisualBase* create(MainVisual *parent, const QString &pluginName) const = 0;
    virtual uint plugins(QStringList *list) const = 0;
    static const VisFactory* VisFactories() {return g_pVisFactories;}
  protected:
    static VisFactory* g_pVisFactories;
    VisFactory*        m_pNextVisFactory;
};

class StereoScope : public VisualBase
{
  public:
    StereoScope();
    virtual ~StereoScope();

    void resize( const QSize &size );
    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );
    void handleKeyPress(const QString &action) {(void) action;}

  protected:
    QColor startColor, targetColor;
    vector<double> magnitudes;
    QSize size;
    bool const rubberband;
    double const falloff;
};

class MonoScope : public StereoScope
{
  public:
    MonoScope();
    virtual ~MonoScope();

    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );
};

class LogScale
{
  public:
    LogScale(int = 0, int = 0);
    ~LogScale();

    int scale() const { return s; }
    int range() const { return r; }

    void setMax(int, int);

    int operator[](int);


  private:
    int *indices;
    int s, r;
};

#ifdef FFTW3_SUPPORT
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
    void handleKeyPress(const QString &action) {(void) action;}

  protected:
    inline double clamp(double cur, double max, double min);

    QColor startColor, targetColor;
    QVector<QRect> rects;
    QVector<double> magnitudes;
    QSize size;
    LogScale scale;
    double scaleFactor, falloff;
    int analyzerBarWidth;

    fftw_plan lplan, rplan;
    myth_fftw_float *lin, *rin;
    myth_fftw_complex *lout, *rout;
};

class Squares : public Spectrum
{
  public:
    Squares();
    virtual ~Squares();

    void resize (const QSize &newsize);
    bool draw(QPainter *p, const QColor &back = Qt::black);
    void handleKeyPress(const QString &action) {(void) action;}

  private:
    void drawRect(QPainter *p, QRect *rect, int i, int c, int w, int h);
    QSize size;
    MainVisual *pParent;
    int fake_height;
    int number_of_squares;
};

#endif // FFTW3_SUPPORT

class Piano : public VisualBase
{
    // This class draws bars (up and down)
    // based on the magnitudes at piano pitch
    // frequencies in the audio data.

#define PIANO_AUDIO_SIZE 4096
#define PIANO_N 88

#define piano_audio float
#define goertzel_data float

#define PIANO_RMS_NEGLIGIBLE .001
#define PIANO_SPECTRUM_SMOOTHING 0.95
#define PIANO_MIN_VOL -10
#define PIANO_KEYPRESS_TOO_LIGHT .2

typedef struct piano_key_data {
    goertzel_data q1, q2, coeff, magnitude;
    goertzel_data max_magnitude_seen;

    // This keeps track of the samples processed for each note
    // Low notes require a lot of samples to be correctly identified
    // Higher ones are displayed quicker
    int samples_processed;
    int samples_process_before_display_update;

    bool is_black_note; // These are painted on top of white notes, and have different colouring
} piano_key_data;

  public:
    Piano();
    virtual ~Piano();

    virtual void resize(const QSize &size);

    bool process(VisualNode *node);

    // These functions are new, since we need to inspect all the data
    bool processUndisplayed(VisualNode *node);
    unsigned long getDesiredSamples(void);

    virtual bool draw(QPainter *p, const QColor &back = Qt::black);
    void handleKeyPress(const QString &action) {(void) action;}

  protected:
    inline double clamp(double cur, double max, double min);
    bool process_all_types(VisualNode *node, bool this_will_be_displayed);
    void zero_analysis(void);

    QColor whiteStartColor, whiteTargetColor, blackStartColor, blackTargetColor;

    vector<QRect> rects;
    QSize size;

    unsigned long offset_processed;

    piano_key_data *piano_data;
    piano_audio *audio_data;

    vector<double> magnitude;
};

class AlbumArt : public VisualBase
{
  public:
    AlbumArt(void);
    virtual ~AlbumArt();

    void resize(const QSize &size);
    bool process(VisualNode *node = 0);
    bool draw(QPainter *p, const QColor &back = Qt::black);
    void handleKeyPress(const QString &action);

  private:
    bool needsUpdate(void);
    void findFrontCover(void);
    bool cycleImage(void);

    QSize m_size, m_cursize;
    ImageType m_currImageType;
    QImage m_image;

    MusicMetadata *m_currentMetadata;
    QDateTime m_lastCycle;
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
    void handleKeyPress(const QString &action) {(void) action;}

  private:
    QSize size;
};

#endif // __visualize_h
