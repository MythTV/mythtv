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

typedef struct commandlinearg {
    QString         name;
    QVariant::Type  type;
    QVariant        def;
    QString         help;
    QString         longhelp;
} CommandLineArg;

class MBASE_PUBLIC MythCommandLineParser
{
  public:
    MythCommandLineParser(QString);

    virtual void LoadArguments(void) {};
    void PrintVersion(void);
    void PrintHelp(void);
    QString GetHelpString(bool with_header) const;
    virtual QString GetHelpHeader(void) const { return ""; }

    virtual bool Parse(int argc, const char * const * argv);

// overloaded add constructors for single string options
    void add(QString arg, QString name, bool def,                       // bool with default
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::Bool,    
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, int def,                        // int
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::Int,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, uint def,                       // uint
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::UInt,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, long long def,                  // long long
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::LongLong,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, double def,                     // double
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::Double,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, const char *def,                // const char *
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::String,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, QString def,                    // QString
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::String,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, QSize def,                      // QSize
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::Size,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, QDateTime def,                  // QDateTime
             QString help, QString longhelp)
                { add(QStringList(arg), name, QVariant::DateTime,
                      QVariant(def), help, longhelp); }
    void add(QString arg, QString name, QVariant::Type type,            // anything else
             QString help, QString longhelp)
                { add(QStringList(arg), name, type,
                      QVariant(type), help, longhelp); }
    void add(QString arg, QString name, QVariant::Type type,            // anything else with default
             QVariant def, QString help, QString longhelp)
                { add(QStringList(arg), name, type,
                      def, help, longhelp); }

// overloaded add constructors for multi-string options
    void add(QStringList arglist, QString name, bool def,               // bool with default
             QString help, QString longhelp)
                { add(arglist, name, QVariant::Bool,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, int def,                // int
             QString help, QString longhelp)
                { add(arglist, name, QVariant::Int,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, uint def,               // uint
             QString help, QString longhelp)
                { add(arglist, name, QVariant::UInt,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, long long def,          // long long
             QString help, QString longhelp)
                { add(arglist, name, QVariant::LongLong,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, double def,             // float
             QString help, QString longhelp)
                { add(arglist, name, QVariant::Double,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, const char *def,        // const char *
             QString help, QString longhelp)
                { add(arglist, name, QVariant::String,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, QString def,            // QString
             QString help, QString longhelp)
                { add(arglist, name, QVariant::String,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, QSize def,              // QSize
             QString help, QString longhelp)
                { add(arglist, name, QVariant::Size,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, QDateTime def,          // QDateTime
             QString help, QString longhelp)
                { add(arglist, name, QVariant::DateTime,
                      QVariant(def), help, longhelp); }
    void add(QStringList arglist, QString name, QVariant::Type type,    // anything else
             QString help, QString longhelp)
                { add(arglist, name, type,
                      QVariant(type), help, longhelp); }
    void add(QStringList arglist, QString name, QVariant::Type type,    // anything else with default
             QVariant def, QString help, QString longhelp);

    QVariant                operator[](const QString &name);
    QStringList             GetArgs(void) const { return m_remainingArgs; }
    QMap<QString,QString>   GetSettingsOverride(void);
    QString	                GetLogFilePath(void);
    int                     GetSyslogFacility(void);
    LogLevel_t              GetLogLevel(void);
    QString                 GetPassthrough(void) const { return m_passthrough.join(" "); }

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
    void allowArgs(bool allow=true) { m_allowArgs = allow; }
    void allowExtras(bool allow=true) { m_allowExtras = allow; }
    void allowPassthrough(bool allow=true) { m_allowPassthrough = allow; }

    void addHelp(void);
    void addVersion(void);
    void addWindowed(bool);
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

  private:
    int getOpt(int argc, const char * const * argv, int &argpos,
               QString &opt, QString &val);

    QString                         m_appname;
    QMap<QString,QVariant>          m_parsed;
    QMap<QString,QVariant>          m_defaults;
    QMap<QString,CommandLineArg>    m_registeredArgs;
    bool                            m_allowExtras;
    bool                            m_allowArgs;
    QStringList                     m_remainingArgs;
    bool                            m_allowPassthrough;
    bool                            m_passthroughActive;
    QStringList                     m_passthrough;
    bool                            m_overridesImported;
    bool                            m_verbose;
};

