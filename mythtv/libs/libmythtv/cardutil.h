// -*- Mode: c++ -*-
#ifndef CARDUTIL_H
#define CARDUTIL_H

// C++ headers
#include <chrono>
#include <cstdint>
#include <vector>

// Qt headers
#include <QList>
#include <QStringList>
#include <QMap>
#include <QMetaEnum>

// MythTV headers
#include "mythtvexp.h"
#include "dtvconfparserhelpers.h"

class InputInfo;
class CardInput;
using InputNames = QMap<int,QString>;

MTV_PUBLIC QString get_on_input(const QString &to_get, uint inputid);

MTV_PUBLIC bool set_on_input(const QString &to_set, uint inputid, const QString &value);

enum dvb_dev_type_t : std::uint8_t
{
    DVB_DEV_FRONTEND = 1,
    DVB_DEV_DVR,
    DVB_DEV_DEMUX,
    DVB_DEV_CA,
    DVB_DEV_AUDIO,
    DVB_DEV_VIDEO,
};

/** \class CardUtil
 *  \brief Collection of helper utilities for input DB use
 */
class MTV_PUBLIC CardUtil : public QObject
{
    Q_OBJECT

  public:
    using InputTypes = QMap<QString, QString>;

    /// \brief all the different inputs
    enum class INPUT_TYPES : std::uint8_t
    {
        ERROR_OPEN    = 0,
        ERROR_UNKNOWN = 1,
        ERROR_PROBE   = 2,
        QPSK      = 3,        DVBS      = 3,
        QAM       = 4,        DVBC      = 4,
        OFDM      = 5,        DVBT      = 5,
        ATSC      = 6,
        V4L       = 7,
        MPEG      = 8,
        FIREWIRE  = 9,
        HDHOMERUN = 10,
        FREEBOX   = 11,
        HDPVR     = 12,
        DVBS2     = 13,
        IMPORT    = 14,
        DEMO      = 15,
        ASI       = 16,
        CETON     = 17,
        EXTERNAL  = 18,
        VBOX      = 19,
        DVBT2     = 20,
        V4L2ENC   = 21,
        SATIP     = 22
    };

    Q_ENUM(INPUT_TYPES)

    static INPUT_TYPES toInputType(const QString &name)
    {
        if ("ERROR_OPEN" == name)
            return INPUT_TYPES::ERROR_OPEN;
        if ("ERROR_UNKNOWN" == name)
            return INPUT_TYPES::ERROR_UNKNOWN;
        if ("ERROR_PROBE" == name)
            return INPUT_TYPES::ERROR_PROBE;
        if ("QPSK" == name)
            return INPUT_TYPES::QPSK;
        if ("DVBS" == name)
            return INPUT_TYPES::DVBS;
        if ("QAM" == name)
            return INPUT_TYPES::QAM;
        if ("DVBC" == name)
            return INPUT_TYPES::DVBC;
        if ("OFDM" == name)
            return INPUT_TYPES::OFDM;
        if ("DVBT" == name)
            return INPUT_TYPES::DVBT;
        if ("ATSC" == name)
            return INPUT_TYPES::ATSC;
        if ("V4L" == name)
            return INPUT_TYPES::V4L;
        if ("MPEG" == name)
            return INPUT_TYPES::MPEG;
        if ("FIREWIRE" == name)
            return INPUT_TYPES::FIREWIRE;
        if ("HDHOMERUN" == name)
            return INPUT_TYPES::HDHOMERUN;
        if ("FREEBOX" == name)
            return INPUT_TYPES::FREEBOX;
        if ("HDPVR" == name)
            return INPUT_TYPES::HDPVR;
        if ("DVB_S2" == name)
            return INPUT_TYPES::DVBS2;
        if ("IMPORT" == name)
            return INPUT_TYPES::IMPORT;
        if ("DEMO" == name)
            return INPUT_TYPES::DEMO;
        if ("ASI" == name)
            return INPUT_TYPES::ASI;
        if ("CETON" == name)
            return INPUT_TYPES::CETON;
        if ("EXTERNAL" == name)
            return INPUT_TYPES::EXTERNAL;
        if ("VBOX" == name)
            return INPUT_TYPES::VBOX;
        if ("DVB_T2" == name)
            return INPUT_TYPES::DVBT2;
        if ("V4L2ENC" == name)
            return INPUT_TYPES::V4L2ENC;
        if ("SATIP" == name)
            return INPUT_TYPES::SATIP;
        return INPUT_TYPES::ERROR_UNKNOWN;
    }

