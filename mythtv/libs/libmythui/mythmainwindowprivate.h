#ifndef MYTHMAINWINDOWPRIVATE_H
#define MYTHMAINWINDOWPRIVATE_H

// MythTV
#include "mythconfig.h"
#include "mythmainwindow.h"
#include "mythgesture.h"

class MythScreenStack;
class MythSignalingTimer;
class MythThemeBase;
class MythUDPListener;
class MythMediaDevice;

class KeyContext
{
  public:
    void AddMapping(int key, const QString& action)
    {
        m_actionMap[key].append(action);
    }

    bool GetMapping(int key, QStringList &actions)
    {
        if (m_actionMap.count(key) > 0)
        {
            actions += m_actionMap[key];
            return true;
        }
        return false;
    }

    QMap<int, QStringList> m_actionMap;
};

// Adding member initializers caused compilation to fail with an error
// that it cannot convert a brace-enclosed initializer list to JumpData.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct JumpData
{
    void (*m_callback)(void);
    QString m_destination;
    QString m_description;
    bool    m_exittomain;
    QString m_localAction;
};

using MythMediaCallback = QPair<QString, MediaPlayCallback>;

class MythMainWindowPrivate
{
    friend class MythMainWindow;

  protected:
    MythMainWindowPrivate() = default;

    static int TranslateKeyNum(QKeyEvent *Event);

    float                m_wmult                { 1.0F    };
    float                m_hmult                { 1.0F    };
    QRect                m_screenRect;
    QRect                m_uiScreenRect;
    bool                 m_doesFillScreen       { false   };
    bool                 m_exitingtomain        { false   };
    bool                 m_popwindows           { false   };
    /// To allow or prevent database access
    bool                 m_useDB                { true    };

    QHash<QString, KeyContext *> m_keyContexts;
    QMap<int, JumpData*> m_jumpMap;
    QMap<QString, JumpData> m_destinationMap;
    QMap<QString, MythMediaCallback> m_mediaPluginMap;
    QHash<QString, QHash<QString, QString> > m_actionText;

    void (*m_exitMenuCallback)(void) { nullptr };
    void (*m_exitMenuMediaDeviceCallback)(MythMediaDevice* mediadevice) { nullptr };
    MythMediaDevice * m_mediaDeviceForCallback { nullptr };

    int      m_escapekey                 { 0       };
    int      m_drawInterval              { 1000 / MythMainWindow::drawRefresh };
    MythSignalingTimer *m_drawTimer      { nullptr };
    QVector<MythScreenStack *> m_stackList;
    MythScreenStack *m_mainStack         { nullptr };
    MythGesture      m_gesture;
    QTimer          *m_gestureTimer      { nullptr };
    QTimer          *m_hideMouseTimer    { nullptr };
    QMutex           m_drawDisableLock;
    uint             m_drawDisabledDepth { 0       };
    bool             m_drawEnabled       { true    };
    MythThemeBase   *m_themeBase         { nullptr };
    MythUDPListener *m_udpListener       { nullptr };
    MythNotificationCenter *m_nc         { nullptr };
    QTimer          *m_idleTimer         { nullptr };
    int              m_idleTime          { 0       };
    bool             m_standby           { false   };
    bool             m_enteringStandby   { false   };
    bool             m_disableIdle       { false   };
    bool             m_allowInput        { true    };
    bool             m_pendingUpdate     { false   };
    // window aspect
    bool             m_firstinit         { true    };
    // Support for long press
    int              m_longPressKeyCode  { 0       };
    ulong            m_longPressTime     { 0       };
};
#endif
