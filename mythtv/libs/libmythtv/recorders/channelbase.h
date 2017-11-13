// -*- Mode: c++ -*-

#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// Qt headers
#include <QWaitCondition>
#include <QStringList>
#include <QMutex>

// MythTV headers
#include "channelinfo.h"
#include "mythsystemlegacy.h"
#include "tv.h"
#include "videoouttypes.h" // for PictureAttribute

class FireWireDBOptions;
class GeneralDBOptions;
class DVBDBOptions;
class ChannelBase;
class TVRec;

/** \class ChannelBase
 *  \brief Abstract class providing a generic interface to tuning hardware.
 *
 *   This class abstracts channel implementations for analog TV, ATSC, DVB, etc.
 *   Also implements many generic functions needed by most derived classes.
 *   It is responsible for tuning, i.e. switching channels.
 */

class ChannelBase
{
    friend class SignalMonitor;

  public:
    explicit ChannelBase(TVRec *parent);
    virtual ~ChannelBase(void);

    virtual bool Init(QString &startchannel, bool setchan);
    virtual bool IsTunable(const QString &channum) const;

    // Methods that must be implemented.
    /// \brief Opens the channel changing hardware for use.
    virtual bool Open(void) = 0;
    /// \brief Closes the channel changing hardware to use.
    virtual void Close(void) = 0;
    /// \brief Reports whether channel is already open
    virtual bool IsOpen(void) const = 0;
    virtual bool SetChannelByString(const QString &chan) = 0;

    // Methods that one might want to specialize
    virtual void SetFormat(const QString &/*format*/) {}
    virtual int SetFreqTable(const QString &/*tablename*/) { return 0; }
    /// \brief Sets file descriptor.
    virtual void SetFd(int fd) { (void)fd; };
    /// \brief Returns file descriptor, -1 if it does not exist.
    virtual int GetFd(void) const { return -1; };
    virtual bool Tune(const QString &, int) { return true; }
    virtual bool IsExternalChannelChangeInUse(void);

    // Gets
    virtual uint GetNextChannel(uint chanid, ChannelChangeDirection direction) const;
    virtual uint GetNextChannel(const QString &channum, ChannelChangeDirection direction) const;
    virtual QString GetChannelName(void) const
        { return m_curchannelname; }
    virtual int GetChanID(void) const;
    virtual int GetInputID(void) const
        { return m_inputid; }
    virtual QString GetInputName(void) const
        { return m_name; }
    virtual uint GetSourceID(void) const
        { return m_sourceid; }

    /// \brief Returns true iff commercial detection is not required
    //         on current channel, for BBC, CBC, etc.
    bool IsCommercialFree(void) const { return m_commfree; }
    /// \brief Returns String representing device, useful for debugging
    virtual QString GetDevice(void) const { return ""; }

    // Sets
    virtual void Renumber(uint srcid, const QString &oldChanNum,
                          const QString &newChanNum);

    virtual bool InitializeInput(void);

    // Misc. Commands
    virtual bool Retune(void) { return false; }

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void);

    // Picture attribute settings
    virtual bool InitPictureAttributes(void) { return false; }
    virtual int  GetPictureAttribute(PictureAttribute) const { return -1; }
    virtual int  ChangePictureAttribute(
        PictureAdjustType, PictureAttribute, bool) { return -1; }

    bool CheckChannel(const QString &channum) const;

    // \brief Set inputid for scanning
    void SetInputID(uint _inputid) { m_inputid = _inputid; }

    static ChannelBase *CreateChannel(
        TVRec                    *tv_rec,
        const GeneralDBOptions   &genOpt,
        const DVBDBOptions       &dvbOpt,
        const FireWireDBOptions  &fwOpt,
        const QString            &startchannel,
        bool                      enter_power_save_mode,
        QString                  &rbFileExt);

  protected:
    /// \brief Switches to another input on hardware,
    ///        and sets the channel is setstarting is true.
    virtual bool IsInputAvailable(uint &mplexid_restriction,
                                  uint &chanid_restrtiction) const;
    virtual bool IsExternalChannelChangeSupported(void) { return false; }

  protected:
    bool KillScript(void);
    void HandleScript(const QString &freqid);
    virtual void HandleScriptEnd(bool ok);
    uint GetScriptStatus(bool holding_lock = false);

    bool ChangeExternalChannel(const QString &changer,
                               const QString &newchan);
    bool ChangeInternalChannel(const QString &newchan,
                               uint cardinputid);

    TVRec   *m_pParent;
    QString  m_curchannelname;
    bool     m_commfree;
    uint     m_inputid;
    uint     m_sourceid;
    QString  m_name;
    QString  m_startChanNum;
    QString  m_externalChanger;
    QString  m_tuneToChannel;
    ChannelInfoList m_channels; ///< channels across all inputs

    QMutex         m_system_lock;
    MythSystemLegacy    *m_system;
    /// These get mapped from the GENERIC_EXIT_* to these values for use
    /// with the signalmonitor code.
    /// 0 == unknown, 1 == pending, 2 == failed, 3 == success
    uint           m_system_status;
};


#endif
