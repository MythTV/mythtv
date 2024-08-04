#ifndef MYTHVRR_H
#define MYTHVRR_H

// Qt
#include <QString>

// Std
#include <cstdint>
#include <memory>
#include <tuple>

using MythVRRPtr = std::shared_ptr<class MythVRR>;
using MythVRRRange = std::tuple<int,int,bool>;

class MythVRR
{
  public:
    enum VRRType : std::uint8_t
    {
        Unknown = 0,
        FreeSync,
        GSync,
        GSyncCompat
    };

    static MythVRRPtr Create(class MythDisplay* MDisplay);
    virtual ~MythVRR() = default;
    virtual void SetEnabled(bool Enable = true) = 0;
    QString      TypeToString() const;
    bool         Enabled() const;
    MythVRRRange GetRange() const;
    QString      RangeDescription() const;
    bool         IsControllable() const;

  protected:
    MythVRR(bool Controllable, VRRType Type, bool Enabled, MythVRRRange Range);

    bool    m_controllable { false };
    VRRType m_type         { Unknown };
    bool    m_enabled      { false };
    MythVRRRange m_range   { 0, 0, false };
};

#endif
