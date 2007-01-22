/** -*- Mode: c++ -*-
 *  IPTVFeeder
 *  Copyright (c) 2006 by Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_H_
#define _IPTV_FEEDER_H_

class QString;
class TSDataListener;

/** \class IPTVFeeder
 *  \brief Base class for UDP and RTSP data sources for IPTVRecorder.
 *
 *   This is the interface that needs to be implemented to support new
 *   protocols in the IPTVRecorder.
 *
 *  To register a new implementation, it must be instanciated in
 *  IPTVFeederWrapper::NewFeeder().
 *
 *  \sa IPTVFeederWrapper
 */

class IPTVFeeder
{
  public:
    IPTVFeeder() {}
    virtual ~IPTVFeeder() {}

    /// \brief Returns true iff the data feed can handle URL
    virtual bool CanHandle(const QString &url) const = 0;
    /// \brief Returns true if the feeder is currently open
    virtual bool IsOpen(void) const = 0;

    /// \brief Inits the feeder and opens the stream identified by url
    virtual bool Open(const QString &url) = 0;
    /// \brief Closes the stream and frees resources allocated in Open()
    virtual void Close(void) = 0;
    /** \brief Reads the stream and sends data to its TSDataListener. This
     *         is a blocking call : it should not exit until Stop() is called.
     *  \sa Stop(void)
     */
    virtual void Run(void) = 0;
    /** \brief Tells Run(void) function that it should stop and exit cleanly.
     *
     *   This function blocks until Run(void) has finished up.
     *
     *  \sa Run(void)
     */
    virtual void Stop(void) = 0;

    virtual void AddListener(TSDataListener*) = 0;
    virtual void RemoveListener(TSDataListener*) = 0;
};

#endif // _IPTV_FEEDER_H_
