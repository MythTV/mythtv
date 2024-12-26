/*
 * \file diseqcsettings.cpp
 * \brief DVB-S Device Tree Configuration Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

// Std C headers
#include <array>
#include <cmath>
#include <utility>

// MythTV headers
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "diseqcsettings.h"

/* Lat/Long items relocated from videosource.cpp */

static GlobalTextEditSetting *DiSEqCLatitude(void)
{
    auto *gc = new GlobalTextEditSetting("latitude");
    gc->setLabel("Latitude");
    gc->setHelpText(
        DeviceTree::tr("The Cartesian latitude for your location. "
                       "Use negative numbers for southern coordinates."));
    return gc;
}

static GlobalTextEditSetting *DiSEqCLongitude(void)
{
    auto *gc = new GlobalTextEditSetting("longitude");
    gc->setLabel("Longitude");
    gc->setHelpText(
        DeviceTree::tr("The Cartesian longitude for your location. "
                       "Use negative numbers for western coordinates."));
    return gc;
}

//////////////////////////////////////// DeviceTypeSetting

class DeviceTypeSetting : public TransMythUIComboBoxSetting
{
  public:
    DeviceTypeSetting()
    {
        setLabel(DeviceTree::tr("Device Type"));
        addSelection(DeviceTree::tr("Unconnected"),
                     QString::number((uint) 0xFFFF));
        addSelection(DeviceTree::tr("Switch"),
                     QString::number((uint) DiSEqCDevDevice::kTypeSwitch));
        addSelection(DeviceTree::tr("Rotor"),
                     QString::number((uint) DiSEqCDevDevice::kTypeRotor));
        addSelection(DeviceTree::tr("Unicable"),
                     QString::number((uint) DiSEqCDevDevice::kTypeSCR));
        addSelection(DeviceTree::tr("LNB"),
                     QString::number((uint) DiSEqCDevDevice::kTypeLNB));
    }

    void Load(void) override // StandardSetting
    {
        if (m_device)
        {
            QString tmp = QString::number((uint) m_device->GetDeviceType());
            setValue(getValueIndex(tmp));
        }
        setChanged(false);
    }

    void SetDevice(DiSEqCDevDevice* dev)
    {
        m_device = dev;
    }

    void DeleteDevice()
    {
        SetDevice(nullptr);
        clearSettings();
    }

    DiSEqCDevDevice * GetDevice()
    {
        return m_device;
    }

  private:
    DiSEqCDevDevice *m_device {nullptr};
};

//////////////////////////////////////// DeviceDescrSetting

class DeviceDescrSetting : public TransTextEditSetting
{
  public:
    explicit DeviceDescrSetting(DiSEqCDevDevice &device) :
        m_device(device)
    {
        setLabel(DeviceTree::tr("Description"));
        QString help = DeviceTree::tr(
            "Optional descriptive name for this device, to "
            "make it easier to configure settings later.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(m_device.GetDescription());
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_device.SetDescription(getValue());
    }

  private:
    DiSEqCDevDevice &m_device;
};


//////////////////////////////////////// DeviceRepeatSetting

class DeviceRepeatSetting : public TransMythUISpinBoxSetting
{
  public:
    explicit DeviceRepeatSetting(DiSEqCDevDevice &device) :
        TransMythUISpinBoxSetting(0, 15, 1), m_device(device)
    {
        setLabel(DeviceTree::tr("Repeat Count"));
        QString help = DeviceTree::tr(
            "Number of repeat (command with repeat flag ON) or resend (the same command) DiSEqC commands. "
            "If value is higher than 10, command will be resend N-10 times. "
            "If value is lower than 10, command will be repeated N times. "
            "Repeat useful for unreliable DiSEqC equipment; resend useful when unreliable DiSEqC equipment has broken/unsupported repeat flag support.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(m_device.GetRepeatCount());
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_device.SetRepeatCount(getValue().toUInt());
    }

  private:
    DiSEqCDevDevice &m_device;
};

//////////////////////////////////////// SwitchTypeSetting

class SwitchTypeSetting : public TransMythUIComboBoxSetting
{
  public:
    explicit SwitchTypeSetting(DiSEqCDevSwitch &switch_dev) :
        m_switch(switch_dev)
    {
        setLabel(DeviceTree::tr("Switch Type"));
        setHelpText(DeviceTree::tr("Select the type of switch from the list."));

        addSelection(DeviceTree::tr("Tone"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeTone));
        addSelection(DeviceTree::tr("Voltage"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeVoltage));
        addSelection(DeviceTree::tr("Mini DiSEqC"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeMiniDiSEqC));
        addSelection(DeviceTree::tr("DiSEqC"),
                     QString::number((uint)
                                     DiSEqCDevSwitch::kTypeDiSEqCCommitted));
        addSelection(DeviceTree::tr("DiSEqC (Uncommitted)"),
                     QString::number((uint)
                                     DiSEqCDevSwitch::kTypeDiSEqCUncommitted));
        addSelection(DeviceTree::tr("Legacy SW21"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeLegacySW21));
        addSelection(DeviceTree::tr("Legacy SW42"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeLegacySW42));
        addSelection(DeviceTree::tr("Legacy SW64"),
                     QString::number((uint) DiSEqCDevSwitch::kTypeLegacySW64));
    }

