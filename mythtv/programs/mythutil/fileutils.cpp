// libmyth* headers
#include "exitcodes.h"
#include "mythlogging.h"
#include "ringbuffer.h"
#include "mythdownloadmanager.h"

// local headers
#include "fileutils.h"

static int CopyFile(const MythUtilCommandLineParser &cmdline)
{
    int result = GENERIC_EXIT_OK;

    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --infile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    if (cmdline.toString("outfile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --outfile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString dest = cmdline.toString("outfile");

    const int readSize = 2 * 1024 * 1024;
    char *buf = new char[readSize];
    if (!buf)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, unable to allocate copy buffer ");
        return GENERIC_EXIT_NOT_OK;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Copying %1 to %2").arg(src).arg(dest));
    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, couldn't create Read RingBuffer");
        delete[] buf;
        return GENERIC_EXIT_NOT_OK;
    }

    if (!srcRB->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, srcRB is not open");
        delete[] buf;
        delete srcRB;
        return GENERIC_EXIT_NOT_OK;
    }

    RingBuffer *destRB = RingBuffer::Create(dest, true);
    if (!destRB)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, couldn't create Write RingBuffer");
        delete[] buf;
        delete srcRB;
        return GENERIC_EXIT_NOT_OK;
    }

    if (!destRB->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, destRB is not open");
        delete[] buf;
        delete srcRB;
        delete destRB;
        return GENERIC_EXIT_NOT_OK;
    }
    destRB->WriterSetBlocking(true);

    long long totalBytes = srcRB->GetRealFileSize();
    long long totalBytesCopied = 0;
    int percentComplete = 0;
    bool ok = true;
    int r;
    int ret;
    while (ok && ((r = srcRB->Read(buf, readSize)) > 0))
    {
        ret = destRB->Write(buf, r);
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("ERROR, couldn't write at offset %1")
                            .arg(totalBytesCopied));
            ok = false;
        }
        else
            totalBytesCopied += ret;

        percentComplete = totalBytesCopied * 100 / totalBytes;
        if ((percentComplete % 5) == 0)
        LOG(VB_GENERAL, LOG_INFO,
            QString("%1 bytes copied, %2%% complete")
                    .arg(totalBytesCopied).arg(percentComplete));
    }

    LOG(VB_GENERAL, LOG_INFO,
        QString("Wrote %1 bytes total").arg(totalBytesCopied));

    LOG(VB_GENERAL, LOG_INFO, "Waiting for write buffer to flush");

    delete[] buf;
    delete srcRB;
    delete destRB;

    if (!ok)
        result = GENERIC_EXIT_NOT_OK;

    return result;
}

static int DownloadFile(const MythUtilCommandLineParser &cmdline)
{
    int result = GENERIC_EXIT_OK;

    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --infile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString url = cmdline.toString("infile");

    if (cmdline.toString("outfile").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --outfile option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString dest = cmdline.toString("outfile");

    bool ok = GetMythDownloadManager()->download(url, dest);

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_INFO, "Error downloading file.");
        result = GENERIC_EXIT_NOT_OK;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "File downloaded.");
    }

    return result;
}

void registerFileUtils(UtilMap &utilMap)
{
    utilMap["copyfile"]             = &CopyFile;
    utilMap["download"]             = &DownloadFile;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
