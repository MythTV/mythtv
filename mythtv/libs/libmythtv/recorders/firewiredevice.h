/**
 *  FirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRE_DEVICE_H
#define FIREWIRE_DEVICE_H

// C++ headers
#include <vector>

// Qt headers
#include <QMutex>
#include <QString>

// MythTV headers
#include "libmythtv/mpeg/streamlisteners.h"
#include "libmythtv/recorders/avcinfo.h"

class TSPacket;

class MTV_PUBLIC FirewireDevice
{
  public:

    // Public enums
    enum PowerState : std::uint8_t
    {
        kAVCPowerOn,
        kAVCPowerOff,
        kAVCPowerUnknown,
        kAVCPowerQueryFailed,
    };

    // AVC commands
    enum IEEE1394Command : std::uint8_t
    {
        kAVCControlCommand         = 0x00,
        kAVCStatusInquiryCommand   = 0x01,
        kAVCSpecificInquiryCommand = 0x02,
        kAVCNotifyCommand          = 0x03,
        kAVCGeneralInquiryCommand  = 0x04,

        kAVCNotImplementedStatus   = 0x08,
        kAVCAcceptedStatus         = 0x09,
        kAVCRejectedStatus         = 0x0a,
        kAVCInTransitionStatus     = 0x0b,
        kAVCImplementedStatus      = 0x0c,
        kAVCChangedStatus          = 0x0d,

        kAVCInterimStatus          = 0x0f,
        kAVCResponseImplemented    = 0x0c,
    };

    // AVC unit addresses
    enum IEEE1394UnitAddress : std::uint8_t
    {
        kAVCSubunitId0                = 0x00,
        kAVCSubunitId1                = 0x01,
        kAVCSubunitId2                = 0x02,
        kAVCSubunitId3                = 0x03,
        kAVCSubunitId4                = 0x04,
        kAVCSubunitIdExtended         = 0x05,
        kAVCSubunitIdIgnore           = 0x07,

        kAVCSubunitTypeVideoMonitor   = (0x00 << 3),
        kAVCSubunitTypeAudio          = (0x01 << 3),
        kAVCSubunitTypePrinter        = (0x02 << 3),
        kAVCSubunitTypeDiscRecorder   = (0x03 << 3),
        kAVCSubunitTypeTapeRecorder   = (0x04 << 3),
        kAVCSubunitTypeTuner          = (0x05 << 3),
        kAVCSubunitTypeCA             = (0x06 << 3),
        kAVCSubunitTypeVideoCamera    = (0x07 << 3),
        kAVCSubunitTypePanel          = (0x09 << 3),
        kAVCSubunitTypeBulletinBoard  = (0x0a << 3),
        kAVCSubunitTypeCameraStorage  = (0x0b << 3),
        kAVCSubunitTypeMusic          = (0x0c << 3),
        kAVCSubunitTypeVendorUnique   = (0x1c << 3),
        kAVCSubunitTypeExtended       = (0x1e << 3),
        kAVCSubunitTypeUnit           = (0x1f << 3),
    };

    // AVC opcode
    enum IEEE1394Opcode : std::uint8_t
    {
        // Unit
        kAVCUnitPlugInfoOpcode               = 0x02,
        kAVCUnitDigitalOutputOpcode          = 0x10,
        kAVCUnitDigitalInputOpcode           = 0x11,
        kAVCUnitChannelUsageOpcode           = 0x12,
        kAVCUnitOutputPlugSignalFormatOpcode = 0x18,
        kAVCUnitInputPlugSignalFormatOpcode  = 0x19,
        kAVCUnitConnectAVOpcode              = 0x20,
        kAVCUnitDisconnectAVOpcode           = 0x21,
        kAVCUnitConnectionsOpcode            = 0x22,
        kAVCUnitConnectOpcode                = 0x24,
        kAVCUnitDisconnectOpcode             = 0x25,
        kAVCUnitUnitInfoOpcode               = 0x30,
        kAVCUnitSubunitInfoOpcode            = 0x31,
        kAVCUnitSignalSourceOpcode           = 0x1a,
        kAVCUnitPowerOpcode                  = 0xb2,

        // Common Unit + Subunit
        kAVCCommonOpenDescriptorOpcode       = 0x08,
        kAVCCommonReadDescriptorOpcode       = 0x09,
        kAVCCommonWriteDescriptorOpcode      = 0x0A,
        kAVCCommonSearchDescriptorOpcode     = 0x0B,
        kAVCCommonObjectNumberSelectOpcode   = 0x0D,
        kAVCCommonPowerOpcode                = 0xB2,
        kAVCCommonReserveOpcode              = 0x01,
        kAVCCommonPlugInfoOpcode             = 0x02,
        kAVCCommonVendorDependentOpcode      = 0x00,

        // Panel
        kAVCPanelPassThrough                 = 0x7c,
    };

    // AVC param 0
    enum IEEE1394UnitPowerParam0 : std::uint8_t
    {
        kAVCPowerStateOn           = 0x70,
        kAVCPowerStateOff          = 0x60,
        kAVCPowerStateQuery        = 0x7f,
    };

    enum IEEE1394PanelPassThroughParam0 : std::uint8_t
    {
        kAVCPanelKeySelect          = 0x00,
        kAVCPanelKeyUp              = 0x01,
        kAVCPanelKeyDown            = 0x02,
        kAVCPanelKeyLeft            = 0x03,
        kAVCPanelKeyRight           = 0x04,
        kAVCPanelKeyRightUp         = 0x05,
        kAVCPanelKeyRightDown       = 0x06,
        kAVCPanelKeyLeftUp          = 0x07,
        kAVCPanelKeyLeftDown        = 0x08,
        kAVCPanelKeyRootMenu        = 0x09,
        kAVCPanelKeySetupMenu       = 0x0A,
        kAVCPanelKeyContentsMenu    = 0x0B,
        kAVCPanelKeyFavoriteMenu    = 0x0C,
        kAVCPanelKeyExit            = 0x0D,

        kAVCPanelKey0               = 0x20,
        kAVCPanelKey1               = 0x21,
        kAVCPanelKey2               = 0x22,
        kAVCPanelKey3               = 0x23,
        kAVCPanelKey4               = 0x24,
        kAVCPanelKey5               = 0x25,
        kAVCPanelKey6               = 0x26,
        kAVCPanelKey7               = 0x27,
        kAVCPanelKey8               = 0x28,
        kAVCPanelKey9               = 0x29,
        kAVCPanelKeyDot             = 0x2A,
        kAVCPanelKeyEnter           = 0x2B,
        kAVCPanelKeyClear           = 0x2C,

        kAVCPanelKeyChannelUp       = 0x30,
        kAVCPanelKeyChannelDown     = 0x31,
        kAVCPanelKeyPreviousChannel = 0x32,
        kAVCPanelKeySoundSelect     = 0x33,
        kAVCPanelKeyInputSelect     = 0x34,
        kAVCPanelKeyDisplayInfo     = 0x35,
        kAVCPanelKeyHelp            = 0x36,
        kAVCPanelKeyPageUp          = 0x37,
        kAVCPanelKeyPageDown        = 0x38,

        kAVCPanelKeyPower           = 0x40,
        kAVCPanelKeyVolumeUp        = 0x41,
        kAVCPanelKeyVolumeDown      = 0x42,
        kAVCPanelKeyMute            = 0x43,
        kAVCPanelKeyPlay            = 0x44,
        kAVCPanelKeyStop            = 0x45,
        kAVCPanelKeyPause           = 0x46,
        kAVCPanelKeyRecord          = 0x47,
        kAVCPanelKeyRewind          = 0x48,
        kAVCPanelKeyFastForward     = 0x49,
        kAVCPanelKeyEject           = 0x4a,
        kAVCPanelKeyForward         = 0x4b,
        kAVCPanelKeyBackward        = 0x4c,

        kAVCPanelKeyAngle           = 0x50,
        kAVCPanelKeySubPicture      = 0x51,

        kAVCPanelKeyTuneFunction    = 0x67,

        kAVCPanelKeyPress           = 0x00,
        kAVCPanelKeyRelease         = 0x80,

    };

    virtual ~FirewireDevice() = default;

    // Commands
    virtual bool OpenPort(void) = 0;
    virtual bool ClosePort(void) = 0;
    virtual bool ResetBus(void) { return false; }

    virtual void AddListener(TSDataListener *listener);
    virtual void RemoveListener(TSDataListener *listener);

    // Sets
    virtual bool SetPowerState(bool on);
    virtual bool SetChannel(const QString &panel_model,
                            uint alt_method, uint channel);

    // Gets
    virtual bool IsPortOpen(void) const = 0;
    bool IsSTBBufferCleared(void) const { return m_bufferCleared; }

    // non-const Gets
    virtual PowerState GetPowerState(void);

    // Statics
    static bool IsSTBSupported(const QString &model);
    static QString GetModelName(uint vendor_id, uint model_id);
    static std::vector<AVCInfo> GetSTBList(void);

  protected:
    FirewireDevice(uint64_t guid, uint subunitid, uint speed);

    virtual bool SendAVCCommand(const std::vector<uint8_t> &cmd,
                                std::vector<uint8_t> &result,
                                int retry_cnt) = 0;
    void SetLastChannel(uint channel);
    void ProcessPATPacket(const TSPacket &tspacket);
    virtual void BroadcastToListeners(
        const unsigned char *data, uint dataSize);

    uint64_t                 m_guid;
    uint                     m_subunitid;
    uint                     m_speed;
    uint                     m_lastChannel    {0};
    uint                     m_lastCrc        {0};
    bool                     m_bufferCleared  {true};

    uint                     m_openPortCnt    {0};
    std::vector<TSDataListener*>  m_listeners;
    mutable QMutex           m_lock;

    /// Vendor ID + Model ID to FirewireDevice STB model string
    static QMap<uint64_t,QString> s_idToModel;
    static QMutex                 s_staticLock;
};

#endif // FIREWIRE_DEVICE_H