    void Load(void) override // StandardSetting
    {
        setValue(getValueIndex(QString::number((uint) m_switch.GetType())));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_switch.SetType((DiSEqCDevSwitch::dvbdev_switch_t)
                         getValue().toUInt());
    }

  private:
    DiSEqCDevSwitch &m_switch;
};

//////////////////////////////////////// SwitchAddressSetting

class SwitchAddressSetting : public TransTextEditSetting
{
  public:
    explicit SwitchAddressSetting(DiSEqCDevSwitch &switch_dev) :
        m_switch(switch_dev)
    {
        setLabel(DeviceTree::tr("Address of switch"));
        setHelpText(DeviceTree::tr("The DiSEqC address of the switch."));
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString("0x%1").arg(m_switch.GetAddress(), 0, 16));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_switch.SetAddress(getValue().toUInt(nullptr, 16));
    }

  private:
    DiSEqCDevSwitch &m_switch;
};

//////////////////////////////////////// SwitchPortsSetting

class SwitchPortsSetting : public TransTextEditSetting
{
  public:
    explicit SwitchPortsSetting(DiSEqCDevSwitch &switch_dev) :
        m_switch(switch_dev)
    {
        setLabel(DeviceTree::tr("Number of ports"));
        setHelpText(DeviceTree::tr("The number of ports this switch has."));
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_switch.GetNumPorts()));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_switch.SetNumPorts(getValue().toUInt());
    }

  private:
    DiSEqCDevSwitch &m_switch;
};

//////////////////////////////////////// SwitchConfig

SwitchConfig::SwitchConfig(DiSEqCDevSwitch &switch_dev, StandardSetting *parent)
  : m_deviceDescr(new DeviceDescrSetting(switch_dev)),
    m_type(new SwitchTypeSetting(switch_dev)),
    m_ports(new SwitchPortsSetting(switch_dev)),
    m_address(new SwitchAddressSetting(switch_dev))
{
    setLabel(DeviceTree::tr("Switch Configuration"));

    parent = this;
    parent->addChild(m_deviceDescr);
    parent->addChild(new DeviceRepeatSetting(switch_dev));
    parent->addChild(m_type);
    parent->addChild(m_address);
    parent->addChild(m_ports);

    connect(m_type, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   qOverload<const QString&>(&SwitchConfig::update));
    connect(m_deviceDescr, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,   qOverload<const QString&>(&StandardSetting::setValue));
}

void SwitchConfig::Load(void)
{
    StandardSetting::Load();
    setValue(m_deviceDescr->getValue());
    update();
    setChanged(false);
}

void SwitchConfig::update(void)
{
    switch ((DiSEqCDevSwitch::dvbdev_switch_t) m_type->getValue().toUInt())
    {
        case DiSEqCDevSwitch::kTypeTone:
        case DiSEqCDevSwitch::kTypeVoltage:
        case DiSEqCDevSwitch::kTypeMiniDiSEqC:
        case DiSEqCDevSwitch::kTypeLegacySW21:
        case DiSEqCDevSwitch::kTypeLegacySW42:
            m_address->setValue(QString("0x10"));
            m_address->setEnabled(false);
            m_ports->setValue("2");
            m_ports->setEnabled(false);
            break;
        case DiSEqCDevSwitch::kTypeLegacySW64:
            m_address->setValue(QString("0x10"));
            m_address->setEnabled(false);
            m_ports->setValue("3");
            m_ports->setEnabled(false);
            break;
        case DiSEqCDevSwitch::kTypeDiSEqCCommitted:
        case DiSEqCDevSwitch::kTypeDiSEqCUncommitted:
            m_address->setEnabled(true);
            m_ports->setEnabled(true);
            break;
    }
}

void SwitchConfig::update(const QString &/*value*/)
{
    update();
}

bool DiseqcConfigBase::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("Global", e, actions))
        return true;

    auto isdelete = [](const QString & action) { return action == "DELETE"; };
    if (std::any_of(actions.cbegin(), actions.cend(), isdelete))
    {
        emit DeleteClicked();
        return true;
    }

    return false;
}

//////////////////////////////////////// RotorTypeSetting

class RotorTypeSetting : public TransMythUIComboBoxSetting
{
  public:
    explicit RotorTypeSetting(DiSEqCDevRotor &rotor) :
        m_rotor(rotor)
    {
        setLabel(DeviceTree::tr("Rotor Type"));
        setHelpText(DeviceTree::tr("Select the type of rotor from the list."));
        addSelection(DeviceTree::tr("DiSEqC 1.2"),
                     QString::number((uint) DiSEqCDevRotor::kTypeDiSEqC_1_2));
        addSelection(DeviceTree::tr("DiSEqC 1.3 (GotoX/USALS)"),
                     QString::number((uint) DiSEqCDevRotor::kTypeDiSEqC_1_3));
    }

    void Load(void) override // StandardSetting
    {
        setValue(getValueIndex(QString::number((uint)m_rotor.GetType())));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_rotor.SetType((DiSEqCDevRotor::dvbdev_rotor_t)getValue().toUInt());
    }

  private:
    DiSEqCDevRotor &m_rotor;
};

//////////////////////////////////////// RotorLoSpeedSetting

