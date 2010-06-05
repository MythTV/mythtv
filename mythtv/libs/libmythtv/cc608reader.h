#ifndef CC608READER_H
#define CC608READER_H

#include <QMutex>

#include "mythexp.h"

#define MAXTBUFFER 60

class CC608Text
{
  public:
    CC608Text(QString T, int X, int Y, int C, int TT)
      : text(T), x(X), y(Y), color(C), teletextmode(TT) {}
    QString text;
    int x;
    int y;
    int color;
    bool teletextmode;
};

struct TextContainer
{
    int timecode;
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
        lock.lock();
        vector<CC608Text*>::iterator i = buffers.begin();
        for (; i != buffers.end(); i++)
        {
            CC608Text *cc = (*i);
            if (cc)
                delete cc;
        }
        buffers.clear();
        lock.unlock();
    }

    QMutex lock;
    vector<CC608Text*> buffers;
};

class NuppelVideoPlayer;

class MPUBLIC CC608Reader
{
  public:
    CC608Reader(NuppelVideoPlayer *parent);
   ~CC608Reader();

    void SetTTPageNum(int page)  { m_ccPageNum = page; }
    void SetEnabled(bool enable) { m_enabled = enable; }
    void FlushTxtBuffers(void);
    CC608Buffer* GetOutputText(bool &changed);
    void SetMode(int mode);
    void ClearBuffers(bool input, bool output);
    void AddTextData(unsigned char *buf, int len,
                     long long timecode, char type);
    void TranscodeWriteText(void (*func)
                           (void *, unsigned char *, int, int, int),
                            void * ptr);

  private:
    void Update(unsigned char *inpos);
    void Update608Text(vector<CC608Text*> *ccbuf,
                         int replace = 0, int scroll = 0,
                         bool scroll_prsv = false,
                         int scroll_yoff = 0, int scroll_ymax = 15);
    int  NumInputBuffers(void);

    NuppelVideoPlayer *m_parent;
    bool     m_enabled;
    // Input buffers
    int      m_readPosition;
    int      m_writePosition;
    QMutex   m_inputBufLock;
    int      m_maxTextSize;
    TextContainer m_inputBuffers[MAXTBUFFER+1];
    int      m_ccMode;
    int      m_ccPageNum;
    // Output buffers
    QString  m_outputText;
    int      m_outputCol;
    int      m_outputRow;
    bool     m_changed;
    CC608Buffer m_outputBuffers;
};

#endif // CC608READER_H
