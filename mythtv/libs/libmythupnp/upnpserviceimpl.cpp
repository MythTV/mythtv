#include "upnpserviceimpl.h"
#include "upnpdevice.h"

/// Creates a UPnpService and adds it to the UPnpDevice's list of services.
void UPnpServiceImpl::RegisterService(UPnpDevice *pDevice)
{
    if (pDevice != nullptr)
    {
        auto *pService = new UPnpService();
            
        pService->m_sServiceType = GetServiceType();
        pService->m_sServiceId   = GetServiceId();
        pService->m_sSCPDURL     = GetServiceDescURL();
        pService->m_sControlURL  = GetServiceControlURL();
        pService->m_sEventSubURL = GetServiceEventURL();

        pDevice->m_listServices.push_back(pService);
    }
}

QString UPnPFeature::toXML()
{
    QString xml;
    xml = "<Feature name=\"" + m_name + "\" version=\"" + QString::number(m_version) + "\">\r\n";
    xml += CreateXML();
    xml += "</Feature>\r\n";
    return xml;
}

UPnPFeatureList::~UPnPFeatureList()
{
    while (!m_features.isEmpty())
        delete m_features.takeFirst();
}

void UPnPFeatureList::AddFeature(UPnPFeature *feature)
{
    m_features.append(feature);
}

void UPnPFeatureList::AddAttribute(const NameValue& attribute)
{
    m_attributes.append(attribute); 
}

QString UPnPFeatureList::toXML()
{
    QString xml;

    xml = "<Features ";
    NameValues::iterator ait;
    for (ait = m_attributes.begin(); ait != m_attributes.end(); ++ait)
    {
        xml += QString(" %1=\"%2\"").arg((*ait).m_sName, (*ait).m_sValue);
    }
    xml += ">\r\n";
    QList<UPnPFeature*>::iterator fit;
    for (fit = m_features.begin(); fit != m_features.end(); ++fit)
    {
        xml += "    " + (*fit)->toXML();
    }
    xml += "</Features>\r\n";
    return xml;
}
