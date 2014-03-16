/* -*- Mode: c++ -*-
 * \file diseqc.h
 * \brief DVB-S Device Tree Control Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

#ifndef _DISEQC_H_
#define _DISEQC_H_

// C headers
#include <inttypes.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QMutex>
#include <QMap>

class DTVMultiplex;

class DiSEqCDevSettings;
class DiSEqCDevTrees;
class DiSEqCDevTree;
class DiSEqCDevDevice;
class DiSEqCDevRotor;
class DiSEqCDevLNB;
class DiSEqCDevSCR;

typedef QMap<uint, double>         uint_to_dbl_t;
typedef QMap<double, uint>         dbl_to_uint_t;
typedef QMap<uint, DiSEqCDevTree*> cardid_to_diseqc_tree_t;
typedef vector<DiSEqCDevDevice*>   dvbdev_vec_t;

class DiSEqCDevSettings
{
  public:
    DiSEqCDevSettings();

    bool   Load( uint card_input_id);
    bool   Store(uint card_input_id) const;
    double GetValue(uint devid) const;
    void   SetValue(uint devid, double value);

  protected:
    uint_to_dbl_t m_config;   ///< map of dev tree id to configuration value
    uint          m_input_id; ///< current input id
};

class DiSEqCDev
{
  public:
    DiSEqCDevTree* FindTree(uint cardid);
    void           InvalidateTrees(void);

  protected:
    static DiSEqCDevTrees m_trees;
};

class DiSEqCDevTrees
{
  public:
    ~DiSEqCDevTrees();

    DiSEqCDevTree *FindTree(uint cardid);
    void           InvalidateTrees(void);

  protected:
    cardid_to_diseqc_tree_t m_trees;
    QMutex                  m_trees_lock;
};

class DiSEqCDevTree
{
  public:
    DiSEqCDevTree();
    ~DiSEqCDevTree();

    bool Load(uint cardid);
    bool Store(uint cardid);
    bool Execute(const DiSEqCDevSettings &settings,
                 const DTVMultiplex &tuning);
    void Reset(void);

    DiSEqCDevRotor  *FindRotor(const DiSEqCDevSettings &settings, uint index = 0);
    DiSEqCDevLNB    *FindLNB(const DiSEqCDevSettings &settings);
    DiSEqCDevSCR    *FindSCR(const DiSEqCDevSettings &settings);
    DiSEqCDevDevice *FindDevice(uint dev_id);

    /** \brief Retrieves the root node in the tree. */
    DiSEqCDevDevice *Root(void) { return m_root; }
    void SetRoot(DiSEqCDevDevice *root);

    bool SendCommand(uint adr, uint cmd, uint repeats = 0,
                     uint data_len = 0, unsigned char *data = NULL);

    bool ResetDiseqc(bool hard_reset, bool is_SCR);

    // frontend fd
    void Open(int fd_frontend, bool is_SCR);
    void Close(void) { m_fd_frontend = -1; }
    int  GetFD(void) const { return m_fd_frontend; }

    // Sets
    bool SetTone(bool on);
    bool SetVoltage(uint voltage);

    // Gets
    uint GetVoltage(void) const { return m_last_voltage; }
    bool IsInNeedOfConf(void) const;

    // tree management
    void AddDeferredDelete(uint dev_id) { m_delete.push_back(dev_id); }
    uint CreateFakeDiSEqCID(void)       { return m_previous_fake_diseqcid++; }

    static bool IsFakeDiSEqCID(uint id) { return id >= kFirstFakeDiSEqCID; }
    static bool Exists(int id);

  protected:
    bool ApplyVoltage(const DiSEqCDevSettings &settings,
                      const DTVMultiplex &tuning);

    int              m_fd_frontend;
    DiSEqCDevDevice *m_root;
    uint             m_last_voltage;
    uint             m_previous_fake_diseqcid;
    vector<uint>     m_delete;

    static const uint kFirstFakeDiSEqCID;
};

class DiSEqCDevDevice
{
  public:
    DiSEqCDevDevice(DiSEqCDevTree &tree, uint devid);
    virtual ~DiSEqCDevDevice();