class RotorLoSpeedSetting : public TransTextEditSetting
{
  public:
    explicit RotorLoSpeedSetting(DiSEqCDevRotor &rotor) :
        m_rotor(rotor)
    {
        setLabel(DeviceTree::tr("Rotor Low Speed (deg/sec)"));
        QString help = DeviceTree::tr(
            "To allow the approximate monitoring of rotor movement, enter "
            "the rated angular speed of the rotor when powered at 13V.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_rotor.GetLoSpeed()));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_rotor.SetLoSpeed(getValue().toDouble());
    }

  private:
    DiSEqCDevRotor &m_rotor;
};

//////////////////////////////////////// RotorHiSpeedSetting

class RotorHiSpeedSetting : public TransTextEditSetting
{
  public:
    explicit RotorHiSpeedSetting(DiSEqCDevRotor &rotor) :
        m_rotor(rotor)
    {
        setLabel(DeviceTree::tr("Rotor High Speed (deg/sec)"));
        QString help = DeviceTree::tr(
            "To allow the approximate monitoring of rotor movement, enter "
            "the rated angular speed of the rotor when powered at 18V.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_rotor.GetHiSpeed()));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_rotor.SetHiSpeed(getValue().toDouble());
    }

  private:
    DiSEqCDevRotor &m_rotor;
};

//////////////////////////////////////// RotorPosMap

static QString AngleToString(double angle)
{
    QString str;
    if (angle >= 0.0)
    {
        str = QString::number(angle) +
            DeviceTree::tr("E", "Eastern Hemisphere");
    }
    else /* if (angle < 0.0) */
    {
        str = QString::number(-angle) +
            DeviceTree::tr("W", "Western Hemisphere");
    }
    return str;
}

static double AngleToEdit(double angle, QString &hemi)
{
    if (angle > 0.0)
    {
        hemi = "E";
        return angle;
    }

    hemi = "W";
    return -angle;
}

static double AngleToFloat(const QString &angle, bool translated = true)
{
    if (angle.length() < 2)
        return 0.0;

    double pos = __builtin_nan("");
    QChar postfix = angle.at(angle.length() - 1);
    if (postfix.isLetter())
    {
        pos = angle.left(angle.length() - 1).toDouble();
        if ((translated &&
             (postfix.toUpper() ==
              DeviceTree::tr("W", "Western Hemisphere").at(0))) ||
            (!translated && (postfix.toUpper() == 'W')))
        {
            pos = -pos;
        }
    }
    else
    {
        pos = angle.toDouble();
    }

    return pos;
}

void RotorPosMap::Load(void)
{
    m_posmap = m_rotor.GetPosMap();
    PopulateList();
}

void RotorPosMap::Save(void)
{
    m_rotor.SetPosMap(m_posmap);
}

class RotorPosTextEdit : public TransTextEditSetting
{
public:
    RotorPosTextEdit(const QString &label, uint id, const QString &value) :
        m_id(id)
    {
        setLabel(label);
        setValue(value);
    }

    void updateButton(MythUIButtonListItem *item) override // MythUITextEditSetting
    {
        TransTextEditSetting::updateButton(item);
        if (getValue().isEmpty())
            item->SetText(DeviceTree::tr("None"), "value");
    }

    uint m_id;
};

void RotorPosMap::newValue(StandardSetting *setting)
{
    auto *posEdit = dynamic_cast<RotorPosTextEdit*>(setting);
    if (posEdit == nullptr)
        return;
    QString value = posEdit->getValue();
    if (value.isEmpty())
        m_posmap.erase(m_posmap.find(posEdit->m_id));
    else
        m_posmap[posEdit->m_id] = AngleToFloat(posEdit->getValue());
}

void RotorPosMap::PopulateList(void)
{
    uint num_pos = 64;
    for (uint pos = 1; pos < num_pos; pos++)
    {
        uint_to_dbl_t::const_iterator it = m_posmap.constFind(pos);
        QString posval;
        if (it != m_posmap.constEnd())
            posval = AngleToString(*it);

        auto *posEdit =
            new RotorPosTextEdit(DeviceTree::tr("Position #%1").arg(pos),
                                 pos,
                                 posval);
        connect(posEdit, qOverload<StandardSetting*>(&StandardSetting::valueChanged),
                this,    &RotorPosMap::newValue);
        addChild(posEdit);
    }
}

//////////////////////////////////////// RotorConfig

RotorConfig::RotorConfig(DiSEqCDevRotor &rotor, StandardSetting *parent)
    : m_rotor(rotor),
      m_pos(new RotorPosMap(rotor))
{
    setLabel(DeviceTree::tr("Rotor Configuration"));
    setValue(rotor.GetDescription());

    parent = this;
    parent->addChild(new DeviceDescrSetting(rotor));
    parent->addChild(new DeviceRepeatSetting(rotor));

    auto *rtype = new RotorTypeSetting(rotor);
    connect(rtype, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,  &RotorConfig::SetType);
    parent->addChild(rtype);

    m_pos->setLabel(DeviceTree::tr("Positions"));
    m_pos->setHelpText(DeviceTree::tr("Rotor position setup."));
    parent->addChild(m_pos);

    parent->addChild(new RotorLoSpeedSetting(rotor));
    parent->addChild(new RotorHiSpeedSetting(rotor));
    parent->addChild(DiSEqCLatitude());
    parent->addChild(DiSEqCLongitude());
}

void RotorConfig::Load()
{
    m_pos->setEnabled(m_rotor.GetType() == DiSEqCDevRotor::kTypeDiSEqC_1_2);
    GroupSetting::Load();
}

void RotorConfig::SetType(const QString &type)
{
    auto rtype = (DiSEqCDevRotor::dvbdev_rotor_t) type.toUInt();
    m_pos->setEnabled(rtype == DiSEqCDevRotor::kTypeDiSEqC_1_2);
}

//////////////////////////////////////// SCRUserBandSetting

class SCRUserBandSetting : public TransMythUISpinBoxSetting
{
  public:
    explicit SCRUserBandSetting(DiSEqCDevSCR &scr) :
        TransMythUISpinBoxSetting(0, 7, 1), m_scr(scr)
    {
        setLabel(DeviceTree::tr("Userband"));
        setHelpText(DeviceTree::tr(
            "Unicable userband ID (0-7). The Unicable userband channels "
            "are often numbered starting at 1 but MythTV starts at 0."));
    }

