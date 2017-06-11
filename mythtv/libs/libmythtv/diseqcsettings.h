/*
 * \file diseqcsettings.h
 * \brief DVB-S Device Tree Configuration Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

#ifndef _DISEQCSETTINGS_H_
#define _DISEQCSETTINGS_H_

#include "diseqc.h"
#include "standardsettings.h"

typedef QMap<uint, StandardSetting*> devid_to_setting_t;

class SwitchTypeSetting;
class SwitchPortsSetting;
class SwitchAddressSetting;
class DeviceDescrSetting;

class DiseqcConfigBase : public GroupSetting
{
    Q_OBJECT

  public:
    virtual bool keyPressEvent(QKeyEvent *);
    static DiseqcConfigBase* CreateByType(DiSEqCDevDevice *dev,
                                          StandardSetting *parent);

  signals:
    void DeleteClicked(void);
};

class SwitchConfig : public DiseqcConfigBase
{
    Q_OBJECT

  public:
    SwitchConfig(DiSEqCDevSwitch &switch_dev, StandardSetting *parent);
    void Load(void);

  public slots:
    void update(void);

  private:
    DeviceDescrSetting *m_deviceDescr;
    SwitchTypeSetting  *m_type;
    SwitchPortsSetting *m_ports;
    SwitchAddressSetting *m_address;
};

class RotorPosMap : public GroupSetting
{
    Q_OBJECT

  public:
    explicit RotorPosMap(DiSEqCDevRotor &rotor);

    virtual void Load(void);
    virtual void Save(void);

  private slots:
    void valueChanged(StandardSetting*);

  protected:
    void PopulateList(void);

  private:
    DiSEqCDevRotor &m_rotor;
    uint_to_dbl_t   m_posmap;
};

class RotorConfig : public DiseqcConfigBase
{
    Q_OBJECT

  public:
    void Load();
    RotorConfig(DiSEqCDevRotor &rotor, StandardSetting *parent);

  public slots:
    void SetType(const QString &type);

  private:
    DiSEqCDevRotor     &m_rotor;
    RotorPosMap        *m_pos;
};

class SCRConfig : public DiseqcConfigBase
{
    Q_OBJECT

  public:
    SCRConfig(DiSEqCDevSCR &scr, StandardSetting* parent);

  private:
    DiSEqCDevSCR &m_scr;
};

class LNBTypeSetting;
class LNBLOFSwitchSetting;
class LNBLOFLowSetting;
class LNBLOFHighSetting;
class LNBPolarityInvertedSetting;
class LNBPresetSetting;

class LNBConfig : public DiseqcConfigBase
{
    Q_OBJECT

  public:
    LNBConfig(DiSEqCDevLNB &lnb, StandardSetting *parent);
    void Load(void);

  public slots:
    void SetPreset(const QString &value);
    void UpdateType(void);

  private:
    LNBPresetSetting    *m_preset;
    LNBTypeSetting      *m_type;
    LNBLOFSwitchSetting *m_lof_switch;
    LNBLOFLowSetting    *m_lof_lo;
    LNBLOFHighSetting   *m_lof_hi;
    LNBPolarityInvertedSetting *m_pol_inv;
};

class DeviceTypeSetting;

class DeviceTree : public GroupSetting
{
    Q_OBJECT

  public:
    explicit DeviceTree(DiSEqCDevTree &tree);
    void DeleteDevice(DeviceTypeSetting *devtype);

    virtual void Load(void);

  protected:
    void PopulateTree(void);
    void PopulateTree(DiSEqCDevDevice *node,
                      DiSEqCDevDevice *parent   = NULL,
                      uint             childnum = 0,
                      GroupSetting    *parentSetting = NULL);
    void PopulateChildren(DiSEqCDevDevice *node, GroupSetting *parentSetting);
    void AddDeviceTypeSetting(DeviceTypeSetting *devtype,
                              DiSEqCDevDevice *parent,
                              uint childnum,
                              GroupSetting *parentSetting);
    void ConnectToValueChanged(DeviceTypeSetting *devtype,
                               DiSEqCDevDevice *parent,
                               uint childnum);
    void ValueChanged(const QString &value,
                      DeviceTypeSetting *devtype,
                      DiSEqCDevDevice *parent,
                      uint childnum);

  private:
    DiSEqCDevTree &m_tree;
};

class DTVDeviceConfigGroup : public GroupSetting
{
  public:
    DTVDeviceConfigGroup(DiSEqCDevSettings &settings, uint cardid,
                         bool switches_enabled);
    ~DTVDeviceConfigGroup(void);

  protected:
    void AddNodes(StandardSetting *group, const QString &trigger,
                  DiSEqCDevDevice *node);

    void AddChild(StandardSetting *group, const QString &trigger,
                  StandardSetting *setting);

  private:
    DiSEqCDevTree       m_tree;
    DiSEqCDevSettings  &m_settings;
    devid_to_setting_t  m_devs;
    bool                m_switches_enabled;
};

#endif // _DISEQCSETTINGS_H_

