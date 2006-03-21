#ifndef INTERACTIVE_TV_H_
#define INTERACTIVE_TV_H_

class OSDSet;
class MHIContext;
class NuppelVideoPlayer;

/** \class InteractiveTV
 *  \brief This is the interface between an MHEG engine and a MythTV TV object.
 */
class InteractiveTV
{
  public:
    // Interface to Myth
    InteractiveTV(NuppelVideoPlayer *nvp);
    virtual ~InteractiveTV();

    void Restart(QString chanid, bool isLive);
    // Process an incoming DSMCC packet.
    void ProcessDSMCCSection(unsigned char *data, int length,
                             int componentTag, unsigned carouselId,
                             int dataBroadcastId);

    // See if the image has changed.
    bool ImageHasChanged(void);
    // Draw the (updated) image.
    void UpdateOSD(OSDSet *osdSet);
    // Called when the visible display area has changed.
    void Reinit(const QRect &display);

    // Offer a key press.  Returns true if it accepts it.
    // This will depend on the current profile.
    bool OfferKey(QString key);

    // Get the initial component tags.
    void GetInitialStreams(int &audioTag, int &videoTag);

    NuppelVideoPlayer *GetNVP(void) { return m_nvp; }

  protected:
    MHIContext        *m_context;
    NuppelVideoPlayer *m_nvp;
};

#endif
