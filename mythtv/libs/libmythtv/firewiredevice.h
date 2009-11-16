/**
 *  FirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FIREWIRE_DEVICE_H_
#define _FIREWIRE_DEVICE_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QMutex>

// MythTV headers
#include "streamlisteners.h"
#include "avcinfo.h"

class TSPacket;

class FirewireDevice
{
  public:

    // Public enums
    typedef enum
    {
        kAVCPowerOn,
        kAVCPowerOff,
        kAVCPowerUnknown,
        kAVCPowerQueryFailed,
    } PowerState;

    // AVC commands
    typedef enum
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
    } IEEE1394Command;

    // AVC unit addresses
    typedef enum
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
    } IEEE1394UnitAddress;

    // AVC opcode
    typedef enum
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
    } IEEE1394Opcode;

    // AVC param 0
    typedef enum
    {
        kAVCPowerStateOn           = 0x70,
        kAVCPowerStateOff          = 0x60,
        kAVCPowerStateQuery        = 0x7f,
    } IEEE1394UnitPowerParam0;

    typedef enum
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

    } IEEE1394PanelPassThroughParam0;

    virtual ~FirewireDevice() { }

    // Commands
    virtual bool OpenPort(void) = 0;
    virtual bool ClosePort(void) = 0;
    virtual bool ResetBus(void) { return false; }

    virtual void AddListener(TSDataListener*);
    virtual void RemoveListener(TSDataListener*);

    // Sets
    virtual bool SetPowerState(bool on);
    virtual bool SetChannel(const QString &panel_model,
                            uint alt_method, uint channel);

    // Gets
    virtual bool IsPortOpen(void) const = 0;
    bool IsSTBBufferCleared(void) const { return m_buffer_cleared; }

    // non-const Gets
    virtual PowerState GetPowerState(void);

    // Statics
    static bool IsSTBSupported(const QString &model);
    static QString GetModelName(uint vendorid, uint modelid);
    static vector<AVCInfo> GetSTBList(void);

  protected:
    FirewireDevice(uint64_t guid, uint subunitid, uint speed);

    virtual bool SendAVCCommand(const vector<uint8_t> &cmd,
                                vector<uint8_t> &result,
                                int retry_cnt) = 0;
    bool GetSubunitInfo(uint8_t table[32]);

    void SetLastChannel(uint channel);
    void ProcessPATPacket(const TSPacket&);
    virtual void BroadcastToListeners(
        const unsigned char *data, uint dataSize);

    uint64_t                 m_guid;
    uint                     m_subunitid;
    uint                     m_speed;
    uint                     m_last_channel;
    uint                     m_last_crc;
    bool                     m_buffer_cleared;

    uint                     m_open_port_cnt;
    vector<TSDataListener*>  m_listeners;
    mutable QMutex           m_lock;

    /// Vendor ID + Model ID to FirewireDevice STB model string
    static QMap<uint64_t,QString> s_id_to_model;
    static QMutex                 s_static_lock;
};

#endif // _FIREWIRE_DEVICE_H_
