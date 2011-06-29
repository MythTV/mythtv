//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.h
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
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

#ifndef SOAPCLIENT_H_
#define SOAPCLIENT_H_

#include <QDomDocument>
#include <QString>
#include <QUrl>

#include "httpcomms.h"
#include "upnputil.h"
#include "upnpexp.h"

/// \brief Subclass SOAPClient to perform actions using the command URL.
class UPNP_PUBLIC SOAPClient
{
  public:
    SOAPClient(const QUrl    &url,
               const QString &sNamespace,
               const QString &sControlPath);
    /// \brief Empty SOAPClient constructor. When this is used, Init()
    ///        Must be called before SendSOAPRequest().
    SOAPClient() {}
    virtual ~SOAPClient() {}

    bool Init(const QUrl    &url,
              const QString &sNamespace,
              const QString &sControlPath);

  protected:
    int      GetNodeValue(const QDomNode &node,
                          const QString  &sName,
                          int             nDefault) const;
    bool     GetNodeValue(const QDomNode &node,
                          const QString  &sName,
                          bool            bDefault) const;
    QString  GetNodeValue(const QDomNode &node,
                          const QString  &sName,
                          const QString  &sDefault) const;

    QDomNode FindNode(const QString  &sName,
                      const QDomNode &baseNode) const;

    QDomDocument SendSOAPRequest(const QString &sMethod,
                                 QStringMap    &list,
                                 int           &nErrCode,
                                 QString       &sErrDesc,
                                 bool           bInQtThread);
  private:
    QDomNode FindNodeInternal(QStringList    &sParts,
                              const QDomNode &curNode) const;
  protected:
    QUrl    m_url;
    QString m_sNamespace;
    QString m_sControlPath;
};

#endif

