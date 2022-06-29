//////////////////////////////////////////////////////////////////////////////
// Program Name: configuration.h
// Created     : Feb. 12, 2007
//
// Purpose     : XML Configuration file Class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <utility>

#include <QDomDocument>
#include <QStringList>
#include "libmythbase/mythbaseexp.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythdirs.h"

//////////////////////////////////////////////////////////////////////////////
//
// **NOTE:  Performance Issue *** 
//
// This class loads an XML file into a QDomDocument.  All requests for 
// getting or setting values navigates the DOM each time.  All settings should 
// only be read/written once if possible.
//
//////////////////////////////////////////////////////////////////////////////

class MBASE_PUBLIC XmlConfiguration
{
    protected:

        QString      m_sPath     {GetConfDir()};
        QString      m_sFileName {k_default_filename};

        QDomDocument m_config;
        QDomNode     m_rootNode;

        QDomNode FindNode( const QString &sName, bool bCreate = false );
        QDomNode FindNode( QStringList &sParts, QDomNode &curNode, bool bCreate = false );

    public:
        static constexpr auto k_default_filename = "config.xml";

        XmlConfiguration() { Load(); }
        explicit XmlConfiguration(QString fileName)
            : m_sFileName(std::move(fileName))
        {
            Load();
        }

        bool    Load();
        bool    Save();

        int     GetValue(const QString &sSetting, int     Default);
        QString GetValue(const QString &sSetting, const QString &Default);
        bool    GetBoolValue(const QString &sSetting, bool Default )
            {return static_cast<bool>(GetValue(sSetting, static_cast<int>(Default))); }

        void    SetValue(const QString &sSetting, int value);
        void    SetValue(const QString &sSetting, const QString &value);
        void    ClearValue(const QString &sSetting );
        void    SetBoolValue(const QString &sSetting, bool value )
            {SetValue(sSetting, static_cast<int>(value)); }

        template <typename T>
        typename std::enable_if_t<std::chrono::__is_duration<T>::value, T>
        GetDuration(const QString &sSetting, T defaultval = T::zero())
        {
            return T(GetValue(sSetting, static_cast<int>(defaultval.count())));
        }

        template <typename T>
        typename std::enable_if_t<std::chrono::__is_duration<T>::value>
        SetDuration(const QString &sSetting, T value)
        {
            SetValue(sSetting, static_cast<int>(value.count()));
        }
};

#endif // CONFIGURATION_H
