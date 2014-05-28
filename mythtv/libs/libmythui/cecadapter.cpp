// Qt
#include <QApplication>
#include <QKeyEvent>
#include <QString>

// MythTV
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythevent.h"
#include "mythuihelper.h"
#include "mythmainwindow.h"

#include "cecadapter.h"
#include <vector>

#define MIN_LIBCEC_VERSION 1
#define MAX_CEC_DEVICES 10
#define LOC QString("CECAdapter: ")

#include <libcec/cec.h>
#include <iostream>
using namespace CEC;
using namespace std;
#include <libcec/cecloader.h>

QMutex*         CECAdapter::gLock = new QMutex(QMutex::Recursive);
QMutex*         CECAdapter::gHandleActionsLock = new QMutex();
QWaitCondition* CECAdapter::gActionsReady = new QWaitCondition();

// The libCEC callback functions
static int CECLogMessageCallback(void *adapter, const cec_log_message &message);
static int CECKeyPressCallback(void *adapter, const cec_keypress &keypress);
static int CECCommandCallback(void *adapter, const cec_command &command);

class CECAdapterPriv
{
  public:
    CECAdapterPriv()
      : adapter(NULL), defaultDevice("auto"), defaultHDMIPort(1),
        defaultDeviceID(CECDEVICE_PLAYBACKDEVICE1), valid(false),
        powerOffTV(false),  powerOffTVAllowed(false), powerOffTVOnExit(false),
        powerOnTV(false),   powerOnTVAllowed(false),  powerOnTVOnStart(false),
        switchInput(false), switchInputAllowed(true)
    {
		// libcec2's ICECCallbacks has a constructor that clears
		// all the entries. We're using 1.x....
		memset(&callbacks, 0, sizeof(callbacks));
		callbacks.CBCecLogMessage = &CECLogMessageCallback;
		callbacks.CBCecKeyPress   = &CECKeyPressCallback;
		callbacks.CBCecCommand    = &CECCommandCallback;
    }

    static QString addressToString(enum cec_logical_address addr, bool source)
    {
        switch (addr)
        {
            case CECDEVICE_UNKNOWN:          return QString("Unknown");
            case CECDEVICE_TV:               return QString("TV");
            case CECDEVICE_RECORDINGDEVICE1: return QString("RecordingDevice1");
            case CECDEVICE_RECORDINGDEVICE2: return QString("RecordingDevice2");
            case CECDEVICE_RECORDINGDEVICE3: return QString("RecordingDevice3");
            case CECDEVICE_TUNER1:           return QString("Tuner1");
            case CECDEVICE_TUNER2:           return QString("Tuner2");
            case CECDEVICE_TUNER3:           return QString("Tuner3");
            case CECDEVICE_TUNER4:           return QString("Tuner4");
            case CECDEVICE_PLAYBACKDEVICE1:  return QString("PlaybackDevice1");
            case CECDEVICE_PLAYBACKDEVICE2:  return QString("PlaybackDevice2");
            case CECDEVICE_PLAYBACKDEVICE3:  return QString("PlaybackDevice3");
            case CECDEVICE_AUDIOSYSTEM:      return QString("Audiosystem");
            case CECDEVICE_RESERVED1:        return QString("Reserved1");
            case CECDEVICE_RESERVED2:        return QString("Reserved2");
            case CECDEVICE_FREEUSE:          return QString("FreeUse");
            case CECDEVICE_UNREGISTERED:
            //case CECDEVICE_BROADCAST:
                return source ? QString("Unregistered") : QString("Broadcast");
        }
        return QString("Invalid");
    }

    // N.B. This may need revisiting when the UI is added
    static QStringList GetDeviceList(void)
    {
        QStringList results;
        cec_device_type_list list;
        list.Clear();
        list.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
        ICECAdapter *adapter = LibCecInit("MythTV", list);
        if (!adapter)
            return results;
        cec_adapter *devices = new cec_adapter[MAX_CEC_DEVICES];
        uint8_t num_devices = adapter->FindAdapters(devices, MAX_CEC_DEVICES, NULL);
        if (num_devices < 1)
            return results;
        for (uint8_t i = 0; i < num_devices; i++)
            results << QString::fromAscii(devices[i].comm);
        UnloadLibCec(adapter);
        return results;
    }

