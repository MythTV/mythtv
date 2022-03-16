#ifndef APPLEREMOTE
#define APPLEREMOTE

// C++ headers
#include <string>
#include <vector>
#include <map>

// MythTV headers
#include "libmythbase/mthread.h"

#include <QTimer>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

class AppleRemote : public QObject, public MThread
{
    Q_OBJECT
public:
    enum Event
    { // label/meaning on White ... and Aluminium remote
        Up = 0,        // VolumePlus    Up
        Down,          // VolumeMinus   Down
        Menu,
        Select,        // Play          Select
        Right,
        Left,
        RightHold,
        LeftHold,
        MenuHold,
        PlayHold,  // was PlaySleep
        ControlSwitched,
        PlayPause,     // Play or Pause
        Undefined      // Used to handle the Apple TV > v2.3
    };

    class Listener
    {
    public:
        virtual      ~Listener() = default;
        virtual void appleRemoteButton(Event button, bool pressedDown) = 0;
    };

    static AppleRemote * Get();
    ~AppleRemote();

    bool      isListeningToRemote();
    void      setListener(Listener* listener);
    Listener* listener()                      { return _listener; }
    void      setOpenInExclusiveMode(bool in) { openInExclusiveMode = in; };
    bool      isOpenInExclusiveMode()         { return openInExclusiveMode; };
    void      startListening();
    void      stopListening();
    void      run() override; // MThread

protected:
    AppleRemote(); // will be a singleton class

    static AppleRemote*      _instance;


private:
    bool                   openInExclusiveMode {true};
    IOHIDDeviceInterface** hidDeviceInterface  {nullptr};
    IOHIDQueueInterface**  queue               {nullptr};
    std::vector<int>       cookies;
    std::map< std::string, Event > cookieToButtonMapping;
    int                    remoteId            {0};
    Listener*              _listener           {nullptr};

    AppleRemote::Event     mLastEvent          {AppleRemote::Undefined};
    int                    mEventCount         {0};
    bool                   mKeyIsDown          {false};
    QTimer*                mCallbackTimer      {nullptr};

    void        _initCookieMap();
    bool        _initCookies();
    bool        _createDeviceInterface(io_object_t hidDevice);
    bool        _openDevice();

    static void QueueCallbackFunction(void* target, IOReturn result,
                                      void* refcon, void* sender);
    void        _queueCallbackFunction(IOReturn result,
                                       void* refcon, void* sender);
    void        _handleEventWithCookieString(std::string cookieString,
                                             SInt32 sumOfValues);
};

#endif // APPLEREMOTE
