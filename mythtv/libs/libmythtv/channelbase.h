// -*- Mode: c++ -*-

#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// Qt headers
#include <QStringList>
#include <qwaitcondition.h>
#include <qmutex.h>
#include <qthread.h>

// MythTV headers
#include "channelutil.h"
#include "inputinfo.h"
#include "mythsystem.h"
#include "tv.h"

class TVRec;
class ChannelBase;

/*
 * Thread to run tunning process in
 */
class ChannelThread : public QThread
{
  public:
    virtual void run(void);

    QString      channel;
    ChannelBase *tuner;
};

/** \class ChannelBase
 *  \brief Abstract class providing a generic interface to tuning hardware.
 *
 *   This class abstracts channel implementations for analog TV, ATSC, DVB, etc.
 *   Also implements many generic functions needed by most derived classes.
 *   It is responsible for tuning, i.e. switching channels.
 */

class ChannelBase
{
    friend class ChannelThread;

  public:
    enum Status { changeUnknown = 'U', changePending = 'P',
                  changeFailed = 'F', changeSuccess = 'S' };

    ChannelBase(TVRec *parent);
    virtual ~ChannelBase(void);

    virtual bool Init(QString &inputname, QString &startchannel, bool setchan);
    virtual bool IsTunable(const QString &input, const QString &channum) const;

    virtual void SelectChannel(const QString & chan, bool use_sm);

    Status GetStatus(void);
    Status Wait(void);

    // Methods that must be implemented.
    /// \brief Opens the channel changing hardware for use.
    virtual bool Open(void) = 0;
    /// \brief Closes the channel changing hardware to use.
    virtual void Close(void) = 0;
    /// \brief Reports whether channel is already open
    virtual bool IsOpen(void) const = 0;

    // Methods that one might want to specialize
    /// \brief Sets file descriptor.
    virtual void SetFd(int fd) { (void)fd; };
    /// \brief Returns file descriptor, -1 if it does not exist.
    virtual int GetFd(void) const { return -1; };

    // Gets
    virtual uint GetNextChannel(uint chanid, int direction) const;
    virtual uint GetNextChannel(const QString &channum, int direction) const;
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
    virtual DBChanList GetChannels(int inputNum) const;
    virtual DBChanList GetChannels(const QString &inputname) const;
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
//    virtual bool SwitchToInput(const QString &input); // not used?
    virtual bool SelectInput(const QString &input, const QString &chan,
                             bool use_sm);

    virtual bool InitializeInputs(void);

    // Misc. Commands
    virtual bool Retune(void) { return false; }

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void)
        { StoreInputChannels(m_inputs);
          StoreDefaultInput(GetCardID(), GetCurrentInput()); }

    // Picture attribute settings
    virtual bool InitPictureAttributes(void) { return false; }
    virtual int  GetPictureAttribute(PictureAttribute) const { return -1; }
    virtual int  ChangePictureAttribute(
        PictureAdjustType, PictureAttribute, bool up) { return -1; }

    bool CheckChannel(const QString &channum, QString& inputName) const;

    // \brief Set cardid for scanning
    void SetCardID(uint _cardid) { m_cardid = _cardid; }
    // \brief Set chan name for ringbuffer
    void SetChanNum(const QString & chan) { m_curchannelname= chan; }

    virtual int GetCardID(void) const;
  protected:
    virtual bool SetChannelByString(const QString &chan) = 0;

    /// \brief Switches to another input on hardware,
    ///        and sets the channel is setstarting is true.
    virtual bool SwitchToInput(int inputNum, bool setstarting);
    virtual bool IsInputAvailable(
        int inputNum, uint &mplexid_restriction) const;

    virtual bool ChangeExternalChannel(const QString &newchan);
    static void StoreInputChannels(const InputMap&);
    static void StoreDefaultInput(uint cardid, const QString &input);
    int GetDefaultInput(uint cardid);
    void ClearInputMap(void);

    bool Aborted();
    void setStatus(Status status);
    void TeardownAll(void);

    TVRec   *m_pParent;
    QString  m_curchannelname;
    int      m_currentInputID;
    bool     m_commfree;
    uint     m_cardid;
    InputMap m_inputs;
    DBChanList m_allchannels; ///< channels across all inputs

    QWaitCondition  m_tuneCond;

  private:
    mutable     ChannelThread   m_tuneThread;
    Status      m_tuneStatus;
    QMutex      m_thread_lock;
    bool        m_abort_change;
    MythSystem *m_changer;
};

#endif

