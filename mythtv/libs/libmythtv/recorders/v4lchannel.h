// -*- Mode: c++ -*-

#ifndef CHANNEL_H
#define CHANNEL_H

#include "dtvchannel.h"
#ifdef USING_V4L2
#include <linux/videodev2.h> // needed for v4l2_std_id type
#else
typedef uint64_t v4l2_std_id;
#endif //USING_V4L2

using namespace std;

#define FAKE_VIDEO 0

class TVRec;

typedef QMap<int,v4l2_std_id> VidModV4L2;

/** \class V4LChannel
 *  \brief Implements tuning for TV cards using the V4L driver API,
 *         both versions 1 and 2.
 *
 *   This class supports a wide range of tuning hardware including
 *   frame grabbers (whose output requires encoding), hardware encoders,
 *   digital cameras, and non-encoding hardware which simply records
 *   pre-encoded broadcast streams.
 *
 */
class V4LChannel : public DTVChannel
{
 public:
    V4LChannel(TVRec *parent, const QString &videodevice);
    virtual ~V4LChannel(void);

    bool Init(QString &inputname, QString &startchannel, bool setchan);

    // Commands
    using DTVChannel::SwitchToInput;
    bool SwitchToInput(int newcapchannel, bool setstarting);
    bool Open(void);
    void Close(void);
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex &tuning, QString inputname);
    bool Tune(uint64_t frequency, QString inputname);
    bool Tune(const QString &freqid, int finetune);
    bool Retune(void);

    // Sets
    void SetFd(int fd);
    void SetFormat(const QString &format);
    int  SetDefaultFreqTable(const QString &name);

    // Gets
    bool IsOpen(void)       const { return GetFd() >= 0; }
    int  GetFd(void)        const { return videofd; }
    QString GetDevice(void) const { return device; }
    QString GetSIStandard(void) const { return "atsc"; }

    // Picture attributes.
    bool InitPictureAttributes(void);
    int  GetPictureAttribute(PictureAttribute) const;
    int  ChangePictureAttribute(PictureAdjustType, PictureAttribute, bool up);

  protected:
    virtual bool IsExternalChannelChangeSupported(void) { return true; }

  private:
    // Helper Sets
    void SetFreqTable(const int index);
    int  SetFreqTable(const QString &name);
    bool SetInputAndFormat(int newcapchannel, QString newFmt);

    // Helper Gets
    int  GetCurrentChannelNum(const QString &channame);
    QString GetFormatForChannel(QString channum,
                                QString inputname);

    // Helper Commands
    bool InitPictureAttribute(const QString &db_col_name);
    bool InitializeInputs(void);

  private:
    // Data
    QString     device;
    int         videofd;
    QString     device_name;
    QString     driver_name;
    QMap<QString,int> pict_attr_default;

    struct CHANLIST *curList;
    int         totalChannels;

    QString     currentFormat;
    bool        has_stream_io;
    bool        has_std_io;
    bool        has_async_io;
    bool        has_tuner;
    bool        has_sliced_vbi;

    VidModV4L2  videomode_v4l2; ///< Current video mode if 'usingv4l2' is true

    int         defaultFreqTable;
};

#endif
