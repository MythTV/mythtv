/* -*- Mode: c++ -*-
 * \file diseqc.h
 * \brief DVB-S Device Tree Control Classes.
 * \author Copyright (C) 2006, Yeasah Pell
 */

#ifndef DISEQC_H
#define DISEQC_H

// C++ headers
#include <cinttypes>
#include <climits>
#include <vector>

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

using uint_to_dbl_t           = QMap<uint, double>;
using dbl_to_uint_t           = QMap<double, uint>;
using cardid_to_diseqc_tree_t = QMap<uint, DiSEqCDevTree*>;
using dvbdev_vec_t            = std::vector<DiSEqCDevDevice*>;
using cmd_vec_t               = std::vector<uint8_t>;

class DiSEqCDevSettings
{
  public:
    DiSEqCDevSettings() = default;

    bool   Load( uint card_input_id);
    bool   Store(uint card_input_id) const;
    double GetValue(uint devid) const;
    void   SetValue(uint devid, double value);

  protected:
    uint_to_dbl_t m_config;       //< map of dev tree id to configuration value
    uint          m_inputId {1};  ///< current input id
};

class DiSEqCDev
{
  public:
    static DiSEqCDevTree* FindTree(uint cardid);
    static void           InvalidateTrees(void);

  protected:
    static DiSEqCDevTrees s_trees;
};

class DiSEqCDevTrees
{
  public:
    ~DiSEqCDevTrees();

    DiSEqCDevTree *FindTree(uint cardid);
    void           InvalidateTrees(void);

  protected:
    cardid_to_diseqc_tree_t m_trees;
    QMutex                  m_treesLock;
};

class DiSEqCDevTree
{
  public:
    DiSEqCDevTree() { Reset(); }
    ~DiSEqCDevTree();

    bool Load(const QString &device);
    bool Load(uint cardid);
    bool Store(uint cardid, const QString &device = "");
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

    bool SendCommand(uint adr, uint cmd, uint repeats,
                     cmd_vec_t &data) const;
    bool SendCommand(uint adr, uint cmd, uint repeats = 0) const
        {
            cmd_vec_t nada;
            return SendCommand(adr, cmd, repeats, nada);
        }

    bool ResetDiseqc(bool hard_reset, bool is_SCR);

    // frontend fd
    void Open(int fd_frontend, bool is_SCR);
    void Close(void) { m_fdFrontend = -1; }
    int  GetFD(void) const { return m_fdFrontend; }

    // Sets
    bool SetTone(bool on) const;
    bool SetVoltage(uint voltage);

    // Gets
    uint GetVoltage(void) const { return m_lastVoltage; }
    bool IsInNeedOfConf(void) const;

    // tree management
    void AddDeferredDelete(uint dev_id) { m_delete.push_back(dev_id); }
    uint CreateFakeDiSEqCID(void)       { return m_previousFakeDiseqcid++; }

    static bool IsFakeDiSEqCID(uint id) { return id >= kFirstFakeDiSEqCID; }
    static bool Exists(int cardid);

  protected:
    bool ApplyVoltage(const DiSEqCDevSettings &settings,
                      const DTVMultiplex &tuning);

    int              m_fdFrontend             {-1};
    DiSEqCDevDevice *m_root                   {nullptr};
    uint             m_lastVoltage            {UINT_MAX};
    uint             m_previousFakeDiseqcid   {kFirstFakeDiSEqCID};
    std::vector<uint> m_delete;

    static const uint kFirstFakeDiSEqCID;
};

class DiSEqCDevDevice
{
  public:
    DiSEqCDevDevice(DiSEqCDevTree &tree, uint devid)
        : m_devid(devid), m_tree(tree) {}
    virtual ~DiSEqCDevDevice();

    // Commands
    virtual void Reset(void) {}
    virtual bool Execute(const DiSEqCDevSettings&, const DTVMultiplex&) = 0;
    virtual bool Load(void) = 0;
    virtual bool Store(void) const = 0;

