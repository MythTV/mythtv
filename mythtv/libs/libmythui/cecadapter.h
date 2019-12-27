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
using namespace std;

class MythCECAdapter
{
  public:
    enum MythCECAction
    {
        None        = 0x00,
        PowerOffTV  = 0x01,
        PowerOnTV   = 0x02,
        SwitchInput = 0x04
    };

    Q_DECLARE_FLAGS(MythCECActions, MythCECAction)

#if CEC_LIB_VERSION_MAJOR <= 3
    static int  LogMessageCallback(void*, const cec_log_message&);
    static int  KeyPressCallback  (void*, const cec_keypress&);
    static int  CommandCallback   (void*, const cec_command&);
    static int  AlertCallback     (void*, const libcec_alert, const libcec_parameter&);
#else
    static void LogMessageCallback(void*, const cec_log_message*);
    static void KeyPressCallback  (void*, const cec_keypress*);
    static void CommandCallback   (void*, const cec_command*);
    static void AlertCallback     (void*, const libcec_alert, const libcec_parameter);
#endif
    static void SourceCallback    (void*, const cec_logical_address, const uint8_t);
    static QString AddressToString(int Address);

    MythCECAdapter() = default;
   ~MythCECAdapter();
    void        Open          (void);
    void        Close         (void);
    void        Action        (const QString &Action);

  protected:
    void        HandleActions (MythCECActions Actions);
    static int  HandleCommand (const cec_command &Command);
    static int  HandleKeyPress(const cec_keypress &Key);
    static void HandleSource  (const cec_logical_address Address, const uint8_t Activated);
    static int  LogMessage    (const cec_log_message &Message);
    static int  HandleAlert   (const libcec_alert Alert, const libcec_parameter &Data);

  protected:
    ICECAdapter   *m_adapter            { nullptr };
    ICECCallbacks  m_callbacks;
    bool           m_valid              { false };
    bool           m_powerOffTVAllowed  { false };
    bool           m_powerOffTVOnExit   { false };
    bool           m_powerOnTVAllowed   { false };
    bool           m_powerOnTVOnStart   { false };
    bool           m_switchInputAllowed { true  };
};
#endif

