// -*- Mode: c++ -*-

#include <QStringList>
#include <QDateTime>
#include <QSize>
#include <QMap>
#include <QString>
#include <QVariant>

#include <stdint.h>   // for uint64_t

#include "mythbaseexp.h"
#include "mythlogging.h"
#include "referencecounter.h"

class MythCommandLineParser;

class MBASE_PUBLIC CommandLineArg : public ReferenceCounter
{
  public:
    CommandLineArg(QString name, QVariant::Type type, QVariant def,
                   QString help, QString longhelp);
    CommandLineArg(QString name, QVariant::Type type, QVariant def);
    CommandLineArg(QString name);
   ~CommandLineArg() {};

    CommandLineArg* SetGroup(QString group)         { m_group = group;
                                                      return this; }
    void            AddKeyword(QString keyword)     { m_keywords << keyword; }

    QString         GetName(void) const             { return m_name; }
    QString         GetUsedKeyword(void) const      { return m_usedKeyword; }
    int             GetKeywordLength(void) const;
    QString         GetHelpString(int off, QString group = "",
                                  bool force = false) const;
    QString         GetLongHelpString(QString keyword) const;

    bool            Set(QString opt);
    bool            Set(QString opt, QByteArray val);
    void            Set(QVariant val)               { m_stored = val;
                                                      m_given = true; }

    CommandLineArg* SetParent(QString opt);
    CommandLineArg* SetParent(QStringList opts);
    CommandLineArg* SetParentOf(QString opt);
    CommandLineArg* SetParentOf(QStringList opts);

    CommandLineArg* SetChild(QString opt);
    CommandLineArg* SetChild(QStringList opt);
    CommandLineArg* SetChildOf(QString opt);
    CommandLineArg* SetChildOf(QStringList opts);

    CommandLineArg* SetRequiredChild(QString opt);
    CommandLineArg* SetRequiredChild(QStringList opt);
    CommandLineArg* SetRequiredChildOf(QString opt);
    CommandLineArg* SetRequiredChildOf(QStringList opt);

    CommandLineArg* SetRequires(QString opt);
    CommandLineArg* SetRequires(QStringList opts);
    CommandLineArg* SetBlocks(QString opt);
    CommandLineArg* SetBlocks(QStringList opts);

    CommandLineArg* SetDeprecated(QString depstr = "");
    CommandLineArg* SetRemoved(QString remstr = "", QString remver = "");

    static void     AllowOneOf(QList<CommandLineArg*> args);

    void            PrintVerbose(void) const;

    friend class MythCommandLineParser;

  private:
    QString GetKeywordString(void) const;

    void            SetParentOf(CommandLineArg *other, bool forward = true);
    void            SetChildOf(CommandLineArg *other, bool forward = true);
    void            SetRequires(CommandLineArg *other, bool forward = true);
    void            SetBlocks(CommandLineArg *other, bool forward = true);

    void            Convert(void);

    QString         GetPreferredKeyword(void) const;
    bool            TestLinks(void) const;
    void            CleanupLinks(void);

    void            PrintRemovedWarning(QString &keyword) const;
    void            PrintDeprecatedWarning(QString &keyword) const;

    bool                    m_given;
    bool                    m_converted;
    QString                 m_name;
    QString                 m_group;
    QString                 m_deprecated;
    QString                 m_removed;
    QString                 m_removedversion;
    QVariant::Type          m_type;
    QVariant                m_default;
    QVariant                m_stored;

    QStringList             m_keywords;
    QString                 m_usedKeyword;

    QList<CommandLineArg*>  m_parents;
    QList<CommandLineArg*>  m_children;
    QList<CommandLineArg*>  m_requires;
    QList<CommandLineArg*>  m_requiredby;
    QList<CommandLineArg*>  m_blocks;

    QString                 m_help;
    QString                 m_longhelp;
};

class MBASE_PUBLIC MythCommandLineParser
{
  public:
    MythCommandLineParser(QString);
   ~MythCommandLineParser();

    virtual void LoadArguments(void) {};
    void PrintVersion(void) const;
    void PrintHelp(void) const;
    QString GetHelpString(void) const;
    virtual QString GetHelpHeader(void) const { return ""; }

