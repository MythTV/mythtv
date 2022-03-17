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

#include <QDomDocument>
#include <QStringList>
#include "libmythbase/mythbaseexp.h"
#include "libmythbase/mythchrono.h"

class MBASE_PUBLIC Configuration
{
    public:

        virtual ~Configuration() = default;

        virtual bool    Load    ( void ) = 0;
        virtual bool    Save    ( void ) = 0;

        virtual int     GetValue( const QString &sSetting, int     Default ) = 0; 
        virtual QString GetValue( const QString &sSetting, const QString &Default ) = 0;
        virtual bool    GetBoolValue( const QString &sSetting, bool Default ) = 0;

        virtual void    SetValue( const QString &sSetting, int     value   ) = 0; 
        virtual void    SetValue( const QString &sSetting, const QString &value ) = 0;
        virtual void    ClearValue( const QString &sSetting )                = 0;
        virtual void    SetBoolValue( const QString &sSetting, bool value   ) = 0;

        template <typename T>
            typename std::enable_if<std::chrono::__is_duration<T>::value, T>::type
            GetDuration(const QString &sSetting, T defaultval = T::zero())
        { return T(GetValue(sSetting, static_cast<int>(defaultval.count()))); }
        template <typename T>
            typename std::enable_if<std::chrono::__is_duration<T>::value, void>::type
            SetDuration(const QString &sSetting, T value)
        { SetValue(sSetting, static_cast<int>(value.count())); }
};


//////////////////////////////////////////////////////////////////////////////
//
// **NOTE:  Performance Issue *** 
//
// This class loads an XML file into a QDomDocument.  All requests for 
// getting or setting values navigates the DOM each time.  All settings should 
// only be read/written once if possible.
//
//////////////////////////////////////////////////////////////////////////////

class MBASE_PUBLIC XmlConfiguration : public Configuration
{
    protected:

        QString      m_sPath;
        QString      m_sFileName;

        QDomDocument m_config;
        QDomNode     m_rootNode;

        QDomNode FindNode( const QString &sName, bool bCreate = false );
        QDomNode FindNode( QStringList &sParts, QDomNode &curNode, bool bCreate = false );

    public:

        explicit XmlConfiguration( const QString &sFileName );

        ~XmlConfiguration() override = default;

        bool    Load    ( void ) override; // Configuration
        bool    Save    ( void ) override; // Configuration

        int     GetValue( const QString &sSetting, int     Default ) override; // Configuration
        QString GetValue( const QString &sSetting, const QString &Default ) override; // Configuration
        bool    GetBoolValue( const QString &sSetting, bool Default ) override // Configuration
            {return static_cast<bool>(GetValue(sSetting, static_cast<int>(Default))); }

        void    SetValue( const QString &sSetting, int     value   ) override; // Configuration
        void    SetValue( const QString &sSetting, const QString &value   ) override; // Configuration
        void    ClearValue( const QString &sSetting ) override; // Configuration
        void    SetBoolValue( const QString &sSetting, bool value ) override // Configuration
            {SetValue(sSetting, static_cast<int>(value)); }
};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class MBASE_PUBLIC DBConfiguration : public Configuration
{
    public:

        DBConfiguration() = default;

        ~DBConfiguration() override = default;

        bool    Load    ( void ) override; // Configuration
        bool    Save    ( void ) override; // Configuration

        int     GetValue( const QString &sSetting, int     Default ) override; // Configuration
        QString GetValue( const QString &sSetting, const QString &Default ) override; // Configuration
        bool    GetBoolValue( const QString &sSetting, bool Default ) override // Configuration
        {return static_cast<bool>(GetValue(sSetting, static_cast<int>(Default))); }

        void    SetValue( const QString &sSetting, int     value   ) override; // Configuration
        void    SetValue( const QString &sSetting, const QString &value   ) override; // Configuration
        void    ClearValue( const QString &sSetting ) override; // Configuration
        void    SetBoolValue( const QString &sSetting, bool value ) override // Configuration
            {SetValue(sSetting, static_cast<int>(value)); }
};

#endif // CONFIGURATION_H