    void Load(void) override // StandardSetting
    {
        setValue(m_scr.GetUserBand());
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_scr.SetUserBand(intValue());
    }

  private:
    DiSEqCDevSCR &m_scr;
};

//////////////////////////////////////// SCRFrequencySetting

class SCRFrequencySetting : public TransTextEditSetting
{
  public:
    explicit SCRFrequencySetting(DiSEqCDevSCR &scr) : m_scr(scr)
    {
        setLabel(DeviceTree::tr("Frequency (MHz)"));
        setHelpText(DeviceTree::tr("Unicable userband frequency (usually 1210, 1420, 1680 and 2040 MHz)"));
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_scr.GetFrequency()));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_scr.SetFrequency(getValue().toUInt());
    }

  private:
    DiSEqCDevSCR &m_scr;
};

//////////////////////////////////////// SCRPINSetting

class SCRPINSetting : public TransTextEditSetting
{
  public:
    explicit SCRPINSetting(DiSEqCDevSCR &scr) : m_scr(scr)
    {
        setLabel(DeviceTree::tr("PIN code"));
        setHelpText(DeviceTree::tr("Unicable PIN code (-1 disabled, 0 - 255)"));
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_scr.GetPIN()));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_scr.SetPIN(getValue().toInt());
    }

  private:
    DiSEqCDevSCR &m_scr;
};

//////////////////////////////////////// SCRConfig

SCRConfig::SCRConfig(DiSEqCDevSCR &scr, StandardSetting *parent) : m_scr(scr)
{
    setLabel(DeviceTree::tr("Unicable Configuration"));

    parent = this;
    parent->addChild(new SCRUserBandSetting(scr));
    parent->addChild(new SCRFrequencySetting(scr));
    parent->addChild(new SCRPINSetting(scr));
    parent->addChild(new DeviceRepeatSetting(scr));
}

//////////////////////////////////////// LnbPresetSetting

class lnb_preset
{
  public:
    lnb_preset(QString _name, DiSEqCDevLNB::dvbdev_lnb_t _type,
               uint _lof_sw = 0, uint _lof_lo = 0,
               uint _lof_hi = 0, bool _pol_inv = false) :
        m_name(std::move(_name)), m_type(_type),
        m_lofSw(_lof_sw), m_lofLo(_lof_lo),
        m_lofHi(_lof_hi), m_polInv(_pol_inv) {}

  public:
    QString                    m_name;
    DiSEqCDevLNB::dvbdev_lnb_t m_type;
    uint                       m_lofSw;
    uint                       m_lofLo;
    uint                       m_lofHi;
    bool                       m_polInv;
};

static const std::array<const lnb_preset,7> lnb_presets
{

    /* description, type, LOF switch, LOF low, LOF high, inverted polarity */
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "Universal (Europe)"),
               DiSEqCDevLNB::kTypeVoltageAndToneControl,
               11700000,  9750000, 10600000),
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "Single (Europe)"),
               DiSEqCDevLNB::kTypeVoltageControl,       0,  9750000),
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "Circular (N. America)"),
               DiSEqCDevLNB::kTypeVoltageControl,       0, 11250000),
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "Linear (N. America)"),
               DiSEqCDevLNB::kTypeVoltageControl,       0, 10750000),
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "C Band"),
               DiSEqCDevLNB::kTypeVoltageControl,       0,  5150000),
    lnb_preset(QT_TRANSLATE_NOOP("DeviceTree", "DishPro Bandstacked"),
               DiSEqCDevLNB::kTypeBandstacked,          0, 11250000, 14350000),
    lnb_preset(QString(), DiSEqCDevLNB::kTypeVoltageControl),
};

static uint FindPreset(const DiSEqCDevLNB &lnb)
{
    uint i = 0;
    for ( ; !lnb_presets[i].m_name.isEmpty(); i++)
    {
        if (lnb_presets[i].m_type   == lnb.GetType()      &&
            lnb_presets[i].m_lofSw == lnb.GetLOFSwitch()  &&
            lnb_presets[i].m_lofLo == lnb.GetLOFLow()     &&
            lnb_presets[i].m_lofHi == lnb.GetLOFHigh()    &&
            lnb_presets[i].m_polInv== lnb.IsPolarityInverted())
        {
            break;
        }
    }
    return i;
}

