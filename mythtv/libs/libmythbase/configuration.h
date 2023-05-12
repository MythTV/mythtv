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
#include <QDomNode>
#include <QString>
#include <QStringList>
#include <QVariant>

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
    QString      m_fileName {kDefaultFilename};

    QDomDocument m_config;
    QDomNode     m_rootNode;

    QDomNode FindNode(const QString &setting, bool create);
    QDomNode FindNode(QStringList &path, QDomNode &current, bool create);

    QString GetValue(const QString &setting);

  public:

    static constexpr auto kDefaultFilename = "config.xml";

    XmlConfiguration() { Load(); }
    explicit XmlConfiguration(QString fileName)
        : m_fileName(std::move(fileName))
    {
        Load();
    }

    bool Load();
    bool Save();

    bool FileExists();

    bool GetValue(const QString &setting, const bool defaultValue)
    {
        QString value = GetValue(setting);
        return (!value.isNull()) ? QVariant(value).toBool() : defaultValue;
    }
    int  GetValue(const QString &setting, const int defaultValue)
    {
        QString value = GetValue(setting);
        return (!value.isNull()) ? QVariant(value).toInt()  : defaultValue;
    }
    QString GetValue(const QString &setting, const QString &defaultValue)
    {
        QString value = GetValue(setting);
        return (!value.isNull()) ? value : defaultValue;
    }
    /** @brief get the string value stored as setting with a default C string.
    This fixes overload resolution, making it choose this instead of the bool one.
    */
    QString GetValue(const QString &setting, const char* defaultValue)
    {
        QString value = GetValue(setting);
        return (!value.isNull()) ? value : QString(defaultValue);
    }

    void SetValue(const QString &setting, bool value)
    {
        SetValue(setting, QVariant(value).toString());
    }
    void SetValue(const QString &setting, int  value)
    {
        SetValue(setting, QVariant(value).toString());
    }
    /** @brief set value to a C string; just in case, for overload resolution
    @note: This should probably never be used since the C string should have already
           been converted to a QString.
    */
    void SetValue(const QString &setting, const char* value)
    {
        SetValue(setting, QString(value));
    }
    void    SetValue(const QString &setting, const QString &value);
    void    ClearValue(const QString &setting);

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
