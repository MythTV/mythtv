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
#include <vector>

// Qt headers
#include <QCoreApplication>
#include <QVector>

// MythTV headers
#include <libmyth/visual.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythbase/mythbaseexp.h>

// MythMusic headers
#include "constants.h"

#include <complex>
extern "C" {
    #include <libavutil/mem.h>
    #include <libavcodec/avfft.h>
}

static constexpr uint16_t SAMPLES_DEFAULT_SIZE { 512 };

class MainVisual;

class VisualNode
{
  public:
    VisualNode(short *l, short *r, unsigned long n, std::chrono::milliseconds timecode)
        : m_left(l), m_right(r), m_length(n), m_offset(timecode)
    {
        // left and right are allocated and then passed to this class
        // the code that allocated left and right should give up all ownership
    }

    ~VisualNode()
    {
        delete [] m_left;
        delete [] m_right;
    }

    short *m_left  {nullptr};
    short *m_right {nullptr};
    unsigned long m_length;
    std::chrono::milliseconds m_offset;
};

class VisualBase
{
  public:
    explicit VisualBase(bool screensaverenable = false);
    virtual ~VisualBase(void);

    // return true if the output should stop
    virtual bool process( VisualNode *node ) = 0;

    // this is called on nodes that will not be displayed :: Not needed for most visualizations
    // (i.e. between the displayed frames, if you need the whole audio stream)
    virtual bool processUndisplayed( VisualNode */*node*/ )
    {
        return true; // By default this does nothing : Ignore the in-between chunks of audio data
    };

    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;
    virtual void handleKeyPress(const QString &action) = 0;
    virtual int getDesiredFPS(void) { return m_fps; }
    // Override this if you need the potential of capturing more data than the default
    virtual unsigned long getDesiredSamples(void) { return SAMPLES_DEFAULT_SIZE; }
    static void drawWarning(QPainter *p, const QColor &back, QSize size, const QString& warning, int fontsize = 28);

  protected:
    int  m_fps                {20};
    bool m_xscreensaverenable {true};
};

class VisFactory
{
  public:
    VisFactory() {m_pNextVisFactory = g_pVisFactories; g_pVisFactories = this;}
    virtual ~VisFactory() = default;
    const VisFactory* next() const {return m_pNextVisFactory;}
    virtual const QString &name(void) const = 0;
    virtual VisualBase* create(MainVisual *parent, const QString &pluginName) const = 0;
    virtual uint plugins(QStringList *list) const = 0;
    static const VisFactory* VisFactories() {return g_pVisFactories;}
  protected:
    static VisFactory* g_pVisFactories;
    VisFactory*        m_pNextVisFactory {nullptr};
};


#define RUBBERBAND 0 // NOLINT(cppcoreguidelines-macro-usage)
#define TWOCOLOUR 1 // NOLINT(cppcoreguidelines-macro-usage)

class StereoScope : public VisualBase
{
  public:
    StereoScope();
    ~StereoScope() override = default;

    void resize( const QSize &size ) override; // VisualBase
    bool process( VisualNode *node ) override; // VisualBase
    bool draw( QPainter *p, const QColor &back ) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

  protected:
    QColor         m_startColor  {Qt::yellow};
    QColor         m_targetColor {Qt::red};
    std::vector<double> m_magnitudes  {};
    QSize          m_size;
    bool const     m_rubberband  {RUBBERBAND};
    double const   m_falloff     {1.0};
};

class MonoScope : public StereoScope
{
  public:
    MonoScope() = default;
    ~MonoScope() override = default;

    bool process( VisualNode *node ) override; // StereoScope
    bool draw( QPainter *p, const QColor &back ) override; // StereoScope
};

// WaveForm - see whole track - by twitham@sbcglobal.net, 2023/01

#define WF_AUDIO_SIZE 4096     // maximum samples to process at a time
#define WF_WIDTH 1920   // image cache size, will scale to any display
#define WF_HEIGHT 1080

class WaveForm : public MonoScope
{
public:
    WaveForm() = default;
    ~WaveForm() override;

    unsigned long getDesiredSamples(void) override;
    bool processUndisplayed(VisualNode *node) override;
    bool process( VisualNode *node ) override;
    bool draw( QPainter *p, const QColor &back ) override;
    void handleKeyPress(const QString &action) override;

protected:
    bool process_all_types(VisualNode *node, bool displayed);
    void saveload(MusicMetadata *meta);
    unsigned long m_offset {0}; // pass from process to draw
    short         *m_right {nullptr};
    QFont         m_font;       // optional text overlay
    bool          m_showtext {1};
    QImage        m_image;      // picture of full track
    MusicMetadata *m_currentMetadata {nullptr};
    unsigned long m_duration {60000};
    unsigned int  m_lastx    {0}; // pixel tracker
    unsigned int  m_position {0}; // location inside pixel
    short int     m_minl {0};     // left range
    short int     m_maxl {0};
    unsigned long m_sqrl {0};   // sum of squares, for RMS
    short int     m_minr {0};   // right range
    short int     m_maxr {0};
    unsigned long m_sqrr {0};
};

