#ifndef MYTHDRMPROPERTY_H
#define MYTHDRMPROPERTY_H

// MythTV
#include "libmythui/platforms/drm/mythdrmresources.h"

using DRMProp  = std::shared_ptr<class MythDRMProperty>;
using DRMProps = std::vector<DRMProp>;

class MUI_PUBLIC MythDRMProperty
{
  public:
    enum Type
    {
        Invalid = 0,
        Range,
        Enum,
        Bitmask,
        Blob,
        Object,
        SignedRange
    };

    static  DRMProps GetProperties(int FD, uint32_t ObjectId, uint32_t ObjectType);
    static  DRMProp  GetProperty  (const QString& Name, const DRMProps& Properties);
    virtual ~MythDRMProperty() = default;
    virtual QString ToString() { return ""; }

    Type     m_type     { Invalid };
    uint32_t m_id       { 0 };
    bool     m_readOnly { true };
    bool     m_atomic   { false };
    QString  m_name;

  protected:
    MythDRMProperty(Type mType, uint32_t Id, uint32_t Flags, const char * Name);

  private:
    Q_DISABLE_COPY(MythDRMProperty)
};

class MUI_PUBLIC MythDRMRangeProperty : public MythDRMProperty
{
   public:
    MythDRMRangeProperty(uint64_t Value, drmModePropertyPtr Property);
    QString ToString() override;
    uint64_t m_value { 0 };
    uint64_t m_min   { 0 };
    uint64_t m_max   { 0 };
};

class MUI_PUBLIC MythDRMSignedRangeProperty : public MythDRMProperty
{
   public:
    MythDRMSignedRangeProperty(uint64_t Value, drmModePropertyPtr Property);
    QString ToString() override;
    int64_t m_value { 0 };
    int64_t m_min   { 0 };
    int64_t m_max   { 0 };
};

class MUI_PUBLIC MythDRMEnumProperty : public MythDRMProperty
{
  public:
    MythDRMEnumProperty(uint64_t Value, drmModePropertyPtr Property);
    QString ToString() override;
    uint64_t m_value { 0 };
    std::map<uint64_t,QString> m_enums;
};

class MUI_PUBLIC MythDRMBitmaskProperty : public MythDRMEnumProperty
{
  public:
    MythDRMBitmaskProperty(uint64_t Value, drmModePropertyPtr Property);
};

class MUI_PUBLIC MythDRMBlobProperty : public MythDRMProperty
{
  public:
    MythDRMBlobProperty(int FD, uint64_t Value, drmModePropertyPtr Property);
    QByteArray m_blob;
};

class MUI_PUBLIC MythDRMObjectProperty : public MythDRMProperty
{
  public:
    MythDRMObjectProperty(drmModePropertyPtr Property);
};

#endif
