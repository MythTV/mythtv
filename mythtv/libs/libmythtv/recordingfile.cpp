
#include "recordingfile.h"

#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"

bool RecordingFile::Load()
{
    if (m_recordingId == 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                  "hostname, storagegroup, id, basename, filesize, "
                  "video_codec, width, height, aspect, fps, "
                  "audio_codec, audio_channels, audio_sample_rate, "
                  "audio_avg_bitrate, container "
                  "FROM recordedfile "
                  "WHERE recordedid = :RECORDEDID ");
    query.bindValue(":RECORDEDID", m_recordingId);

    if (!query.exec())
    {
        MythDB::DBError("RecordingFile::Load()", query);
        return false;
    }

    if (query.next())
    {
        m_storageDeviceID = query.value(0).toString();
        m_storageGroup = query.value(1).toString();

        m_fileId = query.value(2).toUInt();
        m_fileName = query.value(3).toString();
        m_fileSize = static_cast<uint64_t>(query.value(4).toLongLong());

        m_videoCodec = query.value(5).toString();
        uint width = query.value(6).toUInt();
        uint height = query.value(7).toUInt();
        m_videoResolution = QSize(width, height);
        m_videoAspectRatio = query.value(8).toDouble();
        m_videoFrameRate = query.value(9).toDouble();

        m_audioCodec = query.value(10).toString();
        m_audioChannels = query.value(11).toUInt();
        m_audioSampleRate = query.value(12).toUInt();
        m_audioBitrate = query.value(13).toUInt();

        m_containerFormat = AVContainerFromString(query.value(14).toString());
    }

    return true;
}

bool RecordingFile::Save()
{
    if (m_recordingId == 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    if (m_fileId > 0)
    {
        query.prepare("UPDATE recordedfile "
                      "SET "
                      "basename = :FILENAME, "
                      "filesize = :FILESIZE, "
                      "width = :WIDTH, "
                      "height = :HEIGHT, "
                      "fps = :FRAMERATE, "
                      "aspect = :ASPECT_RATIO, "
                      "audio_sample_rate = :AUDIO_SAMPLERATE, "
                      "audio_avg_bitrate = :AUDIO_BITRATE, "
                      "audio_channels = :AUDIO_CHANNELS, "
                      "audio_codec = :AUDIO_CODEC, "
                      "video_codec = :VIDEO_CODEC, "
                      "hostname = :STORAGE_DEVICE, "
                      "storagegroup = :STORAGE_GROUP, "
                      "recordedid = :RECORDING_ID, "
                      "container = :CONTAINER "
                      "WHERE id = :FILE_ID ");
        query.bindValue(":FILE_ID", m_fileId);
    }
    else
    {
        query.prepare("INSERT INTO recordedfile "
                      "SET "
                      "basename = :FILENAME, "
                      "filesize = :FILESIZE, "
                      "width = :WIDTH, "
                      "height = :HEIGHT, "
                      "fps = :FRAMERATE, "
                      "aspect = :ASPECT_RATIO, "
                      "audio_sample_rate = :AUDIO_SAMPLERATE, "
                      "audio_avg_bitrate = :AUDIO_BITRATE, "
                      "audio_channels = :AUDIO_CHANNELS, "
                      "audio_codec = :AUDIO_CODEC, "
                      "video_codec = :VIDEO_CODEC, "
                      "hostname = :STORAGE_DEVICE, "
                      "storagegroup = :STORAGE_GROUP, "
                      "recordedid = :RECORDING_ID, "
                      "container = :CONTAINER ");
    }

    query.bindValueNoNull(":FILENAME", m_fileName);
    query.bindValue(":FILESIZE", static_cast<quint64>(m_fileSize));
    query.bindValue(":WIDTH", m_videoResolution.width());
    query.bindValue(":HEIGHT", m_videoResolution.height());
    query.bindValue(":FRAMERATE", m_videoFrameRate);
    query.bindValue(":ASPECT_RATIO", m_videoAspectRatio);
    query.bindValue(":AUDIO_SAMPLERATE", m_audioSampleRate);
    query.bindValue(":AUDIO_BITRATE", m_audioBitrate);
    query.bindValue(":AUDIO_CHANNELS", m_audioChannels);
    query.bindValueNoNull(":AUDIO_CODEC", m_audioCodec);
    query.bindValueNoNull(":VIDEO_CODEC", m_videoCodec);
    query.bindValueNoNull(":STORAGE_DEVICE", m_storageDeviceID);
    query.bindValueNoNull(":STORAGE_GROUP", m_storageGroup);
    query.bindValue(":CONTAINER", AVContainerToString(m_containerFormat));
    query.bindValue(":RECORDING_ID", m_recordingId);

    if (!query.exec())
    {
        MythDB::DBError("RecordingFile::Save()", query);
        return false;
    }

    if (m_fileId == 0)
        m_fileId = query.lastInsertId().toUInt();

    return true;
}

AVContainer RecordingFile::AVContainerFromString(const QString &formatStr)
{
    if (formatStr == "NUV")
        return formatNUV;
    if (formatStr == "MPEG2-TS")
        return formatMPEG2_TS;
    if (formatStr == "MPEG2-PS")
        return formatMPEG2_PS;
    return formatUnknown;
}

QString RecordingFile::AVContainerToString(AVContainer format)
{
    switch (format)
    {
        case formatNUV :
            return "NUV";
        case formatMPEG2_TS :
            return "MPEG2-TS";
        case formatMPEG2_PS :
            return "MPEG2-PS";
        case formatUnknown:
        default:
            return "";
    }
}


