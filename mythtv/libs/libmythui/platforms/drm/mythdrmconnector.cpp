#include "platforms/drm/mythdrmconnector.h"

#include <algorithm>

/*! \class MythDRMConnector
 * \brief A wrapper around a DRM connector object.
 *
 * The full list of available connectors can be retrieved with GetConnectors and
 * a single connector retrieved from a list with GetConnector.
*/
DRMConn MythDRMConnector::Create(int FD, uint32_t Id)
{
    if (FD && Id)
        if (auto c = std::shared_ptr<MythDRMConnector>(new MythDRMConnector(FD, Id)); c.get() && c->m_id)
            return c;

    return nullptr;
}

DRMConn MythDRMConnector::GetConnector(const DRMConns &Connectors, uint32_t Id)
{
    auto match = [&Id](const auto & Conn) { return Conn->m_id == Id; };
    if (auto found = std::find_if(Connectors.cbegin(), Connectors.cend(), match); found != Connectors.cend())
        return *found;
    return nullptr;
}

DRMConns MythDRMConnector::GetConnectors(int FD)
{
    DRMConns result;
    if (auto resources = MythDRMResources(FD); *resources)
    {
        for (auto i = 0; i < resources->count_connectors; ++i)
            if (auto connector = Create(FD, resources->connectors[i]); connector.get())
                result.emplace_back(connector);
    }
    return result;
}

MythDRMConnector::MythDRMConnector(int FD, uint32_t Id)
{
    if (auto * connector = drmModeGetConnector(FD, Id); connector)
    {
        m_id         = connector->connector_id;
        m_encoderId  = connector->encoder_id;
        m_type       = connector->connector_type;
        m_typeId     = connector->connector_type_id;
        m_name       = GetConnectorName(m_type, m_typeId);
        m_state      = connector->connection;
        m_mmWidth    = connector->mmWidth;
        m_mmHeight   = connector->mmHeight;
        m_properties = MythDRMProperty::GetProperties(FD, m_id, DRM_MODE_OBJECT_CONNECTOR);
        for (auto i = 0; i < connector->count_modes; ++i)
            m_modes.emplace_back(MythDRMMode::Create(&connector->modes[i], i));
        drmModeFreeConnector(connector);
    }
}

bool MythDRMConnector::Connected() const
{
    return m_state == DRM_MODE_CONNECTED;
}

QString MythDRMConnector::GetConnectorName(uint32_t Type, uint32_t Id)
{
    constexpr size_t count = DRM_MODE_CONNECTOR_DPI + 1;
    static const std::array<const QString,count> s_connectorNames
    {
        "None", "VGA", "DVI", "DVI",  "DVI",  "Composite", "TV", "LVDS",
        "CTV",  "DIN", "DP",  "HDMI", "HDMI", "TV", "eDP", "Virtual", "DSI", "DPI"
    };
    uint32_t type = std::min(Type, static_cast<uint32_t>(DRM_MODE_CONNECTOR_DPI));
    return QString("%1%2").arg(s_connectorNames[type]).arg(Id);
}

DRMConn MythDRMConnector::GetConnectorByName(const DRMConns& Connectors, const QString &Name)
{
    for (const auto & connector : Connectors)
        if (Name.compare(connector->m_name, Qt::CaseInsensitive) == 0)
            return connector;

    return nullptr;
}
