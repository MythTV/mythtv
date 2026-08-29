#include "mythdvdlogger.h"

#include "libmythbase/mythlogging.h"

extern "C" {
static void MythDVDread_log(void *, dvd_logger_level_t level, const char * fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE())
    {
        return;
    }

    uint64_t   verbose_mask  = VB_DVD;
    LogLevel_t verbose_level = LOG_EMERG;
    switch (level)
    {
        case DVD_LOGGER_LEVEL_INFO:
            verbose_level = LOG_INFO;
            break;
        case DVD_LOGGER_LEVEL_ERROR:
            verbose_level = LOG_ERR;
            break;
        case DVD_LOGGER_LEVEL_WARN:
            verbose_level = LOG_WARNING;
            break;
        case DVD_LOGGER_LEVEL_DEBUG:
            verbose_level = LOG_DEBUG;
            break;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
    {
        return;
    }

    LOG(verbose_mask, verbose_level, QString("libdvdread: ") + QString::vasprintf(fmt, vl));
}

static void MythDVDnav_log(void *, dvdnav_logger_level_t level, const char * fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE())
    {
        return;
    }

    uint64_t   verbose_mask  = VB_DVD;
    LogLevel_t verbose_level = LOG_EMERG;
    switch (level)
    {
        case DVDNAV_LOGGER_LEVEL_INFO:
            verbose_level = LOG_INFO;
            break;
        case DVDNAV_LOGGER_LEVEL_ERROR:
            verbose_level = LOG_ERR;
            break;
        case DVDNAV_LOGGER_LEVEL_WARN:
            verbose_level = LOG_WARNING;
            break;
        case DVDNAV_LOGGER_LEVEL_DEBUG:
            verbose_level = LOG_DEBUG;
            break;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
    {
        return;
    }

    LOG(verbose_mask, verbose_level, QString("libdvdnav: ") + QString::vasprintf(fmt, vl));
}

} // extern "C"

dvd_logger_cb s_dvdread_logger = {.pf_log = &MythDVDread_log};
dvdnav_logger_cb s_dvdnav_logger = {.pf_log = &MythDVDnav_log};
