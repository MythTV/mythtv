#ifndef VIDEOPLAYERCOMMAND_H_
#define VIDEOPLAYERCOMMAND_H_

class VideoMetadata;
class VideoPlayerCommand
{
  public:
    static VideoPlayerCommand PlayerFor(const VideoMetadata *item);
    static VideoPlayerCommand PlayerFor(const QString &filename);
    static VideoPlayerCommand AltPlayerFor(const VideoMetadata *item);

  public:
    VideoPlayerCommand();
    ~VideoPlayerCommand();

    VideoPlayerCommand(const VideoPlayerCommand &other);
    VideoPlayerCommand &operator=(const VideoPlayerCommand &rhs);

    void Play() const;

    /// Returns the player command suitable for display to the user.
    QString GetCommandDisplayName() const;

  private:
    class VideoPlayerCommandPrivate *m_d;
};

#endif // PLAYERCOMMAND_H_