    bool Open(void)
    {
        // get settings
        // N.B. these do not currently work as there is no UI
        defaultDevice     = gCoreContext->GetSetting(LIBCEC_DEVICE, "auto").trimmed();
        QString hdmi_port = gCoreContext->GetSetting(LIBCEC_PORT, "auto");
        QString device_id = gCoreContext->GetSetting(LIBCEC_DEVICEID, "auto");
        powerOffTVAllowed = (bool)gCoreContext->GetNumSetting(POWEROFFTV_ALLOWED, 1);
        powerOffTVOnExit  = (bool)gCoreContext->GetNumSetting(POWEROFFTV_ONEXIT, 1);
        powerOnTVAllowed  = (bool)gCoreContext->GetNumSetting(POWERONTV_ALLOWED, 1);
        powerOnTVOnStart  = (bool)gCoreContext->GetNumSetting(POWERONTV_ONSTART, 1);

        defaultHDMIPort = 1;
        if ("auto" != hdmi_port)
        {
            defaultHDMIPort = hdmi_port.toInt();
            if (defaultHDMIPort < 1 || defaultHDMIPort > 4)
                defaultHDMIPort = 1;
        }
        defaultHDMIPort = defaultHDMIPort << 12;

        defaultDeviceID = CECDEVICE_PLAYBACKDEVICE1;
        if ("auto" != device_id)
        {
            int id = device_id.toInt();
            if (id < 1 || id > 3)
                id = 1;
            defaultDeviceID = (id == 1) ? CECDEVICE_PLAYBACKDEVICE1 :
                             ((id == 2) ? CECDEVICE_PLAYBACKDEVICE2 :
                                          CECDEVICE_PLAYBACKDEVICE3);
        }

        // create adapter interface
        cec_device_type_list list;
        list.Clear();
        list.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
        adapter = LibCecInit("MythTV", list);

        if (!adapter)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load libcec.");
            return false;
        }

