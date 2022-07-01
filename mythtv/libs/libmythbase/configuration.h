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

    QString      m_path     {GetConfDir()};
    QString      m_fileName {k_default_filename};

    QDomDocument m_config;
    QDomNode     m_rootNode;

    QDomNode FindNode(const QString &name, bool create);
    QDomNode FindNode(QStringList &path, QDomNode &current, bool create);

  public:

    static constexpr auto k_default_filename = "config.xml";

    XmlConfiguration() { Load(); }
    explicit XmlConfiguration(QString fileName)
        : m_fileName(std::move(fileName))
    {
        Load();
    }

    bool Load();
    bool Save();

    int     GetValue(const QString &setting, int defaultValue);
    QString GetValue(const QString &setting, const QString &defaultValue);
    bool    GetBoolValue(const QString &setting, bool defaultValue)
        {return static_cast<bool>(GetValue(setting, static_cast<int>(defaultValue)));}

    void    SetValue(const QString &setting, int value);
    void    SetValue(const QString &setting, const QString &value);
    void    ClearValue(const QString &setting);
    void    SetBoolValue(const QString &setting, bool value)
        {SetValue(setting, static_cast<int>(value));}

    template <typename T>
    typename std::enable_if_t<std::chrono::__is_duration<T>::value, T>
    GetDuration(const QString &setting, T defaultValue = T::zero())
    {
        return T(GetValue(setting, static_cast<int>(defaultValue.count())));
    }

    template <typename T>
    typename std::enable_if_t<std::chrono::__is_duration<T>::value>
    SetDuration(const QString &setting, T value)
    {
        SetValue(setting, static_cast<int>(value.count()));
    }
};

#endif // CONFIGURATION_H
