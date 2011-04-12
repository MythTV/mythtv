//////////////////////////////////////////////////////////////////////////////
// Program Name: soapSerializer.h
// Created     : Dec. 30, 2009
//
// Purpose     : Serializer Implementation for SOAP 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __SOAPSERIALIZER_H__
#define __SOAPSERIALIZER_H__

#include "upnpexp.h"
#include "xmlSerializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SoapSerializer : public XmlSerializer
{
    public:

        SoapSerializer( QIODevice     *pDevice, 
                        const QString &sNamespace, 
                        const QString &sRequestName ) : XmlSerializer( pDevice, sRequestName ) 
        {
            m_sNamespace = sNamespace;
        }

        virtual ~SoapSerializer() {};

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

        virtual void AddHeaders( QStringMap &headers )
        {
            XmlSerializer::AddHeaders( headers );

            if (!headers.contains( "EXT" ))
                headers.insert( "EXT", "" );
        }


    protected:

        QString     m_sNamespace;

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

        virtual void BeginSerialize( QString &sName) 
        {
            m_pXmlWriter->writeStartDocument( "1.0" );

            m_pXmlWriter->writeStartElement("http://schemas.xmlsoap.org/soap/envelope/", "Envelope" );
           // m_pXmlWriter->writeAttribute( "encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/" );

            m_pXmlWriter->writeStartElement( "http://schemas.xmlsoap.org/soap/envelope/", "Body" );

            m_pXmlWriter->writeStartElement( m_sRequestName + "Response" );
            m_pXmlWriter->writeAttribute( "xmlns", m_sNamespace );

            sName = m_sRequestName + "Result";
        }
};

#endif

