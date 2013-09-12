// -*- Mode: c++ -*-

#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// Qt headers
#include <QWaitCondition>
#include <QStringList>
#include <QMutex>

// MythTV headers
#include "channelinfo.h"
#include "inputinfo.h"
#include "mythsystemlegacy.h"
#include "tv.h"

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
    ChannelBase(TVRec *parent);
    virtual ~ChannelBase(void);

    virtual bool Init(QString &inputname, QString &startchannel, bool setchan);
    virtual bool IsTunable(const QString &input, const QString &channum) const;

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
    virtual bool Tune(const QString &freqid, int finetune) { return true; }
    virtual bool IsExternalChannelChangeInUse(void);

    // Gets
    virtual uint GetNextChannel(uint chanid, ChannelChangeDirection direction) const;
    virtual uint GetNextChannel(const QString &channum, ChannelChangeDirection direction) const;
    virtual int GetInputByName(const QString &input) const;
    virtual QString GetInputByNum(int capchannel) const;
    virtual QString GetCurrentName(void) const
        { return m_curchannelname; }
    virtual int GetChanID(void) const;
    virtual int GetCurrentInputNum(void) const
        { return m_currentInputID; }
    virtual QString GetCurrentInput(void) const
        { return m_inputs[GetCurrentInputNum()]->name; }
    virtual int GetNextInputNum(void) const;
    virtual QString GetNextInput(void) const
        { return m_inputs[GetNextInputNum()]->name; }
    virtual QString GetNextInputStartChan(void)
        { return m_inputs[GetNextInputNum()]->startChanNum; }
    virtual uint GetCurrentSourceID(void) const
        { return m_inputs[GetCurrentInputNum()]->sourceid; }
    virtual uint GetSourceID(int inputID) const
        { return m_inputs[inputID]->sourceid; }
    virtual uint GetInputCardID(int inputNum) const;
    virtual ChannelInfoList GetChannels(int inputNum) const;
    virtual ChannelInfoList GetChannels(const QString &inputname) const;
    virtual vector<InputInfo> GetFreeInputs(
        const vector<uint> &excluded_cards) const;
    virtual QStringList GetConnectedInputs(void) const;

    /// \brief Returns true iff commercial detection is not required
    //         on current channel, for BBC, CBC, etc.
    bool IsCommercialFree(void) const { return m_commfree; }
    /// \brief Returns String representing device, useful for debugging
    virtual QString GetDevice(void) const { return ""; }

    // Sets
    virtual void Renumber(uint srcid, const QString &oldChanNum,
                          const QString &newChanNum);

    // Input toggling convenience methods
    virtual bool SwitchToInput(const QString &input);
    virtual bool SwitchToInput(const QString &input, const QString &chan);

    virtual bool InitializeInputs(void);

    // Misc. Commands
    virtual bool Retune(void) { return false; }

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void)
        { StoreInputChannels(m_inputs); }

    // Picture attribute settings
    virtual bool InitPictureAttributes(void) { return false; }
    virtual int  GetPictureAttribute(PictureAttribute) const { return -1; }
    virtual int  ChangePictureAttribute(
        PictureAdjustType, PictureAttribute, bool up) { return -1; }

    bool CheckChannel(const QString &channum, QString& inputName) const;

    // \brief Set cardid for scanning
    void SetCardID(uint _cardid) { m_cardid = _cardid; }

    virtual uint GetCardID(void) const;

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
    virtual bool SwitchToInput(int inputNum, bool setstarting);
    virtual bool IsInputAvailable(
        int inputNum, uint &mplexid_restriction) const;
    virtual bool IsExternalChannelChangeSupported(void) { return false; }

    int GetStartInput(uint cardid);
    void ClearInputMap(void);

    static void StoreInputChannels(const InputMap&);

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
    int      m_currentInputID;
    bool     m_commfree;
    uint     m_cardid;
    InputMap m_inputs;
    ChannelInfoList m_allchannels; ///< channels across all inputs

    QMutex         m_system_lock;
    MythSystemLegacy    *m_system;
    /// These get mapped from the GENERIC_EXIT_* to these values for use
    /// with the signalmonitor code.
    /// 0 == unknown, 1 == pending, 2 == failed, 3 == success
    uint           m_system_status;
};


#endif