class LNBPresetSetting : public MythUIComboBoxSetting
{
  public:
    explicit LNBPresetSetting(DiSEqCDevLNB &lnb) : m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB Preset"));
        QString help = DeviceTree::tr(
            "Select the LNB preset from the list, or choose "
            "'Custom' and set the advanced settings below.");
        setHelpText(help);

        uint i = 0;
        for (; !lnb_presets[i].m_name.isEmpty(); i++)
                        addSelection(DeviceTree::tr( lnb_presets[i].m_name.toUtf8() ),
                         QString::number(i));
        addSelection(DeviceTree::tr("Custom"), QString::number(i));
    }

    void Load(void) override // StandardSetting
    {
        setValue(FindPreset(m_lnb));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

//////////////////////////////////////// LNBTypeSetting

class LNBTypeSetting : public MythUIComboBoxSetting
{
  public:
    explicit LNBTypeSetting(DiSEqCDevLNB &lnb) : m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB Type"));
        setHelpText(DeviceTree::tr("Select the type of LNB from the list."));
        addSelection(DeviceTree::tr("Legacy (Fixed)"),
                     QString::number((uint) DiSEqCDevLNB::kTypeFixed));
        addSelection(DeviceTree::tr("Standard (Voltage)"),
                     QString::number((uint) DiSEqCDevLNB::
                                     kTypeVoltageControl));
        addSelection(DeviceTree::tr("Universal (Voltage & Tone)"),
                     QString::number((uint) DiSEqCDevLNB::
                                     kTypeVoltageAndToneControl));
        addSelection(DeviceTree::tr("Bandstacked"),
                     QString::number((uint) DiSEqCDevLNB::kTypeBandstacked));
    }

    void Load(void) override // StandardSetting
    {
        setValue(getValueIndex(QString::number((uint) m_lnb.GetType())));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_lnb.SetType((DiSEqCDevLNB::dvbdev_lnb_t) getValue().toUInt());
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

//////////////////////////////////////// LNBLOFSwitchSetting

class LNBLOFSwitchSetting : public TransTextEditSetting
{
  public:
    explicit LNBLOFSwitchSetting(DiSEqCDevLNB &lnb) : m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB LOF Switch (MHz)"));
        QString help = DeviceTree::tr(
            "This defines at what frequency the LNB will do a "
            "switch from high to low setting, and vice versa.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_lnb.GetLOFSwitch() / 1000));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_lnb.SetLOFSwitch(getValue().toUInt() * 1000);
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

//////////////////////////////////////// LNBLOFLowSetting

class LNBLOFLowSetting : public TransTextEditSetting
{
  public:
    explicit LNBLOFLowSetting(DiSEqCDevLNB &lnb) : m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB LOF Low (MHz)"));
        QString help = DeviceTree::tr(
            "This defines the offset the frequency coming "
            "from the LNB will be in low setting. For bandstacked "
            "LNBs this is the vertical/right polarization band.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_lnb.GetLOFLow() / 1000));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_lnb.SetLOFLow(getValue().toUInt() * 1000);
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

//////////////////////////////////////// LNBLOFHighSetting

class LNBLOFHighSetting : public TransTextEditSetting
{
  public:
    explicit LNBLOFHighSetting(DiSEqCDevLNB &lnb) : m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB LOF High (MHz)"));
        QString help = DeviceTree::tr(
            "This defines the offset the frequency coming from "
            "the LNB will be in high setting. For bandstacked "
            "LNBs this is the horizontal/left polarization band.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(QString::number(m_lnb.GetLOFHigh() / 1000));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_lnb.SetLOFHigh(getValue().toUInt() * 1000);
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

class LNBPolarityInvertedSetting : public MythUICheckBoxSetting
{
  public:
    explicit LNBPolarityInvertedSetting(DiSEqCDevLNB &lnb) :
        m_lnb(lnb)
    {
        setLabel(DeviceTree::tr("LNB Reversed"));
        QString help = DeviceTree::tr(
            "This defines whether the signal reaching the LNB "
            "is reversed from normal polarization. This happens "
            "to circular signals bouncing twice on a toroidal "
            "dish.");
        setHelpText(help);
    }

    void Load(void) override // StandardSetting
    {
        setValue(m_lnb.IsPolarityInverted());
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_lnb.SetPolarityInverted(boolValue());
    }

  private:
    DiSEqCDevLNB &m_lnb;
};

//////////////////////////////////////// LNBConfig

LNBConfig::LNBConfig(DiSEqCDevLNB &lnb, StandardSetting *parent)
  : m_preset(new LNBPresetSetting(lnb)),
    m_type(new LNBTypeSetting(lnb)),
    m_lofSwitch(new LNBLOFSwitchSetting(lnb)),
    m_lofLo(new LNBLOFLowSetting(lnb)),
    m_lofHi(new LNBLOFHighSetting(lnb)),
    m_polInv(new LNBPolarityInvertedSetting(lnb))
{
    setLabel(DeviceTree::tr("LNB Configuration"));
    setValue(lnb.GetDescription());

    parent = this;
    auto *deviceDescr = new DeviceDescrSetting(lnb);
    parent->addChild(deviceDescr);
    parent->addChild(m_preset);
    parent->addChild(m_type);
    parent->addChild(m_lofSwitch);
    parent->addChild(m_lofLo);
    parent->addChild(m_lofHi);
    parent->addChild(m_polInv);
    connect(m_type,      qOverload<const QString&>(&StandardSetting::valueChanged),
            this,        qOverload<const QString&>(&LNBConfig::UpdateType));
    connect(m_preset,    qOverload<const QString&>(&StandardSetting::valueChanged),
            this,        &LNBConfig::SetPreset);
    connect(deviceDescr, qOverload<const QString&>(&StandardSetting::valueChanged),
            this,        qOverload<const QString&>(&StandardSetting::setValue));
}

void LNBConfig::Load(void)
{
    StandardSetting::Load();
    SetPreset(m_preset->getValue());
}

void LNBConfig::SetPreset(const QString &value)
{
    uint index = value.toUInt();
    if (index >= (sizeof(lnb_presets) / sizeof(lnb_preset)))
        return;

    const lnb_preset &preset = lnb_presets[index];
    if (preset.m_name.isEmpty())
    {
        m_type->setEnabled(true);
        UpdateType();
    }
    else
    {
        m_type->setValue(m_type->getValueIndex(
                             QString::number((uint)preset.m_type)));
        m_lofSwitch->setValue(QString::number(preset.m_lofSw / 1000));
        m_lofLo->setValue(QString::number(preset.m_lofLo / 1000));
        m_lofHi->setValue(QString::number(preset.m_lofHi / 1000));
        m_polInv->setValue(preset.m_polInv);
        m_type->setEnabled(false);
        m_lofSwitch->setEnabled(false);
        m_lofHi->setEnabled(false);
        m_lofLo->setEnabled(false);
        m_polInv->setEnabled(false);
    }
}

void LNBConfig::UpdateType(void)
{
    if (!m_type->isEnabled())
        return;

    switch ((DiSEqCDevLNB::dvbdev_lnb_t) m_type->getValue().toUInt())
    {
        case DiSEqCDevLNB::kTypeFixed:
        case DiSEqCDevLNB::kTypeVoltageControl:
            m_lofSwitch->setEnabled(false);
            m_lofHi->setEnabled(false);
            m_lofLo->setEnabled(true);
            m_polInv->setEnabled(true);
            break;
        case DiSEqCDevLNB::kTypeVoltageAndToneControl:
            m_lofSwitch->setEnabled(true);
            m_lofHi->setEnabled(true);
            m_lofLo->setEnabled(true);
            m_polInv->setEnabled(true);
            break;
        case DiSEqCDevLNB::kTypeBandstacked:
            m_lofSwitch->setEnabled(false);
            m_lofHi->setEnabled(true);
            m_lofLo->setEnabled(true);
            m_polInv->setEnabled(true);
            break;
    }
}

void LNBConfig::UpdateType(const QString &/*value*/)
{
    UpdateType();
}
//////////////////////////////////////// DeviceTree

void DeviceTree::Load(void)
{
    PopulateTree();
    GroupSetting::Load();
}

void DeviceTree::DeleteDevice(DeviceTypeSetting *devtype)
{
    DiSEqCDevDevice *dev = devtype->GetDevice();
    if (dev)
    {
        DiSEqCDevDevice *parent = dev->GetParent();
        if (parent)
            parent->SetChild(dev->GetOrdinal(), nullptr);
        else
            m_tree.SetRoot(nullptr);
    }

    devtype->DeleteDevice();
}

void DeviceTree::PopulateTree(void)
{
    clearSettings();
    PopulateTree(m_tree.Root());
}

DiseqcConfigBase* DiseqcConfigBase::CreateByType(DiSEqCDevDevice *dev,
                                                 StandardSetting *parent)
{
    DiseqcConfigBase *setting = nullptr;
    switch (dev->GetDeviceType())
    {
        case DiSEqCDevDevice::kTypeSwitch:
        {
            auto *sw = dynamic_cast<DiSEqCDevSwitch*>(dev);
            if (sw)
                setting = new SwitchConfig(*sw, parent);
        }
        break;

        case DiSEqCDevDevice::kTypeRotor:
        {
            auto *rotor = dynamic_cast<DiSEqCDevRotor*>(dev);
            if (rotor)
                setting = new RotorConfig(*rotor, parent);
        }
        break;

        case DiSEqCDevDevice::kTypeSCR:
        {
            auto *scr = dynamic_cast<DiSEqCDevSCR*>(dev);
            if (scr)
                setting = new SCRConfig(*scr, parent);
        }
        break;

        case DiSEqCDevDevice::kTypeLNB:
        {
            auto *lnb = dynamic_cast<DiSEqCDevLNB*>(dev);
            if (lnb)
                setting = new LNBConfig(*lnb, parent);
        }
        break;

        default:
            break;
    }

    return setting;
}

void DeviceTree::PopulateTree(DiSEqCDevDevice *node,
                              DiSEqCDevDevice *parent,
                              uint childnum,
                              GroupSetting *parentSetting)
{
    if (node)
    {
        auto *devtype = new DeviceTypeSetting();
        devtype->SetDevice(node);
        devtype->Load();
        DiseqcConfigBase *setting = DiseqcConfigBase::CreateByType(node,
                                                                   devtype);

        if (setting)
        {
            setting->Load();
            devtype->addChild(setting);
            PopulateChildren(node, setting);

            AddDeviceTypeSetting(devtype, parent, childnum, parentSetting);
        }
        else
        {
            delete devtype;
        }
    }
    else
    {
        auto *devtype = new DeviceTypeSetting();
        AddDeviceTypeSetting(devtype, parent, childnum, parentSetting);
    }
}

void DeviceTree::AddDeviceTypeSetting(DeviceTypeSetting *devtype,
                                      DiSEqCDevDevice *parent,
                                      uint childnum,
                                      GroupSetting *parentSetting)
{
    if (parentSetting)
        parentSetting->addChild(devtype);
    else // Root node
        addChild(devtype);

    ConnectToValueChanged(devtype, parent, childnum);
}

void DeviceTree::ConnectToValueChanged(DeviceTypeSetting *devtype,
                                       DiSEqCDevDevice *parent,
                                       uint childnum)
{
    auto slot = [devtype, parent, childnum, this](const QString &value)
    {
        ValueChanged(value, devtype, parent, childnum);
    };

    connect(devtype,
            static_cast<void (StandardSetting::*)(const QString&)>(
                                                &StandardSetting::valueChanged),
            this,
            slot);
}

void DeviceTree::PopulateChildren(DiSEqCDevDevice *node,
                                  GroupSetting *parentSetting)
{
    uint num_ch = node->GetChildCount();
    for (uint ch = 0; ch < num_ch; ch++)
    {
        PopulateTree(node->GetChild(ch), node, ch, parentSetting);
    }
}

void DeviceTree::ValueChanged(const QString &value,
                              DeviceTypeSetting *devtype,
                              DiSEqCDevDevice *parent,
                              uint childnum)
{
    // Remove old setting from m_tree
    DeleteDevice(devtype);

    const auto type = static_cast<DiSEqCDevDevice::dvbdev_t>(value.toUInt());

    DiSEqCDevDevice *dev = DiSEqCDevDevice::CreateByType(m_tree, type);
    if (dev)
    {
        if (!parent)
        {
            m_tree.SetRoot(dev);
            PopulateTree();
        }
        else if (parent->SetChild(childnum, dev))
        {
            DiseqcConfigBase *newConfig =
                DiseqcConfigBase::CreateByType(dev, devtype);
            newConfig->Load();

            PopulateChildren(dev, newConfig);

            devtype->addChild(newConfig);
            devtype->SetDevice(dev);
        }
        else
        {
            delete dev;
        }
    }

    emit settingsChanged(this);
}

//////////////////////////////////////// SwitchSetting

class SwitchSetting : public MythUIComboBoxSetting
{
  public:
    SwitchSetting(DiSEqCDevDevice &node, DiSEqCDevSettings &settings)
        : m_node(node), m_settings(settings)
    {
        setLabel(node.GetDescription());
        setHelpText(DeviceTree::tr("Choose a port to use for this switch."));

        uint num_children = node.GetChildCount();
        for (uint ch = 0; ch < num_children; ch++)
        {
            QString val = QString("%1").arg(ch);
            QString descr = DeviceTree::tr("Port %1").arg(ch+1);
            DiSEqCDevDevice *child = node.GetChild(ch);
            if (child)
                descr += QString(" (%2)").arg(child->GetDescription());
            addSelection(descr, val);
        }
    }

    void Load(void) override // StandardSetting
    {
        double value = m_settings.GetValue(m_node.GetDeviceID());
        setValue((uint)value);
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_settings.SetValue(m_node.GetDeviceID(), getValue().toDouble());
    }

  private:
    DiSEqCDevDevice   &m_node;
    DiSEqCDevSettings &m_settings;
};

//////////////////////////////////////// RotorSetting

class RotorSetting : public MythUIComboBoxSetting
{
  public:
    RotorSetting(DiSEqCDevDevice &node, DiSEqCDevSettings &settings)
        : m_node(node), m_settings(settings)
    {
        setLabel(node.GetDescription());
        setHelpText(DeviceTree::tr("Choose a satellite position."));

        auto *rotor = dynamic_cast<DiSEqCDevRotor*>(&m_node);
        if (rotor)
            m_posmap = rotor->GetPosMap();
    }

    void Load(void) override // StandardSetting
    {
        clearSelections();

        for (double d : std::as_const(m_posmap))
            addSelection(AngleToString(d), QString::number(d));

        double angle = m_settings.GetValue(m_node.GetDeviceID());
        setValue(getValueIndex(QString::number(angle)));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_settings.SetValue(m_node.GetDeviceID(), getValue().toDouble());
    }

  private:
    DiSEqCDevDevice   &m_node;
    DiSEqCDevSettings &m_settings;
    uint_to_dbl_t      m_posmap;
};

//////////////////////////////////////// USALSRotorSetting

class USALSRotorSetting : public GroupSetting
{
  public:
    USALSRotorSetting(DiSEqCDevDevice &node, DiSEqCDevSettings &settings) :
        m_numeric(new TransTextEditSetting()),
        m_hemisphere(new TransMythUIComboBoxSetting(/*false*/)),
        m_node(node), m_settings(settings)
    {
        QString help =
            DeviceTree::tr(
                "Locates the satellite you wish to point to "
                "with the longitude along the Clarke Belt of "
                "the satellite [-180..180] and its hemisphere.");

        m_numeric->setLabel(DeviceTree::tr("Longitude (degrees)"));
        m_numeric->setHelpText(help);
        m_hemisphere->setLabel(DeviceTree::tr("Hemisphere"));
        m_hemisphere->addSelection(DeviceTree::tr("Eastern"), "E", false);
        m_hemisphere->addSelection(DeviceTree::tr("Western"), "W", true);
        m_hemisphere->setHelpText(help);

        addChild(m_numeric);
        addChild(m_hemisphere);

        addChild(new RotorConfig(dynamic_cast<DiSEqCDevRotor&>(m_node), this));
    }

    void Load(void) override // StandardSetting
    {
        double  val  = m_settings.GetValue(m_node.GetDeviceID());
        QString hemi;
        double  eval = AngleToEdit(val, hemi);
        m_numeric->setValue(QString::number(eval));
        m_hemisphere->setValue(m_hemisphere->getValueIndex(hemi));
        GroupSetting::Load();
    }

    void Save(void) override // StandardSetting
    {
        QString val = QString::number(m_numeric->getValue().toDouble());
        val += m_hemisphere->getValue();
        m_settings.SetValue(m_node.GetDeviceID(), AngleToFloat(val, false));
    }

  private:
    TransTextEditSetting       *m_numeric    {nullptr};
    TransMythUIComboBoxSetting *m_hemisphere {nullptr};
    DiSEqCDevDevice            &m_node;
    DiSEqCDevSettings          &m_settings;
};

//////////////////////////////////////// SCRPositionSetting

class SCRPositionSetting : public MythUIComboBoxSetting
{
  public:
    SCRPositionSetting(DiSEqCDevDevice &node, DiSEqCDevSettings &settings)
        : m_node(node), m_settings(settings)
    {
        setLabel("Position");
        setHelpText(DeviceTree::tr("Unicable satellite position (A/B)"));
        addSelection(DiSEqCDevSCR::SCRPositionToString(DiSEqCDevSCR::kTypeScrPosA),
                     QString::number((uint)DiSEqCDevSCR::kTypeScrPosA), true);
        addSelection(DiSEqCDevSCR::SCRPositionToString(DiSEqCDevSCR::kTypeScrPosB),
                     QString::number((uint)DiSEqCDevSCR::kTypeScrPosB), false);
    }

    void Load(void) override // StandardSetting
    {
        double value = m_settings.GetValue(m_node.GetDeviceID());
        setValue(getValueIndex(QString::number((uint)value)));
        setChanged(false);
    }

    void Save(void) override // StandardSetting
    {
        m_settings.SetValue(m_node.GetDeviceID(), getValue().toDouble());
    }

  private:
    DiSEqCDevDevice      &m_node;
    DiSEqCDevSettings    &m_settings;
};

//////////////////////////////////////// DTVDeviceConfigGroup

DTVDeviceConfigGroup::DTVDeviceConfigGroup(
    DiSEqCDevSettings &settings, uint cardid, bool switches_enabled) :
    m_settings(settings), m_switchesEnabled(switches_enabled)
{
    setLabel(DeviceTree::tr("DTV Device Configuration"));

    // load
    m_tree.Load(cardid);

    // initial UI setup
    AddNodes(this, QString(), m_tree.Root());
}

void DTVDeviceConfigGroup::AddNodes(
    StandardSetting *group, const QString &trigger, DiSEqCDevDevice *node)
{
    if (!node)
        return;

    StandardSetting *setting = nullptr;
    switch (node->GetDeviceType())
    {
        case DiSEqCDevDevice::kTypeSwitch:
            setting = new SwitchSetting(*node, m_settings);
            setting->setEnabled(m_switchesEnabled);
            break;
        case DiSEqCDevDevice::kTypeRotor:
        {
            auto *rotor = dynamic_cast<DiSEqCDevRotor*>(node);
            if (rotor && (rotor->GetType() == DiSEqCDevRotor::kTypeDiSEqC_1_2))
                setting = new RotorSetting(*node, m_settings);
            else
                setting = new USALSRotorSetting(*node, m_settings);
            break;
        }
        case DiSEqCDevDevice::kTypeSCR:
        {
            setting = new SCRPositionSetting(*node, m_settings);
            break;
        }
        default:
            break;
    }

    if (!setting)
        return;

    m_devs[node->GetDeviceID()] = setting;

    uint num_ch = node->GetChildCount();
    if (DiSEqCDevDevice::kTypeSwitch == node->GetDeviceType())
    {
        for (uint i = 0; i < num_ch; i++)
            AddNodes(setting, QString::number(i), node->GetChild(i));

        AddChild(group, trigger, setting);
        return;
    }

    if (!num_ch)
    {
        AddChild(group, trigger, setting);
        return;
    }

    AddChild(group, trigger, setting);
    for (uint i = 0; i < num_ch; i++)
        AddNodes(group, trigger, node->GetChild(i));
}

void DTVDeviceConfigGroup::AddChild(
    StandardSetting *group, const QString &trigger, StandardSetting *setting)
{
    if (!trigger.isEmpty())
        group->addTargetedChild(trigger, setting);
    else
        group->addChild(setting);
}
