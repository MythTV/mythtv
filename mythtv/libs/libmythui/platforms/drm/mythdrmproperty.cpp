// MythTV
#include "platforms/drm/mythdrmproperty.h"

// C++
#include <algorithm>

// Qt
#include <QStringList>

/*! \class MythDRMProperty
 * \brief A wrapper around a DRM object property.
 *
 * Retrieve a list of the properties available for an object with GetProperties
 * and retrieve a single, named property from a given list with GetProperty.
 *
 * \note Property values represent the initial property state and will not reflect
 * subsequent changes unless explicitly updated.
*/
MythDRMProperty::MythDRMProperty(Type mType, uint32_t Id, uint32_t Flags, const char * Name)
  : m_type(mType),
    m_id(Id),
    m_readOnly(Flags & DRM_MODE_PROP_IMMUTABLE),
    m_atomic(Flags & DRM_MODE_PROP_ATOMIC),
    m_name(Name)
{
}

DRMProps MythDRMProperty::GetProperties(int FD, const uint32_t ObjectId, uint32_t ObjectType)
{
    DRMProps result;
    if (auto * props = drmModeObjectGetProperties(FD, ObjectId, ObjectType); props)
    {
        for (uint32_t index = 0; index < props->count_props; ++index)
        {
            if (auto * prop = drmModeGetProperty(FD, props->props[index]); prop)
            {
                auto value = props->prop_values[index];
                if (prop->flags & DRM_MODE_PROP_RANGE)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMRangeProperty(value, prop)));
                else if (prop->flags & DRM_MODE_PROP_ENUM)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMEnumProperty(value, prop)));
                else if (prop->flags & DRM_MODE_PROP_BITMASK)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMBitmaskProperty(value, prop)));
                else if (prop->flags & DRM_MODE_PROP_BLOB)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMBlobProperty(FD, value, prop)));
                else if (prop->flags & DRM_MODE_PROP_OBJECT)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMObjectProperty(prop)));
                else if (prop->flags & DRM_MODE_PROP_SIGNED_RANGE)
                    result.emplace_back(std::shared_ptr<MythDRMProperty>(new MythDRMSignedRangeProperty(value, prop)));
                drmModeFreeProperty(prop);
            }
        }
        drmModeFreeObjectProperties(props);
    }
    return result;
}

DRMProp MythDRMProperty::GetProperty(const QString& Name, const DRMProps &Properties)
{
    for (const auto & prop : Properties)
        if (Name.compare(prop->m_name, Qt::CaseInsensitive) == 0)
            return prop;

    return nullptr;
}

MythDRMRangeProperty::MythDRMRangeProperty(uint64_t Value, drmModePropertyPtr Property)
  : MythDRMProperty(Range, Property->prop_id, Property->flags, Property->name),
    m_value(Value)
{
    for (int i = 0; i < Property->count_values; ++i)
    {
        auto value = Property->values[i];
        m_min = std::min(value, m_min);
        m_max = std::max(value, m_max);
    }
}

QString MythDRMRangeProperty::ToString()
{
    return QString("%1 Value: %2 Range: %3<->%4").arg(m_name).arg(m_value).arg(m_min).arg(m_max);
}

MythDRMSignedRangeProperty::MythDRMSignedRangeProperty(uint64_t Value, drmModePropertyPtr Property)
  : MythDRMProperty(SignedRange, Property->prop_id, Property->flags, Property->name),
    m_value(static_cast<int64_t>(Value))
{
    for (int i = 0; i < Property->count_values; ++i)
    {
        auto value = static_cast<int64_t>(Property->values[i]);
        m_min = std::min(value, m_min);
        m_max = std::max(value, m_max);
    }
}

QString MythDRMSignedRangeProperty::ToString()
{
    return QString("%1 Value: %2 Range: %3<->%4").arg(m_name).arg(m_value).arg(m_min).arg(m_max);
}

MythDRMEnumProperty::MythDRMEnumProperty(uint64_t Value, drmModePropertyPtr Property)
  : MythDRMProperty(Enum, Property->prop_id, Property->flags, Property->name),
    m_value(Value)
{
    for (int i = 0; i < Property->count_enums; ++i)
        m_enums.emplace(Property->enums[i].value, Property->enums[i].name);
}

QString MythDRMEnumProperty::ToString()
{
    QStringList values;
    for (const auto & value : m_enums)
        values.append(QString("%1(%2)").arg(value.first).arg(value.second));
    return QString("%1 Value: %2 Values: %3").arg(m_name).arg(m_value).arg(values.join(", "));
}

MythDRMBitmaskProperty::MythDRMBitmaskProperty(uint64_t Value, drmModePropertyPtr Property)
  : MythDRMEnumProperty(Value, Property)
{
    m_type = Bitmask;
}

MythDRMBlobProperty::MythDRMBlobProperty(int FD, uint64_t Value, drmModePropertyPtr Property)
  : MythDRMProperty(Blob, Property->prop_id, Property->flags, Property->name)
{
    if (auto * blob = drmModeGetPropertyBlob(FD, static_cast<uint32_t>(Value)); blob)
    {
        m_blob = QByteArray(reinterpret_cast<const char *>(blob->data), static_cast<int>(blob->length));
        drmModeFreePropertyBlob(blob);
    }
}

MythDRMObjectProperty::MythDRMObjectProperty(drmModePropertyPtr Property)
  : MythDRMProperty(Object, Property->prop_id, Property->flags, Property->name)
{
}