    // Commands
    virtual void Reset(void) {}
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&) = 0;
    virtual bool Load(void) = 0;
    virtual bool Store(void) const = 0;

    // Sets
    enum dvbdev_t
    {
        kTypeSwitch = 0,
        kTypeRotor = 1,
        kTypeSCR = 2,
        kTypeLNB = 3,
    };
    void SetDeviceType(dvbdev_t type)        { m_dev_type = type;    }
    void SetParent(DiSEqCDevDevice* parent)  { m_parent   = parent;  }
    void SetOrdinal(uint ordinal)            { m_ordinal  = ordinal; }
    void SetDescription(const QString &desc) { m_desc     = desc;    }
    void SetRepeatCount(uint repeat)         { m_repeat   = repeat;  }
    virtual bool SetChild(uint, DiSEqCDevDevice*){return false;      }

    // Gets
    dvbdev_t      GetDeviceType(void)  const { return m_dev_type;    }
    uint          GetDeviceID(void)    const { return m_devid;       }
    bool          IsRealDeviceID(void) const
        { return !DiSEqCDevTree::IsFakeDiSEqCID(m_devid); }
    DiSEqCDevDevice *GetParent(void)   const { return m_parent;      }
    uint          GetOrdinal(void)     const { return m_ordinal;     }
    QString       GetDescription(void) const { return m_desc;        }
    uint          GetRepeatCount(void) const { return m_repeat;      }
    virtual uint  GetChildCount(void)  const { return 0;             }
    virtual bool  IsCommandNeeded(
        const DiSEqCDevSettings&, const DTVMultiplex&)
                                       const { return false;         }
    virtual uint  GetVoltage(
        const DiSEqCDevSettings&, const DTVMultiplex&) const = 0;

    // Non-const Gets
    DiSEqCDevDevice *FindDevice(uint dev_id);
    virtual DiSEqCDevDevice *GetSelectedChild(
        const DiSEqCDevSettings&)      const { return NULL;          }
    virtual DiSEqCDevDevice *GetChild(uint)  { return NULL;          }

    // Statics
    static QString DevTypeToString(dvbdev_t type)
        { return TableToString((uint)type, dvbdev_lookup); }
    static dvbdev_t DevTypeFromString(const QString &type)
        { return (dvbdev_t) TableFromString(type, dvbdev_lookup); }

    static DiSEqCDevDevice *CreateById(  DiSEqCDevTree &tree,
                                      uint        devid);
    static DiSEqCDevDevice *CreateByType(DiSEqCDevTree &tree,
                                      dvbdev_t    type,
                                      uint        devid = 0);

  protected:
    void SetDeviceID(uint devid)       const { m_devid    = devid;   }

    mutable uint     m_devid;
    dvbdev_t         m_dev_type;
    QString          m_desc;
    DiSEqCDevTree   &m_tree;
    DiSEqCDevDevice *m_parent;
    uint             m_ordinal;
    uint             m_repeat;

    typedef struct { QString name; uint value; } TypeTable;
    static QString TableToString(uint type, const TypeTable *table);
    static uint    TableFromString(const QString   &type,
                                   const TypeTable *table);

  private:
    static const TypeTable dvbdev_lookup[5];
};

class DiSEqCDevSwitch : public DiSEqCDevDevice
{
  public:
    DiSEqCDevSwitch(DiSEqCDevTree &tree, uint devid);
    ~DiSEqCDevSwitch();

    // Commands
    virtual void Reset(void);
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&);
    virtual bool Load(void);
    virtual bool Store(void) const;

    // Sets
    enum dvbdev_switch_t
    {
        kTypeTone              = 0,
        kTypeDiSEqCCommitted   = 1,
        kTypeDiSEqCUncommitted = 2,
        kTypeLegacySW21        = 3,
        kTypeLegacySW42        = 4,
        kTypeLegacySW64        = 5,
        kTypeVoltage           = 6,
        kTypeMiniDiSEqC        = 7,
    };
    void SetType(dvbdev_switch_t type)        { m_type = type;       }
    void SetAddress(uint address)             { m_address = address; }
    void SetNumPorts(uint num_ports);
    virtual bool SetChild(uint, DiSEqCDevDevice*);

    // Gets
    dvbdev_switch_t GetType(void)       const { return m_type;       }
    uint            GetAddress(void)    const { return m_address;    }
    uint            GetNumPorts(void)   const { return m_num_ports;  }
    bool            ShouldSwitch(const DiSEqCDevSettings &settings,
                                 const DTVMultiplex &tuning) const;
    virtual uint    GetChildCount(void) const;
    virtual bool    IsCommandNeeded(const DiSEqCDevSettings&,
                                    const DTVMultiplex&) const;
    virtual uint    GetVoltage(const DiSEqCDevSettings&,
                               const DTVMultiplex&) const;

    // Non-const Gets
    virtual DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings&) const;
    virtual DiSEqCDevDevice *GetChild(uint);

    // Statics
    static QString SwitchTypeToString(dvbdev_switch_t type)
        { return TableToString((uint)type, SwitchTypeTable); }
    static dvbdev_switch_t SwitchTypeFromString(const QString &type)
        { return (dvbdev_switch_t) TableFromString(type, SwitchTypeTable); }


  protected:
    bool ExecuteLegacy(
        const DiSEqCDevSettings&, const DTVMultiplex&, uint pos);
    bool ExecuteTone(
        const DiSEqCDevSettings&, const DTVMultiplex&, uint pos);
    bool ExecuteVoltage(
        const DiSEqCDevSettings&, const DTVMultiplex&, uint pos);
    bool ExecuteMiniDiSEqC(
        const DiSEqCDevSettings&, const DTVMultiplex&, uint pos);
    bool ExecuteDiseqc(
        const DiSEqCDevSettings&, const DTVMultiplex&, uint pos);

    int  GetPosition(  const DiSEqCDevSettings&) const;

  private:
    dvbdev_switch_t m_type;
    uint            m_address;
    uint            m_num_ports;
    uint            m_last_pos;
    uint            m_last_high_band;
    uint            m_last_horizontal;
    dvbdev_vec_t    m_children;

    static const TypeTable SwitchTypeTable[9];
};

