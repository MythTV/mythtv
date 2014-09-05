#include "upnpserviceimpl.h"
#include "upnpdevice.h"

/// Creates a UPnpService and adds it to the UPnpDevice's list of services.
void UPnpServiceImpl::RegisterService(UPnpDevice *pDevice)
{
    if (pDevice != NULL)
    {
        UPnpService *pService = new UPnpService();
            
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
    xml = "<" + m_name;
    NameValues::iterator ait;
    for (ait = m_attributes.begin(); ait != m_attributes.end(); ++ait)
    {
        xml += QString(" %1=\"%2\"").arg((*ait).sName).arg((*ait).sValue);
    }
    xml += ">";
    NameValues::iterator pit;
    for (pit = m_properties.begin(); pit != m_properties.end(); ++pit)
    {
        xml += "    " + (*pit).toXML();
    }
    xml += "</" + m_name + ">";
    return xml;
}

UPnPFeatureList::UPnPFeatureList()
{
    m_attributes.append(NameValue( "xmlns",
                                   "urn:schemas-upnp-org:av:cm-featureList" ));
    m_attributes.append(NameValue( "xmlns:xsi",
                                   "http://www.w3.org/2001/XMLSchema-instance" ));
    m_attributes.append(NameValue( "xsi:schemaLocation",
                                   "urn:schemas-upnp-org:av:cm-featureList "
                                   "http://www.upnp.org/schemas/av/cm-featureList.xsd" ));
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


QString UPnPFeatureList::toXML()
{
    QString xml;

    xml = "<Features ";
    NameValues::iterator ait;
    for (ait = m_attributes.begin(); ait != m_attributes.end(); ++ait)
    {
        xml += QString(" %1=\"%2\"").arg((*ait).sName).arg((*ait).sValue);
    }
    xml += ">";
    QList<UPnPFeature*>::iterator fit;
    for (fit = m_features.begin(); fit != m_features.end(); ++fit)
    {
        xml += "    " + (*fit)->toXML();
    }
    xml += "</Features>";
    return xml;
}
