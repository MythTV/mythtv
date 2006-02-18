#ifndef VBIDECODER_H_
#define VBIDECODER_H_

#include <stdint.h>

#include <qwaitcondition.h>
#include <qobject.h>
#include <qmutex.h>

class OSDTypeTeletext;
class OSDType;
class CCDecoder;

class TeletextReader
{
  public:
    virtual ~TeletextReader() { }
    virtual void AddTextData(unsigned char *buf, int len,
                             long long timecode, char type) = 0;
};

class TeletextDecoder : public QObject
{
    Q_OBJECT

  public:
    TeletextDecoder();
    ~TeletextDecoder();

    // Sets
    void SetViewer(OSDTypeTeletext*);

    // Gets
    int GetDecoderType(void) const;

    void Decode(const unsigned char *buf, int vbimode);

  private:

    OSDTypeTeletext *m_teletextviewer;
    int              m_decodertype;
};

#endif
