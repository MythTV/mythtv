#ifndef INTERACTIVE_TV_H_
#define INTERACTIVE_TV_H_

class InteractiveScreen;
class MythPainter;
class MHIContext;
class MythPlayer;

/** \class InteractiveTV
 *  \brief This is the interface between an MHEG engine and a MythTV TV object.
 */
class InteractiveTV
{
#ifdef USING_MHEG
  public:
    // Interface to Myth
    InteractiveTV(MythPlayer *nvp);
    InteractiveTV(const InteractiveTV& rhs);
    virtual ~InteractiveTV();

    void Restart(int chanid, int sourceid, bool isLive);
    // Process an incoming DSMCC packet.
    void ProcessDSMCCSection(unsigned char *data, int length,
                             int componentTag, unsigned carouselId,
                             int dataBroadcastId);

    // A NetworkBootInfo sub-descriptor is present in the PMT
    void SetNetBootInfo(const unsigned char *data, uint length);

    // See if the image has changed.
    bool ImageHasChanged(void);
    // Draw the (updated) image.
    void UpdateOSD(InteractiveScreen *osdWindow, MythPainter *osdPainter);
    // Called when the visible display area has changed.
    void Reinit(const QRect &display);

    // Offer a key press.  Returns true if it accepts it.
    // This will depend on the current profile.
    bool OfferKey(QString key);

    // Get the initial component tags.
    void GetInitialStreams(int &audioTag, int &videoTag);
    // Called when a stream starts or stops. Returns true if event is handled
    bool StreamStarted(bool bStarted = true);

    MythPlayer *GetNVP(void) { return m_nvp; }

  protected:
    MHIContext        *m_context;
    MythPlayer *m_nvp;
#endif
};

#endif
