// -*- Mode: c++ -*-

#include <QStringList>
#include <QDateTime>
#include <QSize>
#include <QMap>

#include <stdint.h>   // for uint64_t

#include "mythexp.h"

typedef enum {
    kCLPOverrideSettingsFile = 0x0000000001ULL,
    kCLPOverrideSettings     = 0x0000000002ULL,
    kCLPWindowed             = 0x0000000004ULL,
    kCLPNoWindowed           = 0x0000000008ULL,
    kCLPGetSettings          = 0x0000000010ULL,
    kCLPQueryVersion         = 0x0000000020ULL,
    kCLPDisplay              = 0x0000000040ULL,
    kCLPGeometry             = 0x0000000080ULL,
    kCLPVerbose              = 0x0000000100ULL,
    kCLPSetVerbose           = 0x0000000200ULL,
    kCLPHelp                 = 0x0000000400ULL,
    kCLPExtra                = 0x0000000800ULL,
    kCLPDaemon               = 0x0000001000ULL,
    kCLPPrintSchedule        = 0x0000002000ULL,
    kCLPTestSchedule         = 0x0000004000ULL,
    kCLPReschedule           = 0x0000008000ULL,
    kCLPNoSchedule           = 0x0000010000ULL,
    kCLPNoUPnP               = 0x0000020000ULL,
    kCLPUPnPRebuild          = 0x0000040000ULL,
    kCLPNoJobqueue           = 0x0000080000ULL,
    kCLPNoHousekeeper        = 0x0000100000ULL,
    kCLPNoAutoExpire         = 0x0000200000ULL,
    kCLPClearCache           = 0x0000400000ULL,
    kCLPLogFile              = 0x0000800000ULL,
    kCLPPidFile              = 0x0001000000ULL,
    kCLPInFile               = 0x0002000000ULL,
    kCLPOutFile              = 0x0004000000ULL,
    kCLPUsername             = 0x0008000000ULL,
    kCLPEvent                = 0x0010000000ULL,
    kCLPSystemEvent          = 0x0020000000ULL,
    kCLPChannelId            = 0x0040000000ULL,
    kCLPStartTime            = 0x0080000000ULL,
    kCLPPrintExpire          = 0x0100000000ULL,
    kCLPGeneratePreview      = 0x0200000000ULL,
    kCLPScanVideos           = 0x0400000000ULL,
} ParseType;

class MPUBLIC MythCommandLineParser
{
  public:
    MythCommandLineParser(uint64_t things_to_parse);

    bool PreParse(int argc, const char * const * argv, int &argpos, bool &err);
    bool Parse(int argc, const char * const * argv, int &argpos, bool &err);
    QString GetHelpString(bool with_header) const;

    QMap<QString,QString> GetSettingsOverride(void) const
        { return settingsOverride; }
    QStringList GetSettingsQuery(void) const
        { return settingsQuery; }
    QString GetDisplay(void)        const { return display;     }
    QString GetGeometry(void)       const { return geometry;    }
    QString GetLogFilename(void)    const { return logfile;     }
    QString GetPIDFilename(void)    const { return pidfile;     }
    QString GetInputFilename(void)  const { return infile;      }
    QString GetOutputFilename(void) const { return outfile;     }
    QString GetNewVerbose(void)     const { return newverbose;  }
    QString GetUsername(void)       const { return username;    }
    QString GetPrintExpire(void)    const { return printexpire; }
    QString GetEventString(void)    const { return eventString; }

    QSize     GetPreviewSize(void)        const { return previewSize; }
    QDateTime GetStartTime(void)          const { return starttime;   }
    uint      GetChanID(void)             const { return chanid;      }
    long long GetPreviewFrameNumber(void) const { return  previewFrameNumber; }
    long long GetPreviewSeconds(void)     const { return previewSeconds; }

    bool IsDaemonizeEnabled(void)   const { return daemonize;   }
    bool IsPrintScheduleEnabled(void) const { return printsched;  }
    bool IsTestSchedulerEnabled(void) const { return testsched;   }
    bool IsSchedulerEnabled(void)   const { return !nosched;    }
    bool IsUPnPEnabled(void)        const { return !noupnp;     }
    bool IsJobQueueEnabled(void)    const { return !nojobqueue; }
    bool IsHouseKeeperEnabled(void) const { return !nohousekeeper; }
    bool IsAutoExpirerEnabled(void) const { return !noexpirer;  }

    bool SetVerbose(void)           const { return setverbose;  }
    bool Reschedule(void)           const { return resched;     }
    bool ScanVideos(void)           const { return scanvideos;  }
    bool ClearSettingsCache(void)   const { return clearsettingscache; }
    bool WantUPnPRebuild(void)      const { return wantupnprebuild; }

    bool    HasInvalidPreviewGenerationParams(void) const
    {
        return ((!chanid || !starttime.isValid()) && infile.isEmpty());
    }

    bool    HasBackendCommand(void) const
    {
        return
            !eventString.isEmpty()    || wantupnprebuild       ||
            setverbose                || clearsettingscache    ||
            printsched                || testsched             ||
            resched                   || scanvideos            ||
            !printexpire.isEmpty();
    }

    bool    WantsToExit(void) const { return wantsToExit; }

  private:
    uint64_t              parseTypes;

    QMap<QString,QString> settingsOverride;
    QStringList           settingsQuery;

    QString               binname;
    QString               display;
    QString               geometry;
    QString               logfile;
    QString               pidfile;
    QString               infile;
    QString               outfile;
    QString               newverbose;
    QString               username;
    QString               printexpire;
    QString               eventString;

    QSize                 previewSize;
    QDateTime             starttime;

    uint                  chanid;
    long long             previewFrameNumber;
    long long             previewSeconds;

    bool                  daemonize;
    bool                  printsched;
    bool                  testsched;
    bool                  setverbose;
    bool                  resched;
    bool                  nosched;
    bool                  scanvideos;
    bool                  noupnp;
    bool                  nojobqueue;
    bool                  nohousekeeper;
    bool                  noexpirer;
    bool                  clearsettingscache;
    bool                  wantupnprebuild;
    bool                  wantsToExit;
};