    virtual bool Parse(int argc, const char * const * argv);

// overloaded add constructors for single string options
    // bool with default
    CommandLineArg* add(QString arg, QString name, bool def,
                        QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::Bool,    
                       QVariant(def), help, longhelp); }
    // int
    CommandLineArg* add(QString arg, QString name, int def,
                        QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::Int,
                       QVariant(def), help, longhelp); }
    // uint
    CommandLineArg* add(QString arg, QString name, uint def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::UInt,
                       QVariant(def), help, longhelp); }
    // long long
    CommandLineArg* add(QString arg, QString name, long long def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::LongLong,
                       QVariant(def), help, longhelp); }
    // double
    CommandLineArg* add(QString arg, QString name, double def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::Double,
                       QVariant(def), help, longhelp); }
    // const char *
    CommandLineArg* add(QString arg, QString name, const char *def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::String,
                       QVariant(def), help, longhelp); }
    // QString
    CommandLineArg* add(QString arg, QString name, QString def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::String,
                       QVariant(def), help, longhelp); }
    // QSize
    CommandLineArg* add(QString arg, QString name, QSize def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::Size,
                       QVariant(def), help, longhelp); }
    // QDateTime
    CommandLineArg* add(QString arg, QString name, QDateTime def,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, QVariant::DateTime,
                       QVariant(def), help, longhelp); }
    // anything else
    CommandLineArg* add(QString arg, QString name, QVariant::Type type,
             QString help, QString longhelp)
          { return add(QStringList(arg), name, type,
                       QVariant(type), help, longhelp); }
    // anything else with default
    CommandLineArg* add(QString arg, QString name, QVariant::Type type,
             QVariant def, QString help, QString longhelp)
          { return add(QStringList(arg), name, type,
                       def, help, longhelp); }

// overloaded add constructors for multi-string options
    // bool with default
    CommandLineArg* add(QStringList arglist, QString name, bool def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::Bool,
                       QVariant(def), help, longhelp); }
    // int
    CommandLineArg* add(QStringList arglist, QString name, int def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::Int,
                       QVariant(def), help, longhelp); }
    // uint
    CommandLineArg* add(QStringList arglist, QString name, uint def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::UInt,
                       QVariant(def), help, longhelp); }
    // long long
    CommandLineArg* add(QStringList arglist, QString name, long long def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::LongLong,
                       QVariant(def), help, longhelp); }
    // float
    CommandLineArg* add(QStringList arglist, QString name, double def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::Double,
                       QVariant(def), help, longhelp); }
    // const char *
    CommandLineArg* add(QStringList arglist, QString name, const char *def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::String,
                       QVariant(def), help, longhelp); }
    // QString
    CommandLineArg* add(QStringList arglist, QString name, QString def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::String,
                       QVariant(def), help, longhelp); }
    // QSize
    CommandLineArg* add(QStringList arglist, QString name, QSize def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::Size,
                       QVariant(def), help, longhelp); }
    // QDateTime
    CommandLineArg* add(QStringList arglist, QString name, QDateTime def,
             QString help, QString longhelp)
          { return add(arglist, name, QVariant::DateTime,
                       QVariant(def), help, longhelp); }
    // anything else
    CommandLineArg* add(QStringList arglist, QString name, QVariant::Type type,
             QString help, QString longhelp)
          { return add(arglist, name, type,
                       QVariant(type), help, longhelp); }
    // anything else with default
    CommandLineArg* add(QStringList arglist, QString name, QVariant::Type type,
             QVariant def, QString help, QString longhelp);

    QVariant                operator[](const QString &name);
    QStringList             GetArgs(void) const;
    QMap<QString,QString>   GetExtra(void) const;
    QString                 GetPassthrough(void) const;
    QMap<QString,QString>   GetSettingsOverride(void);
    QString                 GetLogFilePath(void);
    int                     GetSyslogFacility(void);
    LogLevel_t              GetLogLevel(void);
    QString                 GetAppName(void) const { return m_appname; }

    bool                    toBool(QString key) const;
    int                     toInt(QString key) const;
    uint                    toUInt(QString key) const;
    long long               toLongLong(QString key) const;
    double                  toDouble(QString key) const;
    QSize                   toSize(QString key) const;
    QString                 toString(QString key) const;
    QStringList             toStringList(QString key, QString sep = "") const;
    QMap<QString,QString>   toMap(QString key) const;
    QDateTime               toDateTime(QString key) const;

    bool                    SetValue(const QString &key, QVariant value);
    int                     ConfigureLogging(QString mask = "general",
                                             unsigned int progress = 0);
    void                    ApplySettingsOverride(void);
    int                     Daemonize(void);

  protected:
    void allowArgs(bool allow=true);
    void allowExtras(bool allow=true);
    void allowPassthrough(bool allow=true);

    void addHelp(void);
    void addVersion(void);
    void addWindowed(void);
    void addMouse(void);
    void addDaemon(void);
    void addSettingsOverride(void);
    void addRecording(void);
    void addGeometry(void);
    void addDisplay(void);
    void addUPnP(void);
    void addLogging(const QString &defaultVerbosity = "general",
                    LogLevel_t defaultLogLevel = LOG_INFO);
    void addPIDFile(void);
    void addJob(void);
    void addInFile(bool addOutFile = false);

  private:
    int getOpt(int argc, const char * const * argv, int &argpos,
               QString &opt, QByteArray &val);
    bool ReconcileLinks(void);

    QString                         m_appname;
    QMap<QString,CommandLineArg*>   m_optionedArgs;
    QMap<QString,CommandLineArg*>   m_namedArgs;
    bool                            m_passthroughActive;
    bool                            m_overridesImported;
    bool                            m_verbose;
};

