#ifndef MYTHDRMCONNECTOR_H
#define MYTHDRMCONNECTOR_H

// MythTV
#include "libmythui/platforms/drm/mythdrmmode.h"
#include "libmythui/platforms/drm/mythdrmproperty.h"
#include "libmythui/platforms/drm/mythdrmresources.h"

using Encoders = std::vector<uint32_t>;
using DRMConn  = std::shared_ptr<class MythDRMConnector>;
using DRMConns = std::vector<DRMConn>;

class MUI_PUBLIC MythDRMConnector
{
  public:
    static DRMConn  Create(int FD, uint32_t Id);
    static DRMConn  GetConnector(const DRMConns& Connectors, uint32_t Id);
    static DRMConns GetConnectors(int FD);
    static QString  GetConnectorName(uint32_t Type, uint32_t Id);
    static DRMConn  GetConnectorByName(const DRMConns& Connectors, const QString& Name);
    bool Connected() const;

    uint32_t m_id             { 0 };
    uint32_t m_encoderId      { 0 };
    uint32_t m_type           { DRM_MODE_CONNECTOR_Unknown };
    uint32_t m_typeId         { 0 };
    QString  m_name;
    drmModeConnection m_state { DRM_MODE_UNKNOWNCONNECTION };
    uint32_t m_mmWidth        { 0 };
    uint32_t m_mmHeight       { 0 };
    DRMModes m_modes;
    DRMProps m_properties;

  protected:
    MythDRMConnector(int FD, uint32_t Id);

  private:
    Q_DISABLE_COPY(MythDRMConnector)
};

#endif
