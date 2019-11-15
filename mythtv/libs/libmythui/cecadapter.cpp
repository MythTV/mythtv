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

#if CEC_LIB_VERSION_MAJOR < 2
// A few defines taken from libcec.h v2
#define CEC_MIN_HDMI_PORTNUMBER      1
#define CEC_MAX_HDMI_PORTNUMBER      15
// libcec1's callback parameters are pass-by-ref
#define CEC_CALLBACK_PARAM_TYPE &
#else
#if CEC_LIB_VERSION_MAJOR <= 3
// libcec2 and 3 callback parameters are pass-by-value
#define CEC_CALLBACK_PARAM_TYPE
#endif
#endif

// The libCEC callback functions
#if CEC_LIB_VERSION_MAJOR <= 3
static int CECLogMessageCallback(void *adapter, const cec_log_message CEC_CALLBACK_PARAM_TYPE message);
static int CECKeyPressCallback(void *adapter, const cec_keypress CEC_CALLBACK_PARAM_TYPE keypress);
static int CECCommandCallback(void *adapter, const cec_command CEC_CALLBACK_PARAM_TYPE command);
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
static void CECLogMessageCallback(void *adapter, const cec_log_message* message);
static void CECKeyPressCallback(void *adapter, const cec_keypress* keypress);
static void CECCommandCallback(void *adapter, const cec_command* command);
#endif

#if CEC_LIB_VERSION_MAJOR >= 2
#if CEC_LIB_VERSION_MAJOR <= 3
static int CECAlertCallback(void *adapter, const libcec_alert alert, const libcec_parameter CEC_CALLBACK_PARAM_TYPE data);
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
static void CECAlertCallback(void *adapter, libcec_alert alert, libcec_parameter data);
#endif
static void CECSourceActivatedCallback(void *adapter, cec_logical_address address, uint8_t activated);
#endif

class CECAdapterPriv
{
  public:
#if CEC_LIB_VERSION_MAJOR < 2
    CECAdapterPriv()
     {
        // libcec1 has this as a POD struct, with no
        // automatic initialisation.
        // And no .Clear() method...
        memset(&m_callbacks, 0, sizeof(m_callbacks));
    }
#else
    CECAdapterPriv() = default;
#endif

    bool Open(void)
    {
        // get settings
        // N.B. these need to be set manually since there is no UI
        QString defaultDevice   = gCoreContext->GetSetting(LIBCEC_DEVICE, "auto").trimmed();
        // Note - if libcec supports automatic detection via EDID then
        // these settings are not used
        // The logical address of the HDMI device Myth is connected to
        QString base_dev        = gCoreContext->GetSetting(LIBCEC_BASE, "auto").trimmed();
        // The number of the HDMI port Myth is connected to
        QString hdmi_port       = gCoreContext->GetSetting(LIBCEC_PORT, "auto").trimmed();

        m_powerOffTVAllowed = (bool)gCoreContext->GetNumSetting(POWEROFFTV_ALLOWED, 1);
        m_powerOffTVOnExit  = (bool)gCoreContext->GetNumSetting(POWEROFFTV_ONEXIT, 1);
        m_powerOnTVAllowed  = (bool)gCoreContext->GetNumSetting(POWERONTV_ALLOWED, 1);
        m_powerOnTVOnStart  = (bool)gCoreContext->GetNumSetting(POWERONTV_ONSTART, 1);

        // create adapter interface
        libcec_configuration configuration;
#if CEC_LIB_VERSION_MAJOR < 2
        // libcec1 has this as a POD struct, with no
        // automatic initialisation
        configuration.Clear();
#endif
        strcpy(configuration.strDeviceName, "MythTV");
        configuration.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);

        if ("auto" != base_dev)
        {
            int base = base_dev.toInt();
            if (base >= 0 && base < CECDEVICE_BROADCAST) {
                configuration.baseDevice = (cec_logical_address)base;
            }
        }
        if ("auto" != hdmi_port)
         {
            int defaultHDMIPort = hdmi_port.toInt();
            if (defaultHDMIPort >= CEC_MIN_HDMI_PORTNUMBER && defaultHDMIPort <= CEC_MAX_HDMI_PORTNUMBER) {
                configuration.iHDMIPort = defaultHDMIPort;
            }
        }

