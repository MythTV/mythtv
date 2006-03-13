#ifndef VBIDECODER_H_
#define VBIDECODER_H_

#include <stdint.h>

#include <qwaitcondition.h>
#include <qobject.h>
#include <qmutex.h>

class OSDType;
class CCDecoder;

class TeletextReader
{
  public:
    TeletextReader() { }
    virtual ~TeletextReader() { }
    virtual void AddTextData(unsigned char *buf, int len,
                             long long timecode, char type) = 0;
};

class TeletextViewer
{
  public:
    TeletextViewer() { }
    virtual ~TeletextViewer() { }

    virtual void KeyPress(uint key) { (void) key; }
    virtual void SetDisplaying(bool displaying) { (void) displaying; }

    virtual void Reset(void) = 0;
    virtual void AddPageHeader(int page,           int subpage,
                               const uint8_t *buf, int vbimode,
                               int lang,           int flags) = 0;
    virtual void AddTeletextData(int magazine, int row,
                                 const uint8_t* buf, int vbimode) = 0;
};

class TeletextDecoder
{
  public:
    TeletextDecoder() :  m_teletextviewer(NULL), m_decodertype(-1) {}
    virtual ~TeletextDecoder() {}

    /// Sets the TeletextViewer which will get the text from this decoder.
    void SetViewer(TeletextViewer *viewer)
        { m_teletextviewer = viewer; }

    /**
     *  \brief Returns the actual decoder type (DVB,IVTV,DVB_SUBTITLE...)
     *
     *   This is used for the decision in NuppelVideoPlayer
     *   to this TeletextDecoder or the caption only decoder.
     */
    int GetDecoderType(void) const
        { return m_decodertype; }

    void Decode(const unsigned char *buf, int vbimode);

  private:

    TeletextViewer *m_teletextviewer;
    int             m_decodertype;
};

#endif
