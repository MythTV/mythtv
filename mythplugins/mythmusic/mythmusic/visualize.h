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
    #include <libavutil/tx.h>
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
    // as of v33, this processes *all* samples, see the comments in WaveForm
    virtual bool processUndisplayed( VisualNode */*node*/ )
    {
        return true; // By default this does nothing : Ignore the in-between chunks of audio data
    };

    virtual bool draw( QPainter *, const QColor & ) = 0;
    virtual void resize( const QSize &size ) = 0;
    virtual void handleKeyPress([[maybe_unused]] const QString &action) { };
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
    VisFactory()
        : m_pNextVisFactory(g_pVisFactories)
        { g_pVisFactories = this;}
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

  protected:
    QColor         m_startColor  {Qt::yellow};
    QColor         m_targetColor {Qt::red};
    std::vector<double> m_magnitudes;
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

class WaveForm : public StereoScope
{
    static constexpr unsigned long kWFAudioSize { 4096 };
    static QImage s_image;      // picture of full track

public:
    WaveForm() = default;
    ~WaveForm() override;

    unsigned long getDesiredSamples(void) override;
    bool processUndisplayed(VisualNode *node) override;
    bool process( VisualNode *node ) override;
    bool draw( QPainter *p, const QColor &back ) override;
    void handleKeyPress(const QString &action) override;

  protected:
    void saveload(MusicMetadata *meta);
    QSize         m_wfsize {1920, 1080}; // picture size
    unsigned long m_offset {0};          // node offset for draw
    short         *m_right {nullptr};
    QFont         m_font;       // optional text overlay
    bool          m_showtext {false};
    MusicMetadata *m_currentMetadata {nullptr};
    unsigned long m_duration {60000}; // file length in milliseconds
    unsigned int  m_lastx    {1920};  // vert line tracker
    unsigned int  m_position {0};     // location inside pixel
    short int     m_minl     {0};     // left range minimum
    short int     m_maxl     {0};     // left range maximum
    unsigned long m_sqrl     {0};     // sum of squares, for RMS
    short int     m_minr     {0};     // right range minimum
    short int     m_maxr     {0};     // right range maximum
    unsigned long m_sqrr     {0};     // sum of squares, for RMS
    bool          m_stream   {false}; // true if radio stream
};

class LogScale
{
  public:
    explicit LogScale(int maxscale = 0, int maxrange = 0);

    int scale() const { return m_scale; }
    int range() const { return m_range; }

    void setMax(int maxscale, int maxrange);

    int operator[](int index);


  private:
    std::vector<int> m_indices;
    int  m_scale       {0};
    int  m_range       {0};
};

class MelScale
{
  public:
    explicit MelScale(int maxscale = 0, int maxrange = 0, int maxfreq = 0);

    int scale() const { return m_scale; }
    int range() const { return m_range; }

    void setMax(int maxscale, int maxrange, int maxfreq);
    static double hz2mel(double hz) { return 1127 * log(1 + (hz / 700)); }
    static double mel2hz(double mel) { return 700 * (exp(mel / 1127) - 1); }
    int operator[](int index);
    QString note(int note);     // text of note, 0 - 125
    int pixel(int note);        // position of note
    int freq(int note);         // frequency of note

  private:
    std::vector<int> m_indices;      // FFT bin of each pixel
    std::array<QString, 12> m_notes  // one octave of notes
    = {"C", ".", "D", ".", "E", "F", ".", "G", ".", "A", ".", "B"};
    std::array<int,126> m_pixels {0}; // pixel of each note
    std::array<int,126> m_freqs {0};  // frequency of each note
    int  m_scale       {0};
    int  m_range       {0};
};

// Spectrogram - by twitham@sbcglobal.net, 2023/05

class Spectrogram : public VisualBase
{
    // 1152 is the most I can get = 38.28125 fps @ 44100
    static constexpr int kSGAudioSize { 1152 };

  public:
    Spectrogram(bool hist);
    ~Spectrogram() override;

    unsigned long getDesiredSamples(void) override;
    void resize(const QSize &size) override; // VisualBase
    void FFT(VisualNode *node);
    bool processUndisplayed(VisualNode *node) override;
    bool process( VisualNode *node ) override;
    bool draw(QPainter *p, const QColor &back = Qt::black) override;
    void handleKeyPress(const QString &action) override;

    static QImage s_image;      // picture of spectrogram
    static int    s_offset;     // position on screen

  protected:
    static inline double clamp(double cur, double max, double min);
    QImage         *m_image;              // picture in use
    QSize          m_sgsize {1920, 1080}; // picture size
    QSize          m_size;                // displayed size
    MelScale       m_scale;               // Y-axis
    int            m_fftlen {16 * 1024}; // window width
    int            m_color {0};          // color or grayscale
    QVector<float> m_sigL;               // decaying signal window
    QVector<float> m_sigR;
    float*         m_dftL { nullptr }; // real in, complex out
    float*         m_dftR { nullptr };
    float*         m_rdftTmp { nullptr };
    static constexpr float kTxScale { 1.0F };
    AVTXContext*   m_rdftContext { nullptr };
    av_tx_fn       m_rdft        { nullptr };

    std::array<int,256*6> m_red   {0}; // continuous color spectrum
    std::array<int,256*6> m_green {0};
    std::array<int,256*6> m_blue  {0};
    bool           m_binpeak { true }; // peak of bins, else mean
    bool           m_history { true }; // spectrogram? or spectrum
    bool           m_showtext {false}; // freq overlay?
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
    bool processUndisplayed(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // VisualBase

  protected:
    static inline double clamp(double cur, double max, double min);

    QColor             m_startColor       {Qt::blue};
    QColor             m_targetColor      {Qt::red};
    QVector<QRect>     m_rectsL;
    QVector<QRect>     m_rectsR;
    QVector<float>     m_magnitudes;
    QSize              m_size;
    MelScale           m_scale;

    // Setup the "magical" audio data transformations
    // provided by the Fast Fourier Transforms library
    float              m_scaleFactor      {2.0};
    float              m_falloff          {10.0};
    int                m_analyzerBarWidth {6};

    int            m_fftlen {16 * 1024}; // window width
    QVector<float> m_sigL;               // decaying signal window
    QVector<float> m_sigR;
    float*         m_dftL { nullptr }; // real in, complex out
    float*         m_dftR { nullptr };
    float*         m_rdftTmp { nullptr };
    static constexpr float kTxScale { 1.0F };
    AVTXContext*   m_rdftContext { nullptr };
    av_tx_fn       m_rdft        { nullptr };
};

class Squares : public Spectrum
{
  public:
    Squares();
    ~Squares() override = default;

    void resize (const QSize &newsize) override; // Spectrum
    bool draw(QPainter *p, const QColor &back = Qt::black) override; // Spectrum

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

  protected:
    static inline double clamp(double cur, double max, double min);
    bool process_all_types(VisualNode *node, bool this_will_be_displayed);
    void zero_analysis(void);

    QColor          m_whiteStartColor  {245,245,245};
    QColor          m_whiteTargetColor {Qt::red};
    QColor          m_blackStartColor  {10,10,10};
    QColor          m_blackTargetColor {Qt::red};

    std::vector<QRect> m_rects;
    QSize           m_size;

    std::chrono::milliseconds m_offsetProcessed  {0ms};

    piano_key_data *m_pianoData        {nullptr};
    piano_audio    *m_audioData        {nullptr};

    std::vector<double> m_magnitude;
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

  private:
    QSize m_size;
};

#endif // __visualize_h