    static bool         IsEncoder(const QString &rawtype)
    {
        return
            (rawtype != "DVB")       && (rawtype != "FIREWIRE") &&
            (rawtype != "HDHOMERUN") && (rawtype != "FREEBOX")  &&
            (rawtype != "IMPORT")    && (rawtype != "DEMO")     &&
            (rawtype != "ASI")       && (rawtype != "CETON")    &&
            (rawtype != "VBOX")      && (rawtype != "SATIP");
    }

    static bool         IsV4L(const QString &rawtype)
    {
        return (rawtype == "V4L"   || rawtype == "MPEG"      ||
                rawtype == "HDPVR" || rawtype == "GO7007"    ||
                rawtype == "MJPEG" || rawtype == "V4L2ENC");
    }

    static bool         IsChannelChangeDiscontinuous(const QString &rawtype)
    {
        return (!IsEncoder(rawtype) || rawtype == "HDPVR" ||
                rawtype == "EXTERNAL");
    }

    static bool         IsUnscanable(const QString &rawtype)
    {
        return
            (rawtype == "FIREWIRE")  || (rawtype == "HDPVR") ||
            (rawtype == "IMPORT")    || (rawtype == "DEMO")  ||
            (rawtype == "GO7007")    || (rawtype == "MJPEG");
    }
    static QString      GetScanableInputTypes(void);

    static bool         IsCableCardPresent(uint inputid,
                                           const QString &inputType);

    static bool         IsEITCapable(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN") ||
            (rawtype == "SATIP");
    }

    static bool         IsTunerSharingCapable(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN") ||
            (rawtype == "ASI")       || (rawtype == "FREEBOX")   ||
            (rawtype == "CETON")     || (rawtype == "EXTERNAL")  ||
            (rawtype == "VBOX")      || (rawtype == "V4L2ENC")   ||
            (rawtype == "SATIP");
    }

    static bool         HasTuner(const QString &rawtype, const QString & device);
    static bool         IsTunerShared(uint inputidA, uint inputidB);

    static bool         IsTuningDigital(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN") ||
            (rawtype == "ASI")       || (rawtype == "CETON")     ||
            (rawtype == "EXTERNAL")  || (rawtype == "SATIP");
    }

    static bool         IsTuningAnalog(const QString &rawtype)
    {
        return
            (rawtype == "V4L")       || (rawtype == "V4L2ENC"    ||
             rawtype == "MPEG");
    }

    static bool         IsTuningVirtual(const QString &rawtype)
    {
        return
            (rawtype == "FIREWIRE")  || (rawtype == "HDPVR") ||
            (rawtype == "EXTERNAL")  || (rawtype == "V4L2ENC");
    }

    static bool         IsSingleInputType(const QString &rawtype)
    {
        return
            (rawtype == "FIREWIRE")  || (rawtype == "HDHOMERUN") ||
            (rawtype == "FREEBOX")   || (rawtype == "ASI")       ||
            (rawtype == "IMPORT")    || (rawtype == "DEMO")      ||
            (rawtype == "CETON")     || (rawtype == "EXTERNAL")  ||
            (rawtype == "VBOX")      || (rawtype == "SATIP");
    }

    static bool         IsChannelReusable(const QString &rawtype)
    {
        return !(rawtype == "FREEBOX" || rawtype == "VBOX");
    }

#ifdef USING_VBOX
    static bool         IsVBoxPresent(uint inputid);
#endif
#ifdef USING_SATIP
    static bool         IsSatIPPresent(uint inputid);
#endif

    // Card creation and deletion

    static int          CreateCaptureCard(const QString &videodevice,
                                          const QString &audiodevice,
                                          const QString &vbidevice,
                                          const QString &inputtype,
                                          uint audioratelimit,
                                          const QString &hostname,
                                          uint dvb_swfilter,
                                          uint dvb_sat_type,
                                          bool       dvb_wait_for_seqstart,
                                          bool       skipbtaudio,
                                          bool       dvb_on_demand,
                                          uint dvb_diseqc_type,
                                          uint firewire_speed,
                                          const QString &firewire_model,
                                          uint firewire_connection,
                                          std::chrono::milliseconds signal_timeout,
                                          std::chrono::milliseconds channel_timeout,
                                          uint dvb_tuning_delay,
                                          uint contrast,
                                          uint brightness,
                                          uint colour,
                                          uint hue,
                                          uint diseqcid,
                                          bool       dvb_eitscan);

    static bool         DeleteInput(uint inputid);
    static bool         DeleteAllInputs(void);
    static std::vector<uint> GetInputList(void);
    static std::vector<uint> GetSchedInputList(void);
    static std::vector<uint> GetLiveTVInputList(void);

