// -*- Mode: c++ -*-
#ifndef _CARDUTIL_H_
#define _CARDUTIL_H_

// ANSI C
#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QList>
#include <QStringList>
#include <QMap>

// MythTV headers
#include "settings.h"
#include "mythtvexp.h"

class InputInfo;
class CardInput;
typedef QMap<int,QString> InputNames;

MTV_PUBLIC QString get_on_input(const QString&, uint);

MTV_PUBLIC bool set_on_input(const QString&, uint, const QString&);

typedef enum
{
    DVB_DEV_FRONTEND = 1,
    DVB_DEV_DVR,
    DVB_DEV_DEMUX,
    DVB_DEV_CA,
    DVB_DEV_AUDIO,
    DVB_DEV_VIDEO,
} dvb_dev_type_t;

/** \class CardUtil
 *  \brief Collection of helper utilities for input DB use
 */
class MTV_PUBLIC CardUtil
{
  public:
    typedef QMap<QString, QString> InputTypes;

    /// \brief all the different inputs
    enum INPUT_TYPES
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
    };

    static enum INPUT_TYPES toInputType(const QString &name)
    {
        if ("ERROR_OPEN" == name)
            return ERROR_OPEN;
        if ("ERROR_UNKNOWN" == name)
            return ERROR_UNKNOWN;
        if ("ERROR_PROBE" == name)
            return ERROR_PROBE;
        if ("QPSK" == name)
            return QPSK;
        if ("QAM" == name)
            return QAM;
        if ("OFDM" == name)
            return OFDM;
        if ("ATSC" == name)
            return ATSC;
        if ("V4L" == name)
            return V4L;
        if ("MPEG" == name)
            return MPEG;
        if ("FIREWIRE" == name)
            return FIREWIRE;
        if ("HDHOMERUN" == name)
            return HDHOMERUN;
        if ("FREEBOX" == name)
            return FREEBOX;
        if ("HDPVR" == name)
            return HDPVR;
        if ("DVB_S2" == name)
            return DVBS2;
        if ("IMPORT" == name)
            return IMPORT;
        if ("DEMO" == name)
            return DEMO;
        if ("ASI" == name)
            return ASI;
        if ("CETON" == name)
            return CETON;
        if ("EXTERNAL" == name)
            return EXTERNAL;
        if ("VBOX" == name)
            return VBOX;
        if ("DVB_T2" == name)
            return DVBT2;
        if ("V4L2ENC" == name)
            return V4L2ENC;
        return ERROR_UNKNOWN;
    }

    static bool         IsEncoder(const QString &rawtype)
    {
        return
            (rawtype != "DVB")       && (rawtype != "FIREWIRE") &&
            (rawtype != "HDHOMERUN") && (rawtype != "FREEBOX")  &&
            (rawtype != "IMPORT")    && (rawtype != "DEMO")     &&
            (rawtype != "ASI")       && (rawtype != "CETON")    &&
            (rawtype != "VBOX");
    }

    static bool         IsV4L(const QString &rawtype)
    {
        return (rawtype == "V4L"   || rawtype == "MPEG"      ||
                rawtype == "HDPVR" || rawtype == "GO7007"    ||
                rawtype == "MJPEG" || rawtype == "V4L2ENC");
    }

    static bool         IsChannelChangeDiscontinuous(const QString &rawtype)
    {
        return !IsEncoder(rawtype) || (rawtype == "HDPVR");
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
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN");
    }

    static bool         IsTunerSharingCapable(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN") ||
            (rawtype == "ASI")       || (rawtype == "FREEBOX")   ||
            (rawtype == "CETON")     || (rawtype == "EXTERNAL")  ||
            (rawtype == "V4L2ENC");
    }

    static bool         HasTuner(const QString &rawtype, const QString & device);
    static bool         IsTunerShared(uint inputidA, uint inputidB);

    static bool         IsTuningDigital(const QString &rawtype)
    {
        return
            (rawtype == "DVB")       || (rawtype == "HDHOMERUN") ||
            (rawtype == "ASI")       || (rawtype == "CETON")     ||
            (rawtype == "EXTERNAL");
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
            (rawtype == "VBOX");
    }

    static bool         IsChannelReusable(const QString &rawtype)
    {
        return !(rawtype == "FREEBOX" || rawtype == "VBOX");
    }



    // Card creation and deletion

    static int          CreateCaptureCard(const QString &videodevice,
                                          const QString &audiodevice,
                                          const QString &vbidevice,
                                          const QString &inputtype,
                                          const uint audioratelimit,
                                          const QString &hostname,
                                          const uint dvb_swfilter,
                                          const uint dvb_sat_type,
                                          bool       dvb_wait_for_seqstart,
                                          bool       skipbtaudio,
                                          bool       dvb_on_demand,
                                          const uint dvb_diseqc_type,
                                          const uint firewire_speed,
                                          const QString &firewire_model,
                                          const uint firewire_connection,
                                          const uint signal_timeout,
                                          const uint channel_timeout,
                                          const uint dvb_tuning_delay,
                                          const uint contrast,
                                          const uint brightness,
                                          const uint colour,
                                          const uint hue,
                                          const uint diseqcid,
                                          bool       dvb_eitscan);

    static bool         DeleteCard(uint inputid);
    static bool         DeleteAllCards(void);
    static vector<uint> GetInputList(void);
    static vector<uint> GetLiveTVInputList(void);

    /// Convenience function for GetInputIDs()
    static uint         GetFirstInputID(const QString &videodevice)
    {
        vector<uint> list = GetInputIDs(videodevice);
        if (list.empty())
            return 0;
        return list[0];
    }

    static vector<uint> GetInputIDs(QString videodevice = QString::null,
                                    QString rawtype     = QString::null,
                                    QString inputname   = QString::null,
                                    QString hostname    = QString::null);

    static uint         GetChildInputCount(uint inputid);
    static vector<uint> GetChildInputIDs(uint inputid);

    static bool         IsInputTypePresent(const QString &rawtype,
                                           QString hostname = QString::null);
    static InputTypes   GetInputTypes(void); // input types on ALL hosts

    static QStringList  GetVideoDevices(const QString &rawtype,
                                        QString hostname = QString::null);

    static QString      GetRawInputType(uint inputid)
        { return get_on_input("cardtype", inputid).toUpper(); }
    static QString      GetVideoDevice(uint inputid)
        { return get_on_input("videodevice", inputid); }
    static QString      GetAudioDevice(uint inputid)
        { return get_on_input("audiodevice", inputid); }
    static QString      GetVBIDevice(uint inputid)
        { return get_on_input("vbidevice", inputid); }

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
    static int           CreateCardInput(const uint inputid,
                                         const uint sourceid,
                                         const QString &inputname,
                                         const QString &externalcommand,
                                         const QString &changer_device,
                                         const QString &changer_model,
                                         const QString &hostname,
                                         const QString &tunechan,
                                         const QString &startchan,
                                         const QString &displayname,
                                         bool  dishnet_eit,
                                         const uint recpriority,
                                         const uint quicktune,
                                         const uint schedorder,
                                         const uint livetvorder);

    static bool         DeleteInput(uint inputid);

    // Other input functions

    static vector<uint> GetInputIDs(uint sourceid);
    static bool         GetInputInfo(InputInfo &info,
                                     vector<uint> *groupids = NULL);
    static QList<InputInfo> GetAllInputInfo();
    static QString      GetInputName(uint inputid);
    static QString      GetStartingChannel(uint inputid);
    static QString      GetDisplayName(uint inputid);
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
    static vector<uint> GetInputGroups(uint inputid);
    static vector<uint> GetGroupInputIDs(uint inputgroupid);
    static vector<uint> GetConflictingInputs(uint inputid);

    static QString      GetDeviceLabel(const QString &inputtype,
                                       const QString &videodevice);
    static QString      GetDeviceLabel(uint inputid);

    static QString      ProbeSubTypeName(uint inputid);

    static QStringList  ProbeVideoInputs(QString device,
                                         QString inputtype = QString::null);
    static QStringList  ProbeAudioInputs(QString device,
                                         QString inputtype = QString::null);
    static void         GetDeviceInputNames(uint               inputid,
                                            const QString      &device,
                                            const QString      &inputtype,
                                            QStringList        &inputs);

    // General info from OS
    static QStringList  ProbeVideoDevices(const QString &rawtype);

    // Other
    static bool         CloneCard(uint src_inputid, uint dst_inputid);
    static QString      GetFirewireChangerNode(uint inputid);
    static QString      GetFirewireChangerModel(uint inputid);

    // DTV info
    static bool         GetTimeouts(uint inputid,
                                    uint &signal_timeout,
                                    uint &channel_timeout);
    static bool         IgnoreEncrypted(uint inputid,
                                        const QString &inputname);
    static bool         TVOnly(uint inputid, const QString &inputname);
    static bool         IsInNeedOfExternalInputConf(uint inputid);
    static uint         GetQuickTuning(uint inputid, const QString &inputname);

    // DVB info
    /// \brief Returns true if the input is a DVB input
    static bool         IsDVB(uint inputid)
        { return "DVB" == GetRawInputType(inputid); }
    static bool         IsDVBInputType(const QString &input_type);
    static QString      ProbeDVBFrontendName(const QString &device);
    static QString      ProbeDVBType(const QString &device);
    static bool         HasDVBCRCBug(const QString &device);
    static uint         GetMinSignalMonitoringDelay(const QString &device);
    static QString      GetDeviceName(dvb_dev_type_t, const QString &device);
    static InputNames   GetConfiguredDVBInputs(const QString &device);

    // V4L info
    static bool         hasV4L2(int videofd);
    static bool         GetV4LInfo(int videofd, QString &input, QString &driver,
                                   uint32_t &version, uint32_t &capabilities);
    static bool         GetV4LInfo(int videofd, QString &input, QString &driver)
        { uint32_t d1,d2; return GetV4LInfo(videofd, input, driver, d1, d2); }
    static InputNames   ProbeV4LVideoInputs(int videofd, bool &ok);
    static InputNames   ProbeV4LAudioInputs(int videofd, bool &ok);

    // HDHomeRun info
    static bool         HDHRdoesDVB(const QString &device);
    static QString      GetHDHRdesc(const QString &device);

    // VBox info
    static QString      GetVBoxdesc(const QString &id, const QString &ip,
                                    const QString &tunerNo, const QString &tunerType);

    // ASI info
    static int          GetASIDeviceNumber(const QString &device,
                                           QString *error = NULL);

    static uint         GetASIBufferSize(uint device_num,
                                         QString *error = NULL);
    static uint         GetASINumBuffers(uint device_num,
                                         QString *error = NULL);
    static int          GetASIMode(uint device_num,
                                   QString *error = NULL);
    static bool         SetASIMode(uint device_num, uint mode,
                                   QString *error = NULL);

  private:
    static QStringList  ProbeV4LVideoInputs(QString device);
    static QStringList  ProbeV4LAudioInputs(QString device);
    static QStringList  ProbeDVBInputs(QString device);
};

#endif //_CARDUTIL_H_