        if (adapter->GetMinLibVersion() > MIN_LIBCEC_VERSION)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("The installed libCEC supports version %1 and above. "
                        "This version of MythTV only supports version %2.")
                .arg(adapter->GetMinLibVersion()).arg(MIN_LIBCEC_VERSION));
            return false;
        }

        // find adapters
        cec_adapter *devices = new cec_adapter[MAX_CEC_DEVICES];
        uint8_t num_devices = adapter->FindAdapters(devices, MAX_CEC_DEVICES, NULL);
        if (num_devices < 1)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to find any CEC devices.");
            return false;
        }

        // find required device and debug
        int devicenum = 0;
        bool find = defaultDevice != "auto";

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found %1 CEC devices(s).")
            .arg(num_devices));
        for (uint8_t i = 0; i < num_devices; i++)
        {
            QString comm = QString::fromAscii(devices[i].comm);
            bool match = find ? (comm == defaultDevice) : (i == 0);
            devicenum = match ? i : devicenum;
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Device %1: path '%2' com port '%3' %4").arg(i + 1)
                .arg(QString::fromAscii(devices[i].path)).arg(comm)
                .arg(match ? "SELECTED" : ""));
        }

        // open adapter
        QString path = QString::fromAscii(devices[devicenum].path);
        QString comm = QString::fromAscii(devices[devicenum].comm);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Trying to open device %1 (%2).")
            .arg(path).arg(comm));

        // set the callbacks
        // don't error check - versions < 1.6.3 always return
        // false. And newer versions always return true, so
        // there's not much point anyway
        adapter->EnableCallbacks(this, &callbacks);

        if (!adapter->Open(devices[devicenum].comm))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open device.");
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC + "Opened CEC device.");

        // get the vendor ID (for non-standard implementations)
        adapter->GetDeviceVendorId(CECDEVICE_TV);

        // set the physical address
        adapter->SetPhysicalAddress(defaultHDMIPort);

        // set the logical address
        adapter->SetLogicalAddress(defaultDeviceID);

        // all good to go
        valid = true;

        // turn on tv (if configured)
        powerOnTV = powerOnTVOnStart;

        // switch input (if configured)
        switchInput = true;

        HandleActions();

        return true;
    }


    void Close(void)
    {
        if (adapter)
        {
            // turn off tv (if configured)
            powerOffTV = powerOffTVOnExit;
            HandleActions();

            // delete adapter
            adapter->Close();
            UnloadLibCec(adapter);

            LOG(VB_GENERAL, LOG_INFO, LOC + "Closing down CEC.");
        }
        valid = false;
        adapter = NULL;
    }

    int LogMessage(const cec_log_message &message)
    {
        QString msg(message.message);
        int lvl = LOG_UNKNOWN;
        switch (message.level)
        {
            case CEC_LOG_ERROR:   lvl = LOG_ERR;     break;
            case CEC_LOG_WARNING: lvl = LOG_WARNING; break;
            case CEC_LOG_NOTICE:  lvl = LOG_INFO;    break;
            case CEC_LOG_DEBUG:   lvl = LOG_DEBUG;   break;
        }
        LOG(VB_GENERAL, lvl, LOC + QString("%1").arg(msg));
        return 1;
    }

    int HandleCommand(const cec_command &command)
    {
        if (!adapter || !valid)
            return 0;

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Command %1 from '%2' (%3) - destination '%4' (%5)")
            .arg(command.opcode)
            .arg(addressToString(command.initiator, true))
            .arg(command.initiator)
            .arg(addressToString(command.destination, false))
            .arg(command.destination));

        switch (command.opcode)
        {
            // TODO
            default:
                break;
        }
        gCoreContext->SendSystemEvent(QString("CEC_COMMAND_RECEIVED COMMAND %1")
                                      .arg(command.opcode));

        return 1;
    }

    int HandleKeyPress(const cec_keypress &key)
    {
        if (!adapter || !valid)
            return 0;

        // Ignore key down events and wait for the key 'up'
        if (key.duration < 1)
            return 1;

        QString code;
        int action = 0;
        switch (key.keycode)
        {
            case CEC_USER_CONTROL_CODE_NUMBER0:
                action = Qt::Key_0;
                code   = "0";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER1:
                action = Qt::Key_1;
                code   = "1";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER2:
                action = Qt::Key_2;
                code   = "2";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER3:
                action = Qt::Key_3;
                code   = "3";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER4:
                action = Qt::Key_4;
                code   = "4";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER5:
                action = Qt::Key_5;
                code   = "5";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER6:
                action = Qt::Key_6;
                code   = "6";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER7:
                action = Qt::Key_7;
                code   = "7";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER8:
                action = Qt::Key_8;
                code   = "8";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER9:
                action = Qt::Key_9;
                code   = "9";
                break;
            case CEC_USER_CONTROL_CODE_SELECT:
                action = Qt::Key_Select;
                code   = "SELECT";
                break;
            case CEC_USER_CONTROL_CODE_ENTER:
                action = Qt::Key_Enter;
                code   = "ENTER";
                break;
            case CEC_USER_CONTROL_CODE_UP:
                action = Qt::Key_Up;
                code   = "UP";
                break;
            case CEC_USER_CONTROL_CODE_DOWN:
                action = Qt::Key_Down;
                code   = "DOWN";
                break;
            case CEC_USER_CONTROL_CODE_LEFT:
                action = Qt::Key_Left;
                code   = "LEFT";
                break;
            case CEC_USER_CONTROL_CODE_LEFT_UP:
                action = Qt::Key_Left;
                code   = "LEFT_UP";
                break;
            case CEC_USER_CONTROL_CODE_LEFT_DOWN:
                action = Qt::Key_Left;
                code   = "LEFT_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT:
                action = Qt::Key_Right;
                code   = "RIGHT";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT_UP:
                action = Qt::Key_Right;
                code   = "RIGHT_UP";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT_DOWN:
                action = Qt::Key_Right;
                code   = "RIGHT_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_ROOT_MENU:
                action = Qt::Key_M;
                code   = "ROOT_MENU";
                break;
            case CEC_USER_CONTROL_CODE_EXIT:
                action = Qt::Key_Escape;
                code   = "EXIT";
                break;
            case CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL:
                action = Qt::Key_H;
                code   = "PREVIOUS_CHANNEL";
                break;
            case CEC_USER_CONTROL_CODE_SOUND_SELECT:
                action = Qt::Key_Plus;
                code   = "SOUND_SELECT";
                break;
            case CEC_USER_CONTROL_CODE_VOLUME_UP:
                action = Qt::Key_VolumeUp;
                code   = "VOLUME_UP";
                break;
            case CEC_USER_CONTROL_CODE_VOLUME_DOWN:
                action = Qt::Key_VolumeDown;
                code   = "VOLUME_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_MUTE:
                action = Qt::Key_VolumeMute;
                code   = "MUTE";
                break;
            case CEC_USER_CONTROL_CODE_PLAY:
                action = Qt::Key_P;
                code   = "PLAY";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE:
                action = Qt::Key_P; // same as play
                code   = "PAUSE";
                break;
            case CEC_USER_CONTROL_CODE_STOP:
                action = Qt::Key_Stop;
                code   = "STOP";
                break;
            case CEC_USER_CONTROL_CODE_RECORD:
                action = Qt::Key_R;
                code   = "RECORD";
                break;
            case CEC_USER_CONTROL_CODE_CLEAR:
                action = Qt::Key_Clear;
                code   = "CLEAR";
                break;
            case CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION:
                action = Qt::Key_I;
                code   = "DISPLAY_INFORMATION";
                break;
            case CEC_USER_CONTROL_CODE_PAGE_UP:
                action = Qt::Key_PageUp;
                code   = "PAGE_UP";
                break;
            case CEC_USER_CONTROL_CODE_PAGE_DOWN:
                action = Qt::Key_PageDown;
                code   = "PAGE_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_EJECT:
                action = Qt::Key_Eject;
                code   = "EJECT";
                break;
            case CEC_USER_CONTROL_CODE_FORWARD:
                action = Qt::Key_Forward;
                code   = "FORWARD";
                break;
            case CEC_USER_CONTROL_CODE_BACKWARD:
                action = Qt::Key_Back;
                code   = "BACKWARD";
                break;
            case CEC_USER_CONTROL_CODE_F1_BLUE:
                action = Qt::Key_F5; // NB F1 is help and we normally map blue to F5
                code   = "F1_BLUE";
                break;
            case CEC_USER_CONTROL_CODE_F2_RED:
                action = Qt::Key_F2;
                code   = "F2_RED";
                break;
            case CEC_USER_CONTROL_CODE_F3_GREEN:
                action = Qt::Key_F3;
                code   = "F3_GREEN";
                break;
            case CEC_USER_CONTROL_CODE_F4_YELLOW:
                action = Qt::Key_F4;
                code   = "F4_YELLOW";
                break;
            case CEC_USER_CONTROL_CODE_SETUP_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "SETUP_MENU";
                break;
            case CEC_USER_CONTROL_CODE_CONTENTS_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "CONTENTS_MENU";
                break;
            case CEC_USER_CONTROL_CODE_FAVORITE_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "FAVORITE_MENU";
                break;
            case CEC_USER_CONTROL_CODE_DOT:
                action = Qt::Key_Period;
                code  = "DOT";
                break;
            case CEC_USER_CONTROL_CODE_NEXT_FAVORITE:
                action = Qt::Key_Slash;
                code   = "NEXT_FAVORITE";
                break;
            case CEC_USER_CONTROL_CODE_INPUT_SELECT:
                action = Qt::Key_C;
                code = "INPUT_SELECT";
                break;
            case CEC_USER_CONTROL_CODE_HELP:
                action = Qt::Key_F1;
                code   = "HELP";
                break;
            case CEC_USER_CONTROL_CODE_STOP_RECORD:
                action = Qt::Key_R; // Duplicate of Record
                code = "STOP_RECORD";
                break;
            case CEC_USER_CONTROL_CODE_SUB_PICTURE:
                action = Qt::Key_V;
                code   = "SUB_PICTURE";
                break;
            case CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE:
                action = Qt::Key_S;
                code   = "ELECTRONIC_PROGRAM_GUIDE";
                break;
            case CEC_USER_CONTROL_CODE_POWER:
                action = Qt::Key_PowerOff;
                code = "POWER";
                break;

             // these codes have 'non-standard' Qt key mappings to ensure
             // each code has a unique key mapping
            case CEC_USER_CONTROL_CODE_CHANNEL_DOWN:
                action = Qt::Key_F20; // to differentiate from Up
                code   = "CHANNEL_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_CHANNEL_UP:
                action = Qt::Key_F21; // to differentiate from Down
                code   = "CHANNEL_UP";
                break;
            case CEC_USER_CONTROL_CODE_REWIND:
                action = Qt::Key_F22; // to differentiate from Left
                code   = "REWIND";
                break;
            case CEC_USER_CONTROL_CODE_FAST_FORWARD:
                action = Qt::Key_F23; // to differentiate from Right
                code   = "FAST_FORWARD";
                break;
            case CEC_USER_CONTROL_CODE_ANGLE:
                action = Qt::Key_F24;
                code = "ANGLE";
                break;
            case CEC_USER_CONTROL_CODE_F5:
                action = Qt::Key_F6; // NB!
                code = "F5";
                break;

            // codes with no obvious MythTV action
            case CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION:
                code = "INITIAL_CONFIGURATION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_RECORD:
                code = "PAUSE_RECORD";
                break;
            case CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND:
                code = "VIDEO_ON_DEMAND";
                break;
            case CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING:
                code = "TIMER_PROGRAMMING";
                break;
            case CEC_USER_CONTROL_CODE_UNKNOWN:
                code = "UNKNOWN";
                break;
            case CEC_USER_CONTROL_CODE_DATA:
                code = "DATA";
                break;

            // Functions aren't implemented (similar to macros?)
            case CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION:
                code = "POWER_ON_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PLAY_FUNCTION:
                code = "PLAY_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION:
                code = "PAUSE_PLAY_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_RECORD_FUNCTION:
                code = "RECORD_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION:
                code = "PAUSE_RECORD_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_STOP_FUNCTION:
                code = "STOP_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_MUTE_FUNCTION:
                code = "MUTE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION:
                code = "RESTORE_VOLUME_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_TUNE_FUNCTION:
                code = "TUNE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION:
                code = "SELECT_MEDIA_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION:
                code = "SELECT_AV_INPUT_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION:
                code = "SELECT_AUDIO_INPUT_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION:
                code = "POWER_TOGGLE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION:
                code = "POWER_OFF_FUNCTION";
                break;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Keypress %1 %2")
            .arg(code).arg(0 == action ? "(Not actioned)" : ""));

        if (0 == action)
            return 1;

        GetMythUI()->ResetScreensaver();
        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, action, Qt::NoModifier);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);

        return 1;
    }

    void HandleActions(void)
    {
        if (!adapter || !valid)
            return;

        // power state
        if (powerOffTV && powerOffTVAllowed)
        {
            if (adapter->StandbyDevices(CECDEVICE_TV))
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to turn off.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to turn TV off.");
        }

        if (powerOnTV && powerOnTVAllowed)
        {
            if (adapter->PowerOnDevices(CECDEVICE_TV))
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to turn on.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to turn TV on.");
        }

        // HDMI input
        if (switchInput && switchInputAllowed)
        {
            if (adapter->SetActiveView())
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to switch to this input.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to switch to this input.");
        }

        powerOffTV  = false;
        powerOnTV   = false;
        switchInput = false;
    }

    ICECAdapter *adapter;
    ICECCallbacks callbacks;
    QString      defaultDevice;
    int          defaultHDMIPort;
    cec_logical_address defaultDeviceID;
    bool         valid;
    bool         powerOffTV;
    bool         powerOffTVAllowed;
    bool         powerOffTVOnExit;
    bool         powerOnTV;
    bool         powerOnTVAllowed;
    bool         powerOnTVOnStart;
    bool         switchInput;
    bool         switchInputAllowed;
};

