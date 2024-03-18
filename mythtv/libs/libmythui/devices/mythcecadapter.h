#ifndef CECADAPTER_H_
#define CECADAPTER_H_

// Qt
#include <QObject>

#define LIBCEC_ENABLED     QString("libCECEnabled")
#define LIBCEC_DEVICE      QString("libCECDevice")
#define LIBCEC_BASE        QString("libCECBase")
#define LIBCEC_PORT        QString("libCECPort")
#define POWEROFFTV_ALLOWED QString("PowerOffTVAllowed")
#define POWEROFFTV_ONEXIT  QString("PowerOffTVOnExit")
#define POWERONTV_ALLOWED  QString("PowerOnTVAllowed")
#define POWERONTV_ONSTART  QString("PowerOnTVOnStart")

// libcec
#include <libcec/cec.h>
#include <iostream>
using namespace CEC;

class MythMainWindow;

class MythCECAdapter
{
  public:
    enum MythCECAction : std::uint8_t
    {
        None        = 0x00,
        PowerOffTV  = 0x01,
        PowerOnTV   = 0x02,
        SwitchInput = 0x04
    };

    Q_DECLARE_FLAGS(MythCECActions, MythCECAction)

#if CEC_LIB_VERSION_MAJOR <= 3
    static int  LogMessageCallback(void* /*unused*/, const cec_log_message Message);
    static int  KeyPressCallback  (void* Adapter,    const cec_keypress Keypress);
    static int  CommandCallback   (void* Adapter,    const cec_command Command);
    static int  AlertCallback     (void* /*unused*/, const libcec_alert Alert, const libcec_parameter Data);
#else
    static void LogMessageCallback(void* /*unused*/, const cec_log_message* Message);
    static void KeyPressCallback  (void* Adapter,    const cec_keypress* Keypress);
    static void CommandCallback   (void* Adapter,    const cec_command* Command);
    static void AlertCallback     (void* /*unused*/, libcec_alert Alert, libcec_parameter Data);
#endif
    static void SourceCallback    (void* /*unused*/, cec_logical_address Address, uint8_t Activated);
    static QString AddressToString(int Address);

    MythCECAdapter() = default;
   ~MythCECAdapter();
    void        Open          (MythMainWindow* Window);
    void        Close         (void);
    void        Action        (const QString &Action);
    void        IgnoreKeys    (bool Ignore);

  protected:
    void        HandleActions (MythCECActions Actions);
    int         HandleCommand (const cec_command &Command);
    int         HandleKeyPress(cec_keypress Key) const;
    static void HandleSource  (cec_logical_address Address, uint8_t Activated);
    static int  LogMessage    (const cec_log_message &Message);
    static int  HandleAlert   (libcec_alert Alert, libcec_parameter Data);

  protected:
    ICECAdapter   *m_adapter            { nullptr };
    ICECCallbacks  m_callbacks;
    bool           m_valid              { false };
    bool           m_ignoreKeys         { false };
    bool           m_powerOffTVAllowed  { false };
    bool           m_powerOffTVOnExit   { false };
    bool           m_powerOnTVAllowed   { false };
    bool           m_powerOnTVOnStart   { false };
    bool           m_switchInputAllowed { true  };
};
#endif

