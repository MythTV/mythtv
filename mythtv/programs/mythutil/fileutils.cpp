// libmyth* headers
#include "exitcodes.h"
#include "mythlogging.h"
#include "ringbuffer.h"

// local headers
#include "mythutil.h"

static int CopyFile(const MythUtilCommandLineParser &cmdline)
{
    int result = GENERIC_EXIT_OK;

    if (cmdline.toString("srcfile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --srcfile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("srcfile");

    if (cmdline.toString("destfile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --destfile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString dest = cmdline.toString("destfile");

    LOG(VB_GENERAL, LOG_INFO, QString("Copying %1 to %2").arg(src).arg(dest));
    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, couldn't create Read RingBuffer");
        return GENERIC_EXIT_NOT_OK;
    }

    if (!srcRB->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, srcRB is not open");
        return GENERIC_EXIT_NOT_OK;
    }

    RingBuffer *destRB = RingBuffer::Create(dest, true);
    if (!destRB)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, couldn't create Write RingBuffer");
        delete srcRB;
        return GENERIC_EXIT_NOT_OK;
    }

    if (!destRB->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, destRB is not open");
        return GENERIC_EXIT_NOT_OK;
    }

    const int readSize = 2 * 1024 * 1024;
    char buf[readSize];
    bool ok = true;
    int r;
    int ret;
    int written = 0;
    while (ok && ((r = srcRB->Read(buf, readSize)) > 0))
    {
        ret = destRB->Write(buf, r);
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("ERROR, couldn't write at offset %1").arg(written));
            ok = false;
        }
        else
            written += ret;
    }

    LOG(VB_GENERAL, LOG_INFO,
            QString("Wrote %1 bytes total").arg(written));

    delete srcRB;
    delete destRB;

    if (!ok)
        result = GENERIC_EXIT_NOT_OK;

    return result;
}

void registerFileUtils(UtilMap &utilMap)
{
    utilMap["copyfile"]             = &CopyFile;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