        // Set up the callbacks
#if CEC_LIB_VERSION_MAJOR <= 3
        m_callbacks.CBCecLogMessage = &CECLogMessageCallback;
        m_callbacks.CBCecKeyPress   = &CECKeyPressCallback;
        m_callbacks.CBCecCommand    = &CECCommandCallback;
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
        m_callbacks.logMessage      = &CECLogMessageCallback;
        m_callbacks.keyPress        = &CECKeyPressCallback;
        m_callbacks.commandReceived = &CECCommandCallback;
#endif
#if CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MAJOR <= 3
        m_callbacks.CBCecAlert      = &CECAlertCallback;
        m_callbacks.CBCecSourceActivated = &CECSourceActivatedCallback;
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
        m_callbacks.alert           = &CECAlertCallback;
        m_callbacks.sourceActivated = &CECSourceActivatedCallback;
#endif
        configuration.callbackParam = this;
        configuration.callbacks = &m_callbacks;

        // and initialise
        m_adapter = LibCecInitialise(&configuration);

        if (!m_adapter)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load libcec.");
            return false;
        }

        // initialise the host on which libCEC is running
        m_adapter->InitVideoStandalone();

        // find adapters
#if CEC_LIB_VERSION_MAJOR >= 4
        cec_adapter_descriptor *devices = new cec_adapter_descriptor[MAX_CEC_DEVICES];
        uint8_t num_devices = m_adapter->DetectAdapters(devices, MAX_CEC_DEVICES, nullptr, true);
#else
        cec_adapter *devices = new cec_adapter[MAX_CEC_DEVICES];
        uint8_t num_devices = m_adapter->FindAdapters(devices, MAX_CEC_DEVICES, nullptr);
#endif
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
#if CEC_LIB_VERSION_MAJOR >= 4
            QString comm = QString::fromLatin1(devices[i].strComName);
            QString path = QString::fromLatin1(devices[i].strComPath);
#else
            QString comm = QString::fromLatin1(devices[i].comm);
            QString path = QString::fromLatin1(devices[i].path);
#endif
            bool match = find ? (comm == defaultDevice) : (i == 0);
            devicenum = match ? i : devicenum;
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Device %1: path '%2' com port '%3' %4").arg(i + 1)
                .arg(path).arg(comm)
                .arg(match ? "SELECTED" : ""));
        }

        // open adapter
#if CEC_LIB_VERSION_MAJOR >= 4
        QString comm = QString::fromLatin1(devices[devicenum].strComName);
        QString path = QString::fromLatin1(devices[devicenum].strComPath);
#else
        QString comm = QString::fromLatin1(devices[devicenum].comm);
        QString path = QString::fromLatin1(devices[devicenum].path);
#endif
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Trying to open device %1 (%2).")
            .arg(path).arg(comm));

#if CEC_LIB_VERSION_MAJOR >= 4
        if (!m_adapter->Open(devices[devicenum].strComName))
#else
        if (!m_adapter->Open(devices[devicenum].comm))
