#ifndef PLAYBACKSTATE_H_
#define PLAYBACKSTATE_H_

// POSIX headers
#include <cstdint> // for [u]int[32,64]_t

#include <QMap>
#include <QString>

/// Utility class to query playback state from database
class PlaybackState
{
public:
    PlaybackState();

    /// Initializes playback state from database
    void Initialize();

    /// Updates playback state of video with specified filename
    void Update(const QString &filename);

    /// Query bookmark of video with the specified filename
    bool HasBookmark(const QString &filename) const;

    /// Query last playback position of video with the specified filename
    uint64_t GetLastPlayPos(const QString &filename) const;

    /// Query watched percent of video with the specified filename
    uint GetWatchedPercent(const QString &filename) const;

    /// Returns cached setting "AlwaysShowWatchedProgress"
    bool AlwaysShowWatchedProgress() const;

private:
    /// Query playback state from database, only for single video if a filename is specified
    void QueryData(const QString &filterFilename = QString());

    /// Markup for a video file
    struct Markup {
        uint64_t totalFrames = 0;   ///< total frames
        uint64_t lastPlayPos = 0;   ///< last playing position
        uint64_t bookmarkPos = 0;   ///< bookmark position
    };

    QMap<QString, Markup> m_fileMarkup; ///< maps filename to markup
    bool m_alwaysShowWatchedProgress {false};
};

#endif // PLAYBACKSTATE_H_
