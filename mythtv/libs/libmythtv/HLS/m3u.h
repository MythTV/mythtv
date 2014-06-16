#ifndef _mythtv_m3u_h_
#define _mythtv_m3u_h_

#include <inttypes.h>

#include <QDateTime>

namespace M3U
{
    QString DecodedURI(const QString& uri);
    QString RelativeURI(const QString& surl, const QString& spath);
    QString ParseAttributes(const QString& line, const char* attr);
    bool ParseDecimalValue(const QString& line, int& target);
    bool ParseDecimalValue(const QString& line, int64_t& target);
    bool ParseVersion(const QString& line, const QString& loc, int& version);
    bool ParseStreamInformation(const QString& line,
				const QString& url,
				const QString& loc,
				int& id, uint64_t& bandwidth);
    bool ParseTargetDuration(const QString& line, const QString& loc,
			     int& duration);
    bool ParseSegmentInformation(int version, const QString& line,
				 uint& duration, QString& title,
				 const QString& url);
    bool ParseMediaSequence(int64_t & sequence_num, const QString& line,
			    const QString& loc);
    bool ParseKey(int version, const QString& line, bool& aesmsg,
		  const QString& loc, QString &path, QString &iv);
    bool ParseProgramDateTime(const QString& line, const QString& loc,
			      QDateTime &date);
    bool ParseAllowCache(const QString& line, const QString& loc,
			 bool& do_cache);
    bool ParseDiscontinuity(const QString& line, const QString& loc);
    bool ParseEndList(const QString& loc, bool& is_vod);
}

#endif