    // Sets
    enum dvbdev_t : std::uint8_t
    {
        kTypeSwitch = 0,
        kTypeRotor = 1,
        kTypeSCR = 2,
        kTypeLNB = 3,
    };
    void SetDeviceType(dvbdev_t type)        { m_devType = type;     }
    void SetParent(DiSEqCDevDevice* parent)  { m_parent   = parent;  }
    void SetOrdinal(uint ordinal)            { m_ordinal  = ordinal; }
    void SetDescription(const QString &desc) { m_desc     = desc;    }
    void SetRepeatCount(uint repeat)         { m_repeat   = repeat;  }
    virtual bool SetChild(uint /*ordinal*/, DiSEqCDevDevice */*device*/)
                                             {return false;          }

    // Gets
    dvbdev_t      GetDeviceType(void)  const { return m_devType;     }
    uint          GetDeviceID(void)    const { return m_devid;       }
    bool          IsRealDeviceID(void) const
        { return !DiSEqCDevTree::IsFakeDiSEqCID(m_devid); }
    DiSEqCDevDevice *GetParent(void)   const { return m_parent;      }
    uint          GetOrdinal(void)     const { return m_ordinal;     }
    QString       GetDescription(void) const { return m_desc;        }
    uint          GetRepeatCount(void) const { return m_repeat;      }
    virtual uint  GetChildCount(void)  const { return 0;             }
    virtual bool  IsCommandNeeded(
        const DiSEqCDevSettings &/*settings*/, const DTVMultiplex &/*tuning*/)
                                       const { return false;         }
    virtual uint  GetVoltage(
        const DiSEqCDevSettings &/*settings*/, const DTVMultiplex &/*tuning*/) const = 0;

    // Non-const Gets
    DiSEqCDevDevice *FindDevice(uint dev_id);
    virtual DiSEqCDevDevice *GetSelectedChild(
        const DiSEqCDevSettings &/*settings*/) const     { return nullptr; }
    virtual DiSEqCDevDevice *GetChild(uint /*ordinal*/)  { return nullptr; }

    // Statics
    static QString DevTypeToString(dvbdev_t type)
        { return TableToString((uint)type, kDvbdevLookup); }
    static dvbdev_t DevTypeFromString(const QString &type)
        { return (dvbdev_t) TableFromString(type, kDvbdevLookup); }

    static DiSEqCDevDevice *CreateById(  DiSEqCDevTree &tree,
                                      uint        devid);
    static DiSEqCDevDevice *CreateByType(DiSEqCDevTree &tree,
                                      dvbdev_t    type,
                                      uint        dev_id = 0);

  protected:
    void SetDeviceID(uint devid)       const { m_devid    = devid;   }

    mutable uint     m_devid;
    dvbdev_t         m_devType  {kTypeLNB};
    QString          m_desc;
    DiSEqCDevTree   &m_tree;
    DiSEqCDevDevice *m_parent   {nullptr};
    uint             m_ordinal  {0};
    uint             m_repeat   {1};

    struct TypeTable { QString name; uint value; };
    using TypeTableVec = std::vector<TypeTable>;
    static QString TableToString(uint type, const TypeTableVec &table);
    static uint    TableFromString(const QString  &type,
                                   const TypeTableVec &table);

  private:
    static const TypeTableVec kDvbdevLookup;
};

class DiSEqCDevSwitch : public DiSEqCDevDevice
{
  public:
    DiSEqCDevSwitch(DiSEqCDevTree &tree, uint devid);
    ~DiSEqCDevSwitch() override;

    // Commands
    void Reset(void) override; // DiSEqCDevDevice
    bool Execute(const DiSEqCDevSettings &/*settings*/,
                 const DTVMultiplex &/*tuning*/) override; // DiSEqCDevDevice
    bool Load(void) override; // DiSEqCDevDevice
    bool Store(void) const override; // DiSEqCDevDevice

    // Sets
    enum dvbdev_switch_t : std::uint8_t
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
    bool SetChild(uint ordinal, DiSEqCDevDevice *device) override; // DiSEqCDevDevice

