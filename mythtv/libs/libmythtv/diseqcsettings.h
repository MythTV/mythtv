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

bool DTVDeviceNeedsConfiguration(uint cardid);
bool DTVDeviceNeedsConfiguration(DiSEqCDevTree& tree);

class SwitchTypeSetting;
class SwitchPortsSetting;

class SwitchConfig : public ConfigurationWizard
{
    Q_OBJECT

  public:
    SwitchConfig(DiSEqCDevSwitch &switch_dev);

  public slots:
    void update(void);

  private:
    SwitchTypeSetting  *m_type;
    SwitchPortsSetting *m_ports;
};

class RotorPosMap : public ListBoxSetting
{
    Q_OBJECT

  public:
    RotorPosMap(DiSEqCDevRotor &rotor);

    virtual void load(void);
    virtual void save(void);

  public slots:
    void edit(void);
    void del(void);

  protected:
    void PopulateList(void);
    
  private:
    DiSEqCDevRotor &m_rotor;
    uint_to_dbl_t   m_posmap;
};

class RotorConfig : public ConfigurationWizard
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

class LNBTypeSetting;
class LNBLOFSwitchSetting;
class LNBLOFLowSetting;
class LNBLOFHighSetting;
class LNBPolarityInvertedSetting;

class LNBConfig : public ConfigurationWizard
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

class DeviceTree : public ListBoxSetting
{
    Q_OBJECT

  public:
    DeviceTree(DiSEqCDevTree &tree);

    virtual void load(void);
    virtual void save(void);

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

class DTVDeviceTreeWizard : public VerticalConfigurationGroup,
                            public ConfigurationDialog
{
    Q_OBJECT

  public:
    DTVDeviceTreeWizard(DiSEqCDevTree &tree);

    virtual int exec(void);
};

class DTVDeviceConfigWizard : public ConfigurationWizard
{
    Q_OBJECT

  public:
    DTVDeviceConfigWizard(DiSEqCDevSettings &settings, uint cardid);
    ~DTVDeviceConfigWizard(void);
    
  public slots:
    void SelectNodes(void);
  
  protected:
    void AddNodes(ConfigurationGroup &group, DiSEqCDevDevice *node);
    
  private:
    DiSEqCDevTree       m_tree;
    DiSEqCDevSettings  &m_settings;
    devid_to_setting_t  m_devs;
};

#endif // _DISEQCSETTINGS_H_

