/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef LIBDSMCC_H
#define LIBDSMCC_H

#include <QLinkedList>
#include <QStringList>

#include "dsmccreceiver.h"
#include "dsmccobjcarousel.h"

/*
   Overview.
   This is brief, perhaps simplistic, overview of the DSMCC object carousel.

   The purpose of the carousel is to make available to the MHEG or MHP application
   a small, Unix-like, filing system.  The notion of a carousel comes from the idea
   that the files and directories of the filing system are constantly retransmitted
   so that whenever a receiver tunes in it will eventually see all the components.

   The input to this code is in the form of MPEG tables which have been constructed
   out of MPEG TS packets.  There are three kinds of tables that are processed: DSI,
   DII and DDB.
   
   DSI tables contain the address (IOR) of the root directory of the filing system.
   Why this is needed and how it works is described below.
   
   DII tables describe "modules".  A module is collection of one or more files
   or directories. Because a module may be larger than the maximum allowed table
   size (4k) the entries in the DII say how many blocks, contained in DDB tables,
   are needed to make the module and also whether, when all the blocks have been
   found, the module needs to be decompressed.
   
   Each DDB contains data forming part of a module as well as information saying
   which module it belongs to and which block within the module it is.  Once all
   the blocks of a module have been found and, if necessary, the module
   decompressed, the data is ready for the next stage.

   A module comprises one of more directory, service gateway or file objects.
   A service gateway is exactly the same as a directory except that it is a
   root directory.  Directories and service gateways are tables with a text
   name, whether the entry is a file or sub-directory, and the address (IOR)
   of the file or sub-directory.  File objects contain the data for the file
   itself.  Note that this arrangement allows for the directory structure to
   be an arbitrary directed graph.  

   There may be multiple service gateways in the carousel and this is the
   reason for the DSI message.  The DSI message identifies one service
   gateway as the root to be used.  Working from this it is possible to
   construct a tree of sub-directories and files to make the filing system.

   The reason for having multiple service gateways and DSI messages rather
   than just transmitting a single service gateway is that it allows for
   the same carousel to be used for several services on a DVB multiplex
   and so reduce the overall bandwidth requirements on the multiplex.  For
   example, several BBC radio programmes are transmitted on the same multiplex.
   The PMT for each programme identifies an initial PID stream containing
   DSMCC packets and a secondary stream.  The initial stream is different
   for each programme and sends only DSI messages.  Everything else is
   transmitted on the common stream with a service gateway and a few extra
   files, such as the programme logo, specific to that programme, but with
   everything else shared.

   The MHEG or MHP library makes requests for files by passing in a
   Unix-like path name.  The organisation of the carousel allows a
   receiver to pick out the file by building the data structures needed
   to satisfy only the request.  That minimises the memory requirements
   but provides a slow response since every file request requires the
   application to wait until the file appears on the carousel.  Instead
   this code builds the filing system as the information appears.
*/

class Dsmcc
{
  public:
    Dsmcc();
    ~Dsmcc();
    // Reset the object carousel and clear the caches.
    void Reset();
    // Process an incoming DSMCC carousel table
    void ProcessSection(const unsigned char *data, int length,
                        int componentTag, unsigned carouselId,
                        int dataBroadcastId);
    // Request for a carousel object.
    int GetDSMCCObject(QStringList &objectPath, QByteArray &result);

    // Add a tap.  This indicates the component tag of the stream that is to
    // be used to receive subsequent messages for this carousel. 
    // Creates a new carousel object if there isn't one for this ID.
    ObjCarousel *AddTap(unsigned short componentTag, unsigned carouselId);

  protected:
    void ProcessSectionIndication(const unsigned char *data, int Lstartength,
                                  unsigned short streamTag);
    void ProcessSectionData(const unsigned char *data, int length);
    void ProcessSectionDesc(const unsigned char *data, int length);

    bool ProcessSectionHeader(DsmccSectionHeader *pSection,
                              const unsigned char *data, int length);

    void ProcessDownloadServerInitiate(const unsigned char *data, int length);
    void ProcessDownloadInfoIndication(const unsigned char *data,
                                       unsigned short streamTag);

    // Return a carousel with the given ID.
    ObjCarousel *GetCarouselById(unsigned int carId);

    // Known carousels.
    QLinkedList<ObjCarousel*> carousels;

    // Initial stream
    unsigned short m_startTag;
};

#define COMBINE32(data, idx) \
    ((((unsigned)((data)[(idx) + 0])) << 24) |  \
     (((unsigned)((data)[(idx) + 1])) << 16) |  \
     (((unsigned)((data)[(idx) + 2])) << 8) |   \
     (((unsigned)((data)[(idx) + 3]))))

#endif
