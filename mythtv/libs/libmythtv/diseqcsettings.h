/*
 * \file diseqcsettings.h
 * \brief DVB-S Device Tree Configuration Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

#ifndef _DISEQCSETTINGS_H_
#define _DISEQCSETTINGS_H_

#include "diseqc.h"
#include "settings.h"

typedef QMap<uint, Setting*> devid_to_setting_t;

class SwitchTypeSetting;
class SwitchPortsSetting;
class SwitchAddressSetting;

bool convert_diseqc_db(void);

class SwitchConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    SwitchConfig(DiSEqCDevSwitch &switch_dev);

  public slots:
    void update(void);

  private:
    SwitchTypeSetting  *m_type;
    SwitchPortsSetting *m_ports;
    SwitchAddressSetting *m_address;
};

class RotorPosMap : public ListBoxSetting, public Storage
{
    Q_OBJECT

  public:
    RotorPosMap(DiSEqCDevRotor &rotor);

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) {}

  public slots:
    void edit(void);
    void del(void);

  protected:
    void PopulateList(void);
    
  private:
    DiSEqCDevRotor &m_rotor;
    uint_to_dbl_t   m_posmap;
};

class RotorConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    RotorConfig(DiSEqCDevRotor &rotor);

  public slots:
    void SetType(const QString &type);
    void RunRotorPositionsDialog(void);

  private:
    DiSEqCDevRotor     &m_rotor;
    TransButtonSetting *m_pos;
};

class SCRConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    SCRConfig(DiSEqCDevSCR &scr);

  private:
    DiSEqCDevSCR &m_scr;
};

class LNBTypeSetting;
class LNBLOFSwitchSetting;
class LNBLOFLowSetting;
class LNBLOFHighSetting;
class LNBPolarityInvertedSetting;

class LNBConfig : public QObject, public ConfigurationWizard
{
    Q_OBJECT

  public:
    LNBConfig(DiSEqCDevLNB &lnb);

  public slots:
    void SetPreset(const QString &value);
    void UpdateType(void);

  private:
    LNBTypeSetting      *m_type;
    LNBLOFSwitchSetting *m_lof_switch;
    LNBLOFLowSetting    *m_lof_lo;
    LNBLOFHighSetting   *m_lof_hi;
    LNBPolarityInvertedSetting *m_pol_inv;
};

class DeviceTree : public ListBoxSetting, public Storage
{
    Q_OBJECT

  public:
    DeviceTree(DiSEqCDevTree &tree);

    virtual void Load(void);
    virtual void Save(void);
    virtual void Save(QString /*destination*/) { }

  protected:
    bool EditNodeDialog(uint nodeid);
    void CreateRootNodeDialog(void);
    void CreateNewNodeDialog(uint parentid, uint child_num);

    bool RunTypeDialog(DiSEqCDevDevice::dvbdev_t &type);
    void PopulateTree(void);
    void PopulateTree(DiSEqCDevDevice *node,
                      DiSEqCDevDevice *parent   = NULL,
                      uint             childnum = 0,
                      uint             depth    = 0);

  public slots:
    void edit(void);
    void del(void);
    
  private:
    DiSEqCDevTree &m_tree;
};

class DTVDeviceTreeWizard : public ConfigurationDialog
{
  public:
    DTVDeviceTreeWizard(DiSEqCDevTree &tree);

    virtual DialogCode exec(void);
    virtual DialogCode exec(bool /*saveOnExec*/, bool /*doLoad*/)
        { return exec(); }
};

class DTVDeviceConfigGroup : public VerticalConfigurationGroup
{
  public:
    DTVDeviceConfigGroup(DiSEqCDevSettings &settings, uint cardid,
                         bool switches_enabled);
    ~DTVDeviceConfigGroup(void);
    
  protected:
    void AddNodes(ConfigurationGroup *group, const QString &trigger,
                  DiSEqCDevDevice *node);

    void AddChild(ConfigurationGroup *group, const QString &trigger,
                  Setting *setting);
    
  private:
    DiSEqCDevTree       m_tree;
    DiSEqCDevSettings  &m_settings;
    devid_to_setting_t  m_devs;
    bool                m_switches_enabled;
};

#endif // _DISEQCSETTINGS_H_

