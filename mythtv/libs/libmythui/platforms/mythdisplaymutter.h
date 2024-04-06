#ifndef MYTHMUTTERDISPLAYCONFIG_H
#define MYTHMUTTERDISPLAYCONFIG_H

// Qt
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QObject>

// MythTV
#include "mythdisplay.h"

#define DISP_CONFIG_SERVICE (QString("org.gnome.Mutter.DisplayConfig"))
#define DISP_CONFIG_PATH    (QString("/org/gnome/Mutter/DisplayConfig"))
#define DISP_CONFIG_SIG     (QString("ua(uxiiiiiuaua{sv})a(uxiausauaua{sv})a(uxuudu)ii"))

using MythMutterMap        = QMap<QString,QDBusVariant>;
using MythMutterProperty   = QPair<QString,QDBusVariant>;
using MythMutterProperties = QList<MythMutterProperty>;

// GetResources CRTCs signature a(uxiiiiiuaua{sv})
struct MythMutterCRTC
{
    uint32_t id                     {};
    qint64   sys_id                 {}; // N.B. needs to be a Qt type here
    int32_t  x                      {};
    int32_t  y                      {};
    int32_t  width                  {};
    int32_t  height                 {};
    int32_t  currentmode            {};
    uint32_t currenttransform       {};
    QList<uint32_t> transforms;
    MythMutterProperties properties;
};

// GetResources Outputs signature a(uxiausauaua{sv})
struct MythMutterOutput
{
    uint32_t id                     {};
    qint64   sys_id                 {};
    int32_t  current_crtc           {};
    QList<uint32_t> possible_crtcs;
    QString  name;
    QList<uint32_t> modes;
    QList<uint32_t> clones;
    MythMutterProperties properties;

    // MythTV properties
    QString serialnumber;
    QByteArray edid;
    int     widthmm                 {};
    int     heightmm                {};
};

// GetResources Modes signature a(uxuudu)
struct MythMutterMode
{
    uint32_t  id;
    qint64    sys_id;
    uint32_t  width;
    uint32_t  height;
    double    frequency;
    uint32_t  flags;
};

using MythMutterCRTCList = QList<MythMutterCRTC>;
Q_DECLARE_METATYPE(MythMutterCRTC);
Q_DECLARE_METATYPE(MythMutterCRTCList);
using MythMutterOutputList = QList<MythMutterOutput>;
Q_DECLARE_METATYPE(MythMutterOutput);
Q_DECLARE_METATYPE(MythMutterOutputList);
using MythMutterModeList = QList<MythMutterMode>;
Q_DECLARE_METATYPE(MythMutterMode);
Q_DECLARE_METATYPE(MythMutterModeList);

class MythDisplayMutter : public MythDisplay
{
    Q_OBJECT

  public:
    static MythDisplayMutter* Create();
    ~MythDisplayMutter() override;

    void UpdateCurrentMode   () override;
    bool VideoModesAvailable () override { return true; }
    bool UsingVideoModes     () override;
    const MythDisplayModes& GetVideoModes(void) override;
    bool SwitchToVideoMode   (QSize Size, double DesiredRate) override;

  public slots:
    void MonitorsChanged();

  private:
    MythDisplayMutter();
    bool IsValid();
    void InitialiseInterface();
    void UpdateResources();

    QDBusInterface*      m_interface { nullptr };
    uint32_t             m_serialVal { 0       };
    MythMutterCRTCList   m_crtcs;
    MythMutterOutputList m_outputs;
    MythMutterModeList   m_modes;
    int                  m_outputIdx { -1      };
    QMap<uint64_t, uint32_t> m_modeMap;
};

#endif