#endif
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to open device.");
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC + "Opened CEC device.");

        // turn on tv (if configured)
        m_powerOnTV = m_powerOnTVOnStart;

        // switch input (if configured)
        m_switchInput = true;

        // all good to go
        m_valid = true;

        return true;
    }

    void Close(void)
    {
        if (m_adapter)
        {
            // turn off tv (if configured)
            m_powerOffTV = m_powerOffTVOnExit;
            HandleActions();

            // delete adapter
            m_adapter->Close();
            UnloadLibCec(m_adapter);

            LOG(VB_GENERAL, LOG_INFO, LOC + "Closing down CEC.");
        }
        m_valid = false;
        m_adapter = nullptr;
    }

    static int LogMessage(const cec_log_message &message)
    {
        QString msg(message.message);
        LogLevel_t lvl = LOG_UNKNOWN;
        switch (message.level)
        {
            case CEC_LOG_ERROR:   lvl = LOG_ERR;     break;
            case CEC_LOG_WARNING: lvl = LOG_WARNING; break;
            case CEC_LOG_NOTICE:  lvl = LOG_INFO;    break;
            case CEC_LOG_DEBUG:   lvl = LOG_DEBUG;   break;
            default: break;
        }
        LOG(VB_GENERAL, lvl, LOC + QString("%1").arg(msg));
        return 1;
    }

    static void LogMessage(const cec_log_message* message)
    {
        QString msg(message->message);
        LogLevel_t lvl = LOG_UNKNOWN;
        switch (message->level)
        {
            case CEC_LOG_ERROR:   lvl = LOG_ERR;     break;
            case CEC_LOG_WARNING: lvl = LOG_WARNING; break;
            case CEC_LOG_NOTICE:  lvl = LOG_INFO;    break;
            case CEC_LOG_DEBUG:   lvl = LOG_DEBUG;   break;
            default: break;
        }
        LOG(VB_GENERAL, lvl, LOC + QString("%1").arg(msg));
    }

    // NOTE - libcec2 changes the callbacks
    // to be pass-by-value.
    // For simplicity, this function remains as pass-by-ref
    int HandleCommand(const cec_command &command)
    {
        if (!m_adapter || !m_valid)
            return 0;

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Command %1 from '%2' (%3) - destination '%4' (%5)")
            .arg(command.opcode)
            .arg(m_adapter->ToString(command.initiator))
            .arg(command.initiator)
            .arg(m_adapter->ToString(command.destination))
            .arg(command.destination));

        switch (command.opcode)
        {
            case CEC_OPCODE_MENU_REQUEST:
                cec_keypress key;
                key.keycode = CEC_USER_CONTROL_CODE_ROOT_MENU;
                key.duration = 5;
                HandleKeyPress(key);
                // SetMenuState could be used to disable the TV menu here
                // That may be a user-unfriendly thing to do
                // So they have to see both the TV menu and the MythTV menu
                break;

            default:
                break;
        }
        gCoreContext->SendSystemEvent(QString("CEC_COMMAND_RECEIVED COMMAND %1")
                                      .arg(command.opcode));

        return 1;
    }

    void HandleCommand(const cec_command* command)
    {
        if (!m_adapter || !m_valid)
            return;

        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Command %1 from '%2' (%3) - destination '%4' (%5)")
            .arg(command->opcode)
            .arg(m_adapter->ToString(command->initiator))
            .arg(command->initiator)
            .arg(m_adapter->ToString(command->destination))
            .arg(command->destination));

        switch (command->opcode)
        {
            // TODO
            default:
                break;
        }
        gCoreContext->SendSystemEvent(QString("CEC_COMMAND_RECEIVED COMMAND %1")
                                      .arg(command->opcode));
    }

    int HandleKeyPress(const cec_keypress &key)
    {
        if (!m_adapter || !m_valid)
            return 0;

        // Ignore key down events and wait for the key 'up'
        if (key.duration < 1)
            return 1;

        QString code;
        int action = 0;
        Qt::KeyboardModifier modifier = Qt::NoModifier;
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
                // Play set to control-p to differentiate from pause
                modifier = Qt::ControlModifier;
                break;
            case CEC_USER_CONTROL_CODE_PAUSE:
                action = Qt::Key_P;
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

            default:
                code = QString("UNKNOWN_FUNCTION_%1").arg(key.keycode);
                break;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Keypress %1 %2")
            .arg(code).arg(0 == action ? "(Not actioned)" : ""));

        if (0 == action)
            return 1;

        MythUIHelper::ResetScreensaver();
        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, action, modifier);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);

        return 1;
    }

    void HandleKeyPress(const cec_keypress* key)
    {
        if (!m_adapter || !m_valid)
            return;

        // Ignore key down events and wait for the key 'up'
        if (key->duration < 1)
            return;

        QString code;
        int action = 0;
        switch (key->keycode)
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

            default:
                code = QString("UNKNOWN_FUNCTION_%1").arg(key->keycode);
                break;
        }

        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Keypress %1 %2")
            .arg(code).arg(0 == action ? "(Not actioned)" : ""));

        if (0 == action)
            return;

        MythUIHelper::ResetScreensaver();
        QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, action, Qt::NoModifier);
        qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
    }

