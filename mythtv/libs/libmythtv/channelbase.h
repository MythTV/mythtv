#ifndef CHANNELBASE_H
#define CHANNELBASE_H

// Qt headers
#include <qmap.h>
#include <qstring.h>
#include <qsqldatabase.h>

// MythTV headers
#include "frequencies.h"
#include "tv.h"

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
 public:
    ChannelBase(TVRec *parent);
    virtual ~ChannelBase();

    // Methods that must be implemented.
    /// \brief Opens the channel changing hardware for use.
    virtual bool Open(void) = 0;
    /// \brief Closes the channel changing hardware to use.
    virtual void Close(void) = 0;
    /// \brief Switches to another input on hardware, 
    ///        and sets the channel is setstarting is true.
    virtual void SwitchToInput(int newcapchannel, bool setstarting) = 0;
    virtual bool SetChannelByString(const QString &chan) = 0;

    // Methods that one might want to specialize
    /// \brief Sets file descriptor.
    virtual void SetFd(int fd) { (void)fd; };
    /// \brief Returns file descriptor, -1 if it does not exist.
    virtual int GetFd(void) const { return -1; };

    // Sets
    virtual void SetChannelOrdering(const QString &chanorder)
        { channelorder = chanorder; }

    // Gets
    virtual int GetInputByName(const QString &input) const;
    virtual QString GetInputByNum(int capchannel) const;
    virtual QString GetCurrentName(void) const
        { return curchannelname; }
    virtual int GetCurrentInputNum(void) const
        { return currentcapchannel; }
    virtual QString GetCurrentInput(void) const
        { return channelnames[GetCurrentInputNum()]; }
    virtual QString GetOrdering(void) const
        { return channelorder; }
    /// \brief Returns true iff commercial detection is not required
    //         on current channel, for BBC, CBC, etc.
    bool IsCommercialFree(void) const { return commfree; }

    // Channel setting convenience method
    virtual bool SetChannelByDirection(ChannelChangeDirection);
#if 0
    /// @deprecated use SetChannelByDirection(ChannelChangeDirection)
    virtual bool ChannelUp(void)
        { return SetChannelByDirection(CHANNEL_DIRECTION_UP); }
    /// @deprecated use SetChannelByDirection(ChannelChangeDirection)
    virtual bool ChannelDown(void)
        { return SetChannelByDirection(CHANNEL_DIRECTION_DOWN); }
    /// @deprecated use SetChannelByDirection(ChannelChangeDirection)
    virtual bool NextFavorite(void)
        { return SetChannelByDirection(CHANNEL_DIRECTION_FAVORITE); }
#endif

    // Input toggling convenience methods
    virtual void ToggleInputs(void);
    virtual void SwitchToInput(const QString &input);
    virtual void SwitchToInput(const QString &input, const QString &chan);

    /// Saves current channel as the default channel for the current input.
    virtual void StoreInputChannels(void);

    // Picture attribute settings
    virtual void SetBrightness(void) {};
    virtual void SetColour(void) {};
    virtual void SetContrast(void) {};
    virtual void SetHue(void) {};
    virtual int  ChangeBrightness(bool up) { (void)up; return 0; };
    virtual int  ChangeColour(bool up)     { (void)up; return 0; };
    virtual int  ChangeContrast(bool up)   { (void)up; return 0; };
    virtual int  ChangeHue(bool up)        { (void)up; return 0; };

  protected:
    virtual bool ChangeExternalChannel(const QString &newchan);

    TVRec  *pParent;
    QString channelorder;
    QString curchannelname;
    int     capchannels;
    int     currentcapchannel;
    bool    commfree;

    QMap<int, QString> channelnames;
    QMap<int, QString> inputChannel;
    QMap<int, QString> inputTuneTo;
    QMap<int, QString> externalChanger;
    QMap<int, QString> sourceid;
};

#endif