class LogScale
{
  public:
    explicit LogScale(int maxscale = 0, int maxrange = 0);
    ~LogScale();

    int scale() const { return m_s; }
    int range() const { return m_r; }

    void setMax(int maxscale, int maxrange);

    int operator[](int index);


  private:
    int *m_indices {nullptr};
    int  m_s       {0};
    int  m_r       {0};
};

class Spectrum : public VisualBase
{
    // This class draws bars (up and down)
    // based on the magnitudes at various
    // frequencies in the audio data.
    
  public:
    Spectrum();
    ~Spectrum() override;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

  protected:
    static inline double clamp(double cur, double max, double min);

    QColor             m_startColor       {Qt::blue};
    QColor             m_targetColor      {Qt::red};
    QVector<QRect>     m_rects;
    QVector<double>    m_magnitudes;
    QSize              m_size;
    LogScale           m_scale;

    // Setup the "magical" audio data transformations
    // provided by the Fast Fourier Transforms library
    double             m_scaleFactor      {2.0};
    double             m_falloff          {10.0};
    int                m_analyzerBarWidth {6};

    FFTComplex*        m_dftL              { nullptr };
    FFTComplex*        m_dftR              { nullptr };
    FFTContext*        m_fftContextForward { nullptr };
};

class Squares : public Spectrum
{
  public:
    Squares();
    ~Squares() override = default;

    void resize (const QSize &newsize) override; // Spectrum
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // Spectrum
    void handleKeyPress(const QString &action) override // Spectrum
        {(void) action;}

  private:
    void drawRect(QPainter *p, QRect *rect, int i, int c, int w, int h);
    QSize m_actualSize        {0,0};
    int   m_fakeHeight        {0};
    int   m_numberOfSquares   {16};
};

class Piano : public VisualBase
{
    // This class draws bars (up and down)
    // based on the magnitudes at piano pitch
    // frequencies in the audio data.

    static constexpr unsigned long kPianoAudioSize { 4096 };
    static constexpr unsigned int  kPianoNumKeys   { 88   };

#define piano_audio float
#define goertzel_data float

    static constexpr double        kPianoRmsNegligible     { .001 };
    static constexpr double        kPianoSpectrumSmoothing { 0.95 };
    static constexpr goertzel_data kPianoMinVol            { -10  };
    static constexpr double        kPianoKeypressTooLight  { .2   };

struct piano_key_data {
    goertzel_data q1, q2, coeff, magnitude;
    goertzel_data max_magnitude_seen;

    // This keeps track of the samples processed for each note
    // Low notes require a lot of samples to be correctly identified
    // Higher ones are displayed quicker
    int samples_processed;
    int samples_process_before_display_update;

    bool is_black_note; // These are painted on top of white notes, and have different colouring
};

  public:
    Piano();
    ~Piano() override;

    void resize(const QSize &size) override; // VisualBase

    bool process(VisualNode *node) override; // VisualBase

    // These functions are new, since we need to inspect all the data
    bool processUndisplayed(VisualNode *node) override; // VisualBase
    unsigned long getDesiredSamples(void) override; // VisualBase

    bool draw(QPainter *p, const QColor &back = Qt::black) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

  protected:
    static inline double clamp(double cur, double max, double min);
    bool process_all_types(VisualNode *node, bool this_will_be_displayed);
    void zero_analysis(void);

    QColor          m_whiteStartColor  {245,245,245};
    QColor          m_whiteTargetColor {Qt::red};
    QColor          m_blackStartColor  {10,10,10};
    QColor          m_blackTargetColor {Qt::red};

    std::vector<QRect> m_rects         {};
    QSize           m_size;

    std::chrono::milliseconds m_offsetProcessed  {0ms};

    piano_key_data *m_pianoData        {nullptr};
    piano_audio    *m_audioData        {nullptr};

    std::vector<double> m_magnitude    {};
};

class AlbumArt : public VisualBase
{
    Q_DECLARE_TR_FUNCTIONS(AlbumArt);

  public:
    AlbumArt(void);
    ~AlbumArt() override = default;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node = nullptr) override; // VisualBase
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // VisualBase
    void handleKeyPress(const QString &action) override; // VisualBase

  private:
    bool needsUpdate(void);
    void findFrontCover(void);
    bool cycleImage(void);

    QSize m_size;
    QSize m_cursize;
    ImageType m_currImageType {IT_UNKNOWN};
    QImage m_image;

    MusicMetadata *m_currentMetadata {nullptr};
    QDateTime m_lastCycle;
};

class Blank : public VisualBase
{
    // This draws ... well ... nothing    
  public:
    Blank();
    ~Blank() override = default;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node = nullptr) override; // VisualBase
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // VisualBase
    void handleKeyPress(const QString &action) override // VisualBase
        {(void) action;}

  private:
    QSize m_size;
};

#endif // __visualize_h