#if CEC_LIB_VERSION_MAJOR >= 2
    static int HandleAlert(const libcec_alert alert, const libcec_parameter &data)
    {
        // These aren't handled yet
        // Note that we *DON'T* want to just show these
        // to the user in a popup, because some (eg prompting about firmware
        // upgrades) aren't appropriate.
        // Ideally we'd try to handle this, eg by reopening the adapter
        // in a separate thread if it lost the connection....

        QString param;
        switch (data.paramType)
        {
            case CEC_PARAMETER_TYPE_STRING:
                param = QString(": %1").arg((char*)data.paramData);
                break;
            case CEC_PARAMETER_TYPE_UNKOWN: /* libcec typo */
            default:
                if (data.paramData != nullptr)
                {
                    param = QString(": UNKNOWN param has type %1").arg(data.paramType);
                }
                break;
        }

        // There is no ToString method for libcec_alert...
        // Plus libcec adds new values in minor releases (eg 2.1.1)
        // but doesn't provide a #define for the last digit...
        // Besides, it makes sense to do this, since we could be compiling
        // against an older version than we're running against
#if CEC_LIB_VERSION_MAJOR == 2 && CEC_LIB_VERSION_MINOR < 1
// since 2.0.4
#define CEC_ALERT_PHYSICAL_ADDRESS_ERROR        4
#endif
#if CEC_LIB_VERSION_MAJOR == 2 && CEC_LIB_VERSION_MINOR < 2
// since 2.1.1
#define CEC_ALERT_TV_POLL_FAILED                5
#endif
        switch (alert)
        {
            case CEC_ALERT_SERVICE_DEVICE:
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("CEC device service message") + param);
                break;
            case CEC_ALERT_CONNECTION_LOST:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("CEC device connection list") + param);
                break;
            case CEC_ALERT_PERMISSION_ERROR:
            case CEC_ALERT_PORT_BUSY:
                /* Don't log due to possible false positives on the initial
                 * open. libcec will log via the logging callback anyway
                 */
                break;
            case CEC_ALERT_PHYSICAL_ADDRESS_ERROR:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("CEC physical address error") + param);
                break;
            case CEC_ALERT_TV_POLL_FAILED:
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("CEC device can't poll TV") + param);
                break;
            default:
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("UNKNOWN CEC device alert %1").arg(alert) + param);
                break;
        }

        return 1;
    }

    void HandleSourceActivated(const cec_logical_address address, const uint8_t activated)
    {
        if (!m_adapter || !m_valid)
            return;

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Source %1 %2")
            .arg(m_adapter->ToString(address))
            .arg(activated ? "Activated" : "Deactivated"));

        if (activated)
            MythUIHelper::ResetScreensaver();
    }
#endif

    void HandleActions(void)
    {
        if (!m_adapter || !m_valid)
            return;

        // power state
        if (m_powerOffTV && m_powerOffTVAllowed)
        {
            if (m_adapter->StandbyDevices(CECDEVICE_TV))
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to turn off.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to turn TV off.");
        }

        if (m_powerOnTV && m_powerOnTVAllowed)
        {
            if (m_adapter->PowerOnDevices(CECDEVICE_TV))
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to turn on.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to turn TV on.");
        }

        // HDMI input
        if (m_switchInput && m_switchInputAllowed)
        {
            if (m_adapter->SetActiveSource())
                LOG(VB_GENERAL, LOG_INFO, LOC + "Asked TV to switch to this input.");
            else
                LOG(VB_GENERAL, LOG_ERR,  LOC + "Failed to switch to this input.");
        }

        m_powerOffTV  = false;
        m_powerOnTV   = false;
        m_switchInput = false;
    }

    ICECAdapter   *m_adapter            {nullptr};
    ICECCallbacks  m_callbacks;
    bool           m_valid              {false};
    bool           m_powerOffTV         {false};
    bool           m_powerOffTVAllowed  {false};
    bool           m_powerOffTVOnExit   {false};
    bool           m_powerOnTV          {false};
    bool           m_powerOnTVAllowed   {false};
    bool           m_powerOnTVOnStart   {false};
    bool           m_switchInput        {false};
    bool           m_switchInputAllowed {true};
};