class DiSEqCDevRotor : public DiSEqCDevDevice
{
  public:
    DiSEqCDevRotor(DiSEqCDevTree &tree, uint devid);
    ~DiSEqCDevRotor();

    // Commands
    virtual void Reset(void);
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&);
    virtual bool Load(void);
    virtual bool Store(void) const;

    // Sets
    enum dvbdev_rotor_t { kTypeDiSEqC_1_2 = 0, kTypeDiSEqC_1_3 = 1, };
    void   SetType(dvbdev_rotor_t type)    { m_type     = type;  }
    void   SetLoSpeed(double speed)        { m_speed_lo = speed; }
    void   SetHiSpeed(double speed)        { m_speed_hi = speed; }
    void   SetPosMap(const uint_to_dbl_t &posmap);
    virtual bool SetChild(uint ordinal, DiSEqCDevDevice* device);
    void   RotationComplete(void) const;

    // Gets
    dvbdev_rotor_t GetType(void)         const { return m_type;      }
    double         GetLoSpeed(void)      const { return m_speed_lo;  }
    double         GetHiSpeed(void)      const { return m_speed_hi;  }
    uint_to_dbl_t  GetPosMap(void)       const;
    double         GetProgress(void)     const;
    bool           IsPositionKnown(void) const;
    virtual uint   GetChildCount(void)   const { return 1;           }
    virtual bool   IsCommandNeeded(const DiSEqCDevSettings&,
                                   const DTVMultiplex&) const;
    bool           IsMoving(const DiSEqCDevSettings&) const;
    virtual uint   GetVoltage(const DiSEqCDevSettings&,
                              const DTVMultiplex&) const;

    // Non-const Gets
    virtual DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings&) const;
    virtual DiSEqCDevDevice *GetChild(uint)    { return m_child;     }

    // Statics
    static QString RotorTypeToString(dvbdev_rotor_t type)
        { return TableToString((uint)type, RotorTypeTable); }
    static dvbdev_rotor_t RotorTypeFromString(const QString &type)
        { return (dvbdev_rotor_t) TableFromString(type, RotorTypeTable); }

  protected:
    bool   ExecuteRotor(const DiSEqCDevSettings&, const DTVMultiplex&,
                        double angle);
    bool   ExecuteUSALS(const DiSEqCDevSettings&, const DTVMultiplex&,
                        double angle);
    void   StartRotorPositionTracking(double azimuth);

    double CalculateAzimuth(double angle) const;
    double GetApproxAzimuth(void) const;

  private:
    // configuration
    dvbdev_rotor_t m_type;
    double         m_speed_hi;
    double         m_speed_lo;
    dbl_to_uint_t  m_posmap;
    DiSEqCDevDevice  *m_child;

    // state
    double         m_last_position;
    double         m_desired_azimuth;
    bool           m_reset;

    // rotor position tracking state
    mutable double m_move_time;
    mutable bool   m_last_pos_known;
    mutable double m_last_azimuth;

    // statics
    static const TypeTable RotorTypeTable[3];
};

class DiSEqCDevSCR : public DiSEqCDevDevice
{
  public:
    DiSEqCDevSCR(DiSEqCDevTree &tree, uint devid);
    ~DiSEqCDevSCR();