    /// Convenience function for GetInputIDs()
    static uint         GetFirstInputID(const QString &videodevice)
    {
        std::vector<uint> list = GetInputIDs(videodevice);
        if (list.empty())
            return 0;
        return list[0];
    }

    static std::vector<uint> GetInputIDs(const QString& videodevice = QString(),
                                    const QString& rawtype     = QString(),
                                    const QString& inputname   = QString(),
                                    QString hostname    = QString());

    static uint         GetChildInputCount(uint inputid);
    static std::vector<uint> GetChildInputIDs(uint inputid);

    static bool         IsInputTypePresent(const QString &rawtype,
                                           QString hostname = QString());
    static InputTypes   GetInputTypes(void); // input types on ALL hosts
    static QStringList  GetInputTypeNames(uint sourceid); // input types for a given source id

    static QStringList  GetVideoDevices(const QString &rawtype,
                                        QString hostname = QString());

    static QString      GetRawInputType(uint inputid)
        { return get_on_input("cardtype", inputid).toUpper(); }
    static QString      GetVideoDevice(uint inputid)
        { return get_on_input("videodevice", inputid); }
    static QString      GetAudioDevice(uint inputid)
        { return get_on_input("audiodevice", inputid); }
    static QString      GetVBIDevice(uint inputid)
        { return get_on_input("vbidevice", inputid); }
    static QString      GetDeliverySystemFromDB(uint inputid)
        { return get_on_input("inputname", inputid); }          // use capturecard/inputname for now
    static QString      GetDiSEqCPosition(uint inputid)
        { return get_on_input("dvb_diseqc_type", inputid); }    // use capturecard/dvb_diseqc_type for now

    static QString      GetHostname(uint inputid)
        { return get_on_input("hostname", inputid); }

    static int          GetValueInt(const QString &col, uint inputid)
        { return get_on_input(col, inputid).toInt(); }
    static bool         SetValue(const QString &col, uint inputid,
                                 int val)
        { return set_on_input(col, inputid, QString::number(val)); }
    static bool         SetValue(const QString &col, uint inputid,
                                 const QString &val)
        { return set_on_input(col, inputid, val); }

    static bool         SetStartChannel(uint inputid,
                                        const QString &channum);

    // Input creation and deletion
    static int           CreateCardInput(uint inputid,
                                         uint sourceid,
                                         const QString &inputname,
                                         const QString &externalcommand,
                                         const QString &changer_device,
                                         const QString &changer_model,
                                         const QString &hostname,
                                         const QString &tunechan,
                                         const QString &startchan,
                                         const QString &displayname,
                                         bool  dishnet_eit,
                                         uint recpriority,
                                         uint quicktune,
                                         uint schedorder,
                                         uint livetvorder);

    // Other input functions

    static std::vector<uint> GetInputIDs(uint sourceid);
    static bool         GetInputInfo(InputInfo &input,
                                     std::vector<uint> *groupids = nullptr);
    static QList<InputInfo> GetAllInputInfo(bool virtTuners);
    static QString      GetInputName(uint inputid);
    static QString      GetStartChannel(uint inputid);
    static QString      GetDisplayName(uint inputid);
    static bool         IsUniqueDisplayName(const QString &name,
                                            uint exclude_inputid);
    static uint         GetSourceID(uint inputid);

    // Input Groups
    static uint         CreateInputGroup(const QString &name);
    static uint         CreateDeviceInputGroup(uint inputid,
                                               const QString &type,
                                               const QString &host,
                                               const QString &device);
    static uint         GetDeviceInputGroup(uint inputid);
    static bool         LinkInputGroup(uint inputid, uint inputgroupid);
    static bool         UnlinkInputGroup(uint inputid, uint inputgroupid);
    static std::vector<uint> GetInputGroups(uint inputid);
    static std::vector<uint> GetGroupInputIDs(uint inputgroupid);
    static std::vector<uint> GetConflictingInputs(uint inputid);

    static QString      GetDeviceLabel(const QString &inputtype,
                                       const QString &videodevice);
    static QString      GetDeviceLabel(uint inputid);

    static QString      ProbeSubTypeName(uint inputid);

    static QStringList  ProbeVideoInputs(const QString& device,
                                         const QString& inputtype = QString());
    static QStringList  ProbeAudioInputs(const QString& device,
                                         const QString& inputtype = QString());
    static void         GetDeviceInputNames(const QString      &device,
                                            const QString      &inputtype,
                                            QStringList        &inputs);

    // General info from OS
    static QStringList  ProbeVideoDevices(const QString &rawtype);
    static void ClearVideoDeviceCache();