    // Gets
    dvbdev_switch_t GetType(void)       const { return m_type;       }
    uint            GetAddress(void)    const { return m_address;    }
    uint            GetNumPorts(void)   const { return m_numPorts;   }
    bool            ShouldSwitch(const DiSEqCDevSettings &settings,
                                 const DTVMultiplex &tuning) const;
    uint    GetChildCount(void) const override; // DiSEqCDevDevice
    bool    IsCommandNeeded(const DiSEqCDevSettings &settings,
                            const DTVMultiplex &tuning) const override; // DiSEqCDevDevice
    uint    GetVoltage(const DiSEqCDevSettings &settings,
                       const DTVMultiplex &tuning) const override; // DiSEqCDevDevice

    // Non-const Gets
    DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings &settings) const override; // DiSEqCDevDevice
    DiSEqCDevDevice *GetChild(uint ordinal) override; // DiSEqCDevDevice

    // Statics
    static QString SwitchTypeToString(dvbdev_switch_t type)
        { return TableToString((uint)type, kSwitchTypeTable); }
    static dvbdev_switch_t SwitchTypeFromString(const QString &type)
        { return (dvbdev_switch_t) TableFromString(type, kSwitchTypeTable); }


  protected:
    bool ExecuteLegacy(
        const DiSEqCDevSettings &settings, const DTVMultiplex &tuning, uint pos);
    bool ExecuteTone(
        const DiSEqCDevSettings &settings, const DTVMultiplex &tuning, uint pos);
    bool ExecuteVoltage(
        const DiSEqCDevSettings &settings, const DTVMultiplex &tuning, uint pos);
    bool ExecuteMiniDiSEqC(
        const DiSEqCDevSettings &settings, const DTVMultiplex &tuning, uint pos);
    bool ExecuteDiseqc(
        const DiSEqCDevSettings &settings, const DTVMultiplex &tuning, uint pos);

    int  GetPosition(  const DiSEqCDevSettings &settings) const;

  private:
    dvbdev_switch_t m_type            {kTypeTone};
    uint            m_address         {0x10}; //DISEQC_ADR_SW_ALL
    uint            m_numPorts        {2};
    uint            m_lastPos         {UINT_MAX};
    uint            m_lastHighBand    {UINT_MAX};
    uint            m_lastHorizontal  {UINT_MAX};
    dvbdev_vec_t    m_children;

    static const TypeTableVec kSwitchTypeTable;
};

class DiSEqCDevRotor : public DiSEqCDevDevice
{
  public:
    DiSEqCDevRotor(DiSEqCDevTree &tree, uint devid)
        : DiSEqCDevDevice(tree, devid) { DiSEqCDevRotor::Reset(); }
    ~DiSEqCDevRotor() override;

    // Commands
    void Reset(void) override; // DiSEqCDevDevice
    bool Execute(const DiSEqCDevSettings &settings, const DTVMultiplex &tuning) override; // DiSEqCDevDevice
    bool Load(void) override; // DiSEqCDevDevice
    bool Store(void) const override; // DiSEqCDevDevice

    // Sets
    enum dvbdev_rotor_t : std::uint8_t { kTypeDiSEqC_1_2 = 0, kTypeDiSEqC_1_3 = 1, };
    void   SetType(dvbdev_rotor_t type)    { m_type     = type;  }
    void   SetLoSpeed(double speed)        { m_speedLo = speed;  }
    void   SetHiSpeed(double speed)        { m_speedHi = speed;  }
    void   SetPosMap(const uint_to_dbl_t &posmap);
    bool SetChild(uint ordinal, DiSEqCDevDevice* device) override; // DiSEqCDevDevice
    void   RotationComplete(void) const;