    // Commands
    virtual void Reset(void);
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&);
    bool         PowerOff(void) const;
    virtual bool Load(void);
    virtual bool Store(void) const;

    // Sets
    enum dvbdev_pos_t
    {
        kTypeScrPosA               = 0,
        kTypeScrPosB               = 1,
    };
    void SetUserBand(uint userband)        { m_scr_userband  = userband;   }
    void SetFrequency(uint freq)           { m_scr_frequency = freq;       }
    void SetPIN(int pin)                   { m_scr_pin       = pin;        }
    virtual bool SetChild(uint ordinal, DiSEqCDevDevice* device);

    // Gets
    uint         GetUserBand(void) const   { return m_scr_userband;        }
    uint         GetFrequency(void) const  { return m_scr_frequency;       }
    int          GetPIN(void) const        { return m_scr_pin;             }
    virtual uint GetChildCount(void) const { return 1;                     }
    virtual bool IsCommandNeeded(const DiSEqCDevSettings&,
                                 const DTVMultiplex&) const { return false; }
    virtual uint GetVoltage(const DiSEqCDevSettings&,
                            const DTVMultiplex&) const;
    uint32_t     GetIntermediateFrequency(const uint32_t frequency) const;

    // Non-const Gets
    virtual DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings&) const
                                            { return m_child;              }
    virtual DiSEqCDevDevice *GetChild(uint) { return m_child;              }

    // statics
    static QString SCRPositionToString(dvbdev_pos_t pos)
        { return TableToString((uint)pos, SCRPositionTable); }

    static dvbdev_pos_t SCRPositionFromString(const QString &pos)
        { return (dvbdev_pos_t) TableFromString(pos, SCRPositionTable); }

  protected:
    bool         SendCommand(uint cmd, uint repeats, uint data_len = 0,
                             unsigned char *data = NULL) const;

  private:
    uint         m_scr_userband;  /* 0-7 */
    uint         m_scr_frequency;
    int          m_scr_pin;       /* 0-255, -1=disabled */

    DiSEqCDevDevice *m_child;

    static const TypeTable SCRPositionTable[3];
};

class DiSEqCDevLNB : public DiSEqCDevDevice
{
  public:
    DiSEqCDevLNB(DiSEqCDevTree &tree, uint devid);

    // Commands
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&);
    virtual bool Load(void);
    virtual bool Store(void) const;

    // Sets
    enum dvbdev_lnb_t
    {
        kTypeFixed                 = 0,
        kTypeVoltageControl        = 1,
        kTypeVoltageAndToneControl = 2,
        kTypeBandstacked           = 3,
    };
    void SetType(dvbdev_lnb_t type)       { m_type       = type;       }
    void SetLOFSwitch(uint lof_switch)    { m_lof_switch = lof_switch; }
    void SetLOFHigh(  uint lof_hi)        { m_lof_hi     = lof_hi;     }
    void SetLOFLow(   uint lof_lo)        { m_lof_lo     = lof_lo;     }
    void SetPolarityInverted(bool inv)    { m_pol_inv    = inv;        }

    // Gets
    dvbdev_lnb_t GetType(void)      const { return m_type;             }
    uint         GetLOFSwitch(void) const { return m_lof_switch;       }
    uint         GetLOFHigh(void)   const { return m_lof_hi;           }
    uint         GetLOFLow(void)    const { return m_lof_lo;           }
    bool         IsPolarityInverted(void) const { return m_pol_inv;    }
    bool         IsHighBand(const DTVMultiplex&) const;
    bool         IsHorizontal(const DTVMultiplex&) const;
    uint32_t     GetIntermediateFrequency(const DiSEqCDevSettings&,
                                          const DTVMultiplex&) const;
    virtual uint GetVoltage(const DiSEqCDevSettings&,
                            const DTVMultiplex&) const;

    // statics
    static QString LNBTypeToString(dvbdev_lnb_t type)
        { return TableToString((uint)type, LNBTypeTable); }

    static dvbdev_lnb_t LNBTypeFromString(const QString &type)
        { return (dvbdev_lnb_t) TableFromString(type, LNBTypeTable); }

  private:
    dvbdev_lnb_t m_type;
    uint         m_lof_switch;
    uint         m_lof_hi;
    uint         m_lof_lo;
    /// If a signal is circularly polarized the polarity will flip
    /// on each reflection, so antenna systems with an even number
    /// of reflectors will need to set this value.
    bool         m_pol_inv;

    static const TypeTable LNBTypeTable[5];
};

#endif // _DISEQC_H_
