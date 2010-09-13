#ifndef APPLEREMOTE
#define APPLEREMOTE

#include <string>
#include <vector>
#include <map>
#include <QThread>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

class AppleRemote : public QThread
{
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
        PlayPause      // Play or Pause
    };

    class Listener
    {
    public:
        virtual      ~Listener();
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
    void      run();

protected:
    AppleRemote(); // will be a singleton class

    static AppleRemote*      _instance;
    static const int         REMOTE_SWITCH_COOKIE;


private:
    bool                   openInExclusiveMode;
    IOHIDDeviceInterface** hidDeviceInterface;
    IOHIDQueueInterface**  queue;
    std::vector<int>       cookies;
    std::map< std::string, Event > cookieToButtonMapping;
    int                    remoteId;
    Listener*              _listener;

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