CECAdapter::CECAdapter() : MThread("CECAdapter"), m_priv(new CECAdapterPriv)
{
    QMutexLocker lock(gLock);

    // don't try if disabled
    if (!gCoreContext->GetBoolSetting(LIBCEC_ENABLED, true))
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
    RunProlog();
    // Handle any actions at startup
    // This is done outside the lock to handle initial setup -
    // we know that nothing else can be calling us this early.
    m_priv->HandleActions();

    while (IsValid()) {
        // Note that a lock is used because the QWaitCondition needs it
        // None of the other HandleActions callers need the lock because
        // they call HandleActions at open/close time, when
        // nothing else can be calling it....
        gHandleActionsLock->lock();
        gActionsReady->wait(gHandleActionsLock);
        if (IsValid()) {
            m_priv->HandleActions();
        }
        gHandleActionsLock->unlock();
    }
    RunEpilog();
}

CECAdapter::~CECAdapter()
{
    QMutexLocker lock(gLock);

    // delete the actual adapter
    m_priv->Close();
    // Free the thread
    gActionsReady->wakeAll();
    // Wait for it to end
    wait();
}

bool CECAdapter::IsValid(void)
{
    return m_priv->m_valid;
}

void CECAdapter::Action(const QString &action)
{
    QMutexLocker lock(gLock);
    if (ACTION_TVPOWERON == action)
        m_priv->m_powerOnTV = true;
    else if (ACTION_TVPOWEROFF == action)
        m_priv->m_powerOffTV = true;
    gActionsReady->wakeAll();
}

#if CEC_LIB_VERSION_MAJOR <= 3
// cppcheck-suppress passedByValue
static int CECLogMessageCallback(void *adapter, const cec_log_message CEC_CALLBACK_PARAM_TYPE message)
{
    return ((CECAdapterPriv*)adapter)->LogMessage(message);
}

// cppcheck-suppress passedByValue
static int CECKeyPressCallback(void *adapter, const cec_keypress CEC_CALLBACK_PARAM_TYPE keypress)
{
    return ((CECAdapterPriv*)adapter)->HandleKeyPress(keypress);
}

// cppcheck-suppress passedByValue
static int CECCommandCallback(void *adapter, const cec_command CEC_CALLBACK_PARAM_TYPE command)
{
    return ((CECAdapterPriv*)adapter)->HandleCommand(command);
}
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
static void CECLogMessageCallback(void *adapter, const cec_log_message* message)
{
    Q_UNUSED(adapter);
    CECAdapterPriv::LogMessage(message);
}

static void CECKeyPressCallback(void *adapter, const cec_keypress* keypress)
{
    ((CECAdapterPriv*)adapter)->HandleKeyPress(keypress);
}

static void CECCommandCallback(void *adapter, const cec_command* command)
{
    ((CECAdapterPriv*)adapter)->HandleCommand(command);
}
#endif

#if CEC_LIB_VERSION_MAJOR >= 2
#if CEC_LIB_VERSION_MAJOR <= 3
// cppcheck-suppress passedByValue
static int CECAlertCallback(void *adapter, const libcec_alert alert, const libcec_parameter CEC_CALLBACK_PARAM_TYPE data)
{
    return ((CECAdapterPriv*)adapter)->HandleAlert(alert, data);
}
#endif
#if CEC_LIB_VERSION_MAJOR >= 4
static void CECAlertCallback(void *adapter, const libcec_alert alert, const libcec_parameter data)
{
    Q_UNUSED(adapter);
    CECAdapterPriv::HandleAlert(alert, data);
}
#endif
static void CECSourceActivatedCallback(void *adapter, const cec_logical_address address, const uint8_t activated)
{
    ((CECAdapterPriv*)adapter)->HandleSourceActivated(address, activated);
}
#endif
