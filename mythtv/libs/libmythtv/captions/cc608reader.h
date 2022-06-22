#ifndef CC608READER_H
#define CC608READER_H

#include <cstdint>
#include <utility>

// Qt headers
#include <QMutex>

// MythTV headers
#include "libmythtv/captions/cc608decoder.h"
#include "libmythtv/mythtvexp.h"

static constexpr int8_t MAXTBUFFER    { 60     };
static constexpr int8_t MAXOUTBUFFERS { 16 + 1 };

class CC608Text
{
  public:
    CC608Text(QString T, int X, int Y) :
        m_text(std::move(T)), m_x(X), m_y(Y) {}
    CC608Text(const CC608Text &other)  = default;
    QString m_text;
    int     m_x;
    int     m_y;
};

struct TextContainer
{
    std::chrono::milliseconds timecode;
    int len;
    unsigned char *buffer;
    char type;
};

class CC608Buffer
{
  public:
   ~CC608Buffer(void) { Clear(); }
    void Clear(void)
    {
        m_lock.lock();
        for (auto *cc : m_buffers)
            delete cc;
        m_buffers.clear();
        m_lock.unlock();
    }

    QMutex             m_lock;
    std::vector<CC608Text*> m_buffers;
};

class CC608StateTracker
{
  public:
    CC608StateTracker() = default;
    
    void Clear(void)
    {
        m_outputText = "";
        m_outputCol = 0;
        m_outputRow = 0;
        m_changed = true;
        m_output.Clear();
    }

    QString  m_outputText;
    int      m_outputCol {0};
    int      m_outputRow {0};
    bool     m_changed   {true};
    CC608Buffer m_output;
};

class MythPlayer;

using CC608WriteFn = void (*) (void *, unsigned char *, int, std::chrono::milliseconds, int);

class MTV_PUBLIC CC608Reader : public CC608Input
{
  public:
    explicit CC608Reader(MythPlayer *parent);
   ~CC608Reader() override;

    void SetTTPageNum(int page)  { m_ccPageNum = page; }
    void SetEnabled(bool enable) { m_enabled = enable; }
    void FlushTxtBuffers(void);
    CC608Buffer *GetOutputText(bool &changed, int &streamIdx);
    CC608Buffer* GetOutputText(bool &changed);
    void SetMode(int mode);
    void ClearBuffers(bool input, bool output, int outputStreamIdx = -1);
    void AddTextData(unsigned char *buf, int len,
                     std::chrono::milliseconds timecode, char type) override; // CC608Input
    void TranscodeWriteText(CC608WriteFn func, void * ptr);

  private:
    int Update(unsigned char *inpos);
    void Update608Text(std::vector<CC608Text*> *ccbuf,
                         int replace = 0, int scroll = 0,
                         bool scroll_prsv = false,
                         int scroll_yoff = 0, int scroll_ymax = 15,
                         int streamIdx = CC_CC1);
    int  NumInputBuffers(bool need_to_lock = true);

    MythPlayer       *m_parent        {nullptr};
    bool              m_enabled       {false};
    // Input buffers
    int               m_readPosition  {0};
    int               m_writePosition {0};
    QMutex            m_inputBufLock;
    int               m_maxTextSize   {0};
    std::array<TextContainer,MAXTBUFFER+1> m_inputBuffers {};
    int               m_ccMode        {CC_CC1};
    int               m_ccPageNum     {0x888};
    // Output buffers
    std::array<CC608StateTracker,MAXOUTBUFFERS> m_state;
};

#endif // CC608READER_H
