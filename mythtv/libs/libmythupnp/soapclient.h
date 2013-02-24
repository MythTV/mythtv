//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.h
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SOAPCLIENT_H_
#define SOAPCLIENT_H_

#include <QDomDocument>
#include <QString>
#include <QUrl>

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
    QString  GetNodeValue(const QDomNode &node,
                          const QString  &sName,
                          const char *sDefault) const
    {
        return GetNodeValue(node, sName, QString(sDefault));
    }

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