    // Other
    static uint         CloneCard(uint src_inputid, uint dst_inputid);
    static bool         InputSetMaxRecordings(uint parentid,
                                              uint max_recordings);
    static uint         AddChildInput(uint parentid);
    static QString      GetFirewireChangerNode(uint inputid);
    static QString      GetFirewireChangerModel(uint inputid);

    // DTV info
    static bool         GetTimeouts(uint inputid,
                                    std::chrono::milliseconds &signal_timeout,
                                    std::chrono::milliseconds &channel_timeout);
    static bool         IgnoreEncrypted(uint inputid,
                                        const QString &inputname);
    static bool         TVOnly(uint inputid, const QString &inputname);
    static bool         IsInNeedOfExternalInputConf(uint inputid);
    static uint         GetQuickTuning(uint inputid, const QString &input_name);

    // DVB info
    /// \brief Returns true if the input is a DVB input
    static bool         IsDVB(uint inputid)
        { return "DVB" == GetRawInputType(inputid); }
    static bool         IsDVBInputType(const QString &inputType);
    static QStringList  ProbeDeliverySystems(const QString &device);
    static QStringList  ProbeDeliverySystems(int fd_frontend);
    static QString      ProbeDefaultDeliverySystem(const QString &device);
    static QString      ProbeDVBType(const QString &device);
    static QString      ProbeDVBFrontendName(const QString &device);
    static bool         HasDVBCRCBug(const QString &device);
    static std::chrono::milliseconds GetMinSignalMonitoringDelay(const QString &device);
    static DTVTunerType ConvertToTunerType(DTVModulationSystem delsys);
    static DTVTunerType GetTunerType(uint inputid);
    static DTVTunerType ProbeTunerType(int fd_frontend);
    static DTVTunerType ProbeTunerType(const QString &device);
    static DTVTunerType GetTunerTypeFromMultiplex(uint mplexid);
    static DTVModulationSystem GetDeliverySystem(uint inputid);
    static DTVModulationSystem ProbeCurrentDeliverySystem(const QString &device);
    static DTVModulationSystem ProbeCurrentDeliverySystem(int fd_frontend);
    static DTVModulationSystem ProbeBestDeliverySystem(int fd);
    static DTVModulationSystem GetOrProbeDeliverySystem(uint inputid, int fd);
    static int          SetDefaultDeliverySystem(uint inputid, int fd);
    static int          SetDeliverySystem(uint inputid);
    static int          SetDeliverySystem(uint inputid, DTVModulationSystem delsys);
    static int          SetDeliverySystem(uint inputid, int fd);
    static int          SetDeliverySystem(uint inputid, DTVModulationSystem delsys, int fd);
    static int          OpenVideoDevice(int inputid);
    static int          OpenVideoDevice(const QString &device);
    static QString      GetDeviceName(dvb_dev_type_t type, const QString &device);
    static InputNames   GetConfiguredDVBInputs(const QString &device);
    static QStringList  CapabilitiesToString(uint64_t capabilities);

    // V4L info
    static bool         hasV4L2(int videofd);
    static bool         GetV4LInfo(int videofd, QString &input, QString &driver,
                                   uint32_t &version, uint32_t &capabilities);
    static bool         GetV4LInfo(int videofd, QString &input, QString &driver)
        {
            uint32_t d1 = 0;
            uint32_t d2 = 0;
            return GetV4LInfo(videofd, input, driver, d1, d2);
        }
    static InputNames   ProbeV4LVideoInputs(int videofd, bool &ok);
    static InputNames   ProbeV4LAudioInputs(int videofd, bool &ok);

    // HDHomeRun info
    static bool         HDHRdoesDVB(const QString &device);
    static bool         HDHRdoesDVBC(const QString &device);
    static QString      GetHDHRdesc(const QString &device);

    // VBox info
    static QString      GetVBoxdesc(const QString &id, const QString &ip,
                                    const QString &tunerNo, const QString &tunerType);

    // ASI info
    static int          GetASIDeviceNumber(const QString &device,
                                           QString *error = nullptr);

    static uint         GetASIBufferSize(uint device_num,
                                         QString *error = nullptr);
    static uint         GetASINumBuffers(uint device_num,
                                         QString *error = nullptr);
    static int          GetASIMode(uint device_num,
                                   QString *error = nullptr);
    static bool         SetASIMode(uint device_num, uint mode,
                                   QString *error = nullptr);

  private:
    static QStringList  ProbeV4LVideoInputs(const QString& device);
    static QStringList  ProbeV4LAudioInputs(const QString& device);
    static QStringList  ProbeDVBInputs(const QString& device);
    static QMap <QString,QStringList> s_videoDeviceCache;
};

#endif //CARDUTIL_H