    // Gets
    dvbdev_rotor_t GetType(void)         const { return m_type;      }
    double         GetLoSpeed(void)      const { return m_speedLo;   }
    double         GetHiSpeed(void)      const { return m_speedHi;   }
    uint_to_dbl_t  GetPosMap(void)       const;
    double         GetProgress(void)     const;
    bool           IsPositionKnown(void) const;
    uint   GetChildCount(void)           const override // DiSEqCDevDevice
        { return 1; }
    bool   IsCommandNeeded(const DiSEqCDevSettings &settings,
                           const DTVMultiplex &tuning) const override; // DiSEqCDevDevice
    bool           IsMoving(const DiSEqCDevSettings &settings) const;
    uint   GetVoltage(const DiSEqCDevSettings &settings,
                      const DTVMultiplex &tuning) const override; // DiSEqCDevDevice

    // Non-const Gets
    DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings &setting) const override; // DiSEqCDevDevice
    DiSEqCDevDevice *GetChild(uint /*ordinal*/) override // DiSEqCDevDevice
        { return m_child;     }

    // Statics
    static QString RotorTypeToString(dvbdev_rotor_t type)
        { return TableToString((uint)type, kRotorTypeTable); }
    static dvbdev_rotor_t RotorTypeFromString(const QString &type)
        { return (dvbdev_rotor_t) TableFromString(type, kRotorTypeTable); }

  protected:
    bool   ExecuteRotor(const DiSEqCDevSettings &settings, const DTVMultiplex &tuning,
                        double angle);
    bool   ExecuteUSALS(const DiSEqCDevSettings &settings, const DTVMultiplex &tuning,
                        double angle);
    void   StartRotorPositionTracking(double azimuth);

    static double CalculateAzimuth(double angle);
    double GetApproxAzimuth(void) const;

  private:
    // configuration
    dvbdev_rotor_t    m_type            {kTypeDiSEqC_1_3};
    double            m_speedHi         {2.5};
    double            m_speedLo         {1.9};
    dbl_to_uint_t     m_posmap;
    DiSEqCDevDevice  *m_child           {nullptr};

    // state
    double            m_lastPosition    {0.0};
    double            m_desiredAzimuth  {0.0};
    bool              m_reset           {true};

    // rotor position tracking state
    mutable double m_moveTime           {0.0};
    mutable bool   m_lastPosKnown       {false};
    mutable double m_lastAzimuth        {0.0};

    // statics
    static const TypeTableVec kRotorTypeTable;
};

class DiSEqCDevSCR : public DiSEqCDevDevice
{
  public:
    DiSEqCDevSCR(DiSEqCDevTree &tree, uint devid)
        : DiSEqCDevDevice(tree, devid) { DiSEqCDevSCR::Reset(); }
    ~DiSEqCDevSCR() override;

    // Commands
    void Reset(void) override; // DiSEqCDevDevice
    bool Execute(const DiSEqCDevSettings &settings, const DTVMultiplex &tuning) override; // DiSEqCDevDevice
    bool PowerOff(void) const;
    bool Load(void) override; // DiSEqCDevDevice
    bool Store(void) const override; // DiSEqCDevDevice

    // Sets
    enum dvbdev_pos_t : std::uint8_t
    {
        kTypeScrPosA               = 0,
        kTypeScrPosB               = 1,
    };
    void SetUserBand(uint userband)        { m_scrUserband  = userband;    }
    void SetFrequency(uint freq)           { m_scrFrequency = freq;        }
    void SetPIN(int pin)                   { m_scrPin       = pin;         }
    bool SetChild(uint ordinal, DiSEqCDevDevice* device) override; // DiSEqCDevDevice

    // Gets
    uint         GetUserBand(void) const   { return m_scrUserband;         }
    uint         GetFrequency(void) const  { return m_scrFrequency;        }
    int          GetPIN(void) const        { return m_scrPin;              }
    uint GetChildCount(void) const  override // DiSEqCDevDevice
        { return 1; }
    bool IsCommandNeeded(const DiSEqCDevSettings &/*settings*/,
                         const DTVMultiplex &/*tuning*/) const  override // DiSEqCDevDevice
        { return false; }
    uint GetVoltage(const DiSEqCDevSettings &settings,
                    const DTVMultiplex &tuning) const override; // DiSEqCDevDevice
    uint32_t     GetIntermediateFrequency(uint32_t frequency) const;