QStringList CECAdapter::GetDeviceList(void)
{
    QMutexLocker lock(gLock);
    return CECAdapterPriv::GetDeviceList();
}

CECAdapter::CECAdapter() : MThread("CECAdapter"), m_priv(new CECAdapterPriv)
{
    QMutexLocker lock(gLock);

    // don't try if disabled
    if (!gCoreContext->GetNumSetting(LIBCEC_ENABLED, 1))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "libCEC support is disabled.");
        return;
    }

    // create the actual adapter
    if (!m_priv->Open())
        return;

    // start thread
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Starting thread.");
    start();
}

void CECAdapter::run()
{
    for (;;) {
        // Note that a lock is used because the QWaitCondition needs it
        // None of the other HandleActions callers need the lock because
        // they call HandleActions at open/close time, when
        // nothing else can be calling it....
        gHandleActionsLock->lock();
        gActionsReady->wait(gHandleActionsLock);
        m_priv->HandleActions();
        gHandleActionsLock->unlock();
    }
}

CECAdapter::~CECAdapter()
{
    QMutexLocker lock(gLock);

    // stop thread
    if (isRunning())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Stopping thread.");
        exit();
    }

    // delete the actual adapter
    m_priv->Close();
}

bool CECAdapter::IsValid(void)
{
    QMutexLocker lock(gLock);
    return m_priv->valid;
}

void CECAdapter::Action(const QString &action)
{
    QMutexLocker lock(gLock);
    if (ACTION_TVPOWERON == action)
        m_priv->powerOnTV = true;
    else if (ACTION_TVPOWEROFF == action)
        m_priv->powerOffTV = true;
	gActionsReady->wakeAll();
}

static int CECLogMessageCallback(void *adapter, const cec_log_message &message)
{
    return ((CECAdapterPriv*)adapter)->LogMessage(message);
}

static int CECKeyPressCallback(void *adapter, const cec_keypress &keypress)
{
    return ((CECAdapterPriv*)adapter)->HandleKeyPress(keypress);
}

static int CECCommandCallback(void *adapter, const cec_command &command)
{
    return ((CECAdapterPriv*)adapter)->HandleCommand(command);
}

