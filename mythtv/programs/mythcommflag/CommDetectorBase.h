#ifndef COMMDETECTOR_BASE_H
#define COMMDETECTOR_BASE_H

#include <iostream>

#include <QObject>
#include <QMap>

#include "libmythbase/programtypes.h"

static constexpr int64_t MAX_BLANK_FRAMES { 180 };

enum CommMapValue : std::uint8_t {
    MARK_START   = 0,
    MARK_END     = 1,
    MARK_PRESENT = 2,
};

using show_map_t = QMap<uint64_t, CommMapValue>;

/** \class CommDetectorBase
 *  \brief Abstract base class for all CommDetectors.
 *   Please use the CommDetectFactory to make actual instances.
 */

class CommDetectorBase : public QObject
{
    Q_OBJECT

public:
    CommDetectorBase() = default;

    virtual bool go() = 0;
    void stop();
    void pause();
    void resume();

    virtual void GetCommercialBreakList(frm_dir_map_t &comms) = 0;
    virtual void recordingFinished([[maybe_unused]] long long totalFileSize) {};
    virtual void requestCommBreakMapUpdate(void) {};

    virtual void PrintFullMap(
        std::ostream &out, const frm_dir_map_t *comm_breaks, bool verbose) const = 0;

signals:
    void statusUpdate(const QString& a) ;
    void gotNewCommercialBreakList();
    void breathe();

protected:    
    ~CommDetectorBase() override = default;
    bool m_bPaused { false };
    bool m_bStop   { false };
};

#endif // COMMDETECTOR_BASE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