    // Non-const Gets
    DiSEqCDevDevice *GetSelectedChild(const DiSEqCDevSettings &/*settings*/) const override // DiSEqCDevDevice
        { return m_child; }
    DiSEqCDevDevice *GetChild(uint /*ordinal*/)  override // DiSEqCDevDevice
        { return m_child; }

    // statics
    static QString SCRPositionToString(dvbdev_pos_t pos)
        { return TableToString((uint)pos, kSCRPositionTable); }

    static dvbdev_pos_t SCRPositionFromString(const QString &pos)
        { return (dvbdev_pos_t) TableFromString(pos, kSCRPositionTable); }

  protected:
    bool         SendCommand(uint cmd, uint repeats, cmd_vec_t &data) const;

  private:
    uint             m_scrUserband   {0};    /* 0-7 */
    uint             m_scrFrequency  {1210};
    int              m_scrPin        {-1};   /* 0-255, -1=disabled */

    DiSEqCDevDevice *m_child         {nullptr};

    static const TypeTableVec kSCRPositionTable;
};

class DiSEqCDevLNB : public DiSEqCDevDevice
{
  public:
    DiSEqCDevLNB(DiSEqCDevTree &tree, uint devid)
        : DiSEqCDevDevice(tree, devid) { DiSEqCDevLNB::Reset(); }

    // Commands
    bool Execute(const DiSEqCDevSettings &settings, const DTVMultiplex &tuning) override; // DiSEqCDevDevice
    bool Load(void) override; // DiSEqCDevDevice
    bool Store(void) const override; // DiSEqCDevDevice

    // Sets
    enum dvbdev_lnb_t : std::uint8_t
    {
        kTypeFixed                 = 0,
        kTypeVoltageControl        = 1,
        kTypeVoltageAndToneControl = 2,
        kTypeBandstacked           = 3,
    };
    void SetType(dvbdev_lnb_t type)       { m_type       = type;       }
    void SetLOFSwitch(uint lof_switch)    { m_lofSwitch = lof_switch;  }
    void SetLOFHigh(  uint lof_hi)        { m_lofHi     = lof_hi;      }
    void SetLOFLow(   uint lof_lo)        { m_lofLo     = lof_lo;      }
    void SetPolarityInverted(bool inv)    { m_polInv    = inv;         }

    // Gets
    dvbdev_lnb_t GetType(void)      const { return m_type;             }
    uint         GetLOFSwitch(void) const { return m_lofSwitch;        }
    uint         GetLOFHigh(void)   const { return m_lofHi;            }
    uint         GetLOFLow(void)    const { return m_lofLo;            }
    bool         IsPolarityInverted(void) const { return m_polInv;     }
    bool         IsHighBand(const DTVMultiplex &tuning) const;
    bool         IsHorizontal(const DTVMultiplex &tuning) const;
    uint32_t     GetIntermediateFrequency(const DiSEqCDevSettings& settings,
                                          const DTVMultiplex &tuning) const;
    uint GetVoltage(const DiSEqCDevSettings &settings,
                    const DTVMultiplex &tuning) const override; // DiSEqCDevDevice

    // statics
    static QString LNBTypeToString(dvbdev_lnb_t type)
        { return TableToString((uint)type, kLNBTypeTable); }

    static dvbdev_lnb_t LNBTypeFromString(const QString &type)
        { return (dvbdev_lnb_t) TableFromString(type, kLNBTypeTable); }

  private:
    dvbdev_lnb_t m_type       {kTypeVoltageAndToneControl};
    uint         m_lofSwitch  {11700000};
    uint         m_lofHi      {10600000};
    uint         m_lofLo      { 9750000};
    /// If a signal is circularly polarized the polarity will flip
    /// on each reflection, so antenna systems with an even number
    /// of reflectors will need to set this value.
    bool         m_polInv     {false};

    static const TypeTableVec kLNBTypeTable;
};

#endif // DISEQC_H
