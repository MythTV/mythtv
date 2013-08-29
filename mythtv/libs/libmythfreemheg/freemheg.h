/* freemheg.h

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#if !defined(FREEMHEG_H)
#define FREEMHEG_H

#include <QtGlobal>
#include <QString>
#include <QByteArray>
#include <QRegion>
#include <QRect>
#include <QSize>

#include <stdio.h>
#include <stdlib.h>

class MHDLADisplay;
class MHTextDisplay;
class MHBitmapDisplay;
class MHContext;
class MHEG;
class MHStream;

// Called to create a new instance of the module.
extern MHEG *MHCreateEngine(MHContext *context);
// Set the logging stream and options.
extern void MHSetLogging(FILE *logStream, unsigned int logLevel);

// This abstract class is implemented by the MHEG Engine.
class MHEG
{
  public:
    virtual ~MHEG() {}
    virtual void SetBooting() = 0;
    virtual void DrawDisplay(QRegion toDraw) = 0;
    // Run synchronous actions and process any asynchronous events until the queues are empty.
    // Returns the number of milliseconds until wake-up or 0 if none.
    virtual int RunAll(void) = 0;
    // Generate a UserAction event i.e. a key press.
    virtual void GenerateUserAction(int nCode) = 0;
    virtual void EngineEvent(int) = 0;
    virtual void StreamStarted(MHStream*, bool bStarted = true) = 0;
};

// Logging control
enum {
    MHLogError = 1,         // Log errors - these are errors that need to be reported to the user.
    MHLogWarning = 2,       // Log warnings - typically bad MHEG which might be an error in this program
    MHLogNotifications = 4, // General things to log.
    MHLogScenes = 8,        // Print each application and scene
    MHLogActions = 16,      // Print each action before it is run.
    MHLogLinks = 32,        // Print each link when it is fired and each event as it is queued
    MHLogDetail = 64        // Detailed evaluation of each action.
};

#define MHLogAll (MHLogError|MHLogWarning|MHLogNotifications|MHLogScenes|MHLogActions|MHLogLinks|MHLogDetail)

class MHRgba
{
  public:
    MHRgba(int red, int green, int blue, int alpha):
      m_red(red), m_green(green), m_blue(blue), m_alpha(alpha) {};
    MHRgba(): m_red(0), m_green(0), m_blue(0), m_alpha(0) {};
    int red() const { return m_red; }
    int green() const { return m_green; }
    int blue() const { return m_blue; }
    int alpha() const { return m_alpha; }
  private:
    unsigned char m_red, m_green, m_blue, m_alpha;
};

// This abstract class provides operations that the surrounding context must provide
// for the MHEG engine.
class MHContext
{
  public:
    virtual ~MHContext() {} // Declared to avoid warnings
    // Interface to MHEG engine.

    // Test for an object in the carousel.  Returns true if the object is present and
    // so a call to GetCarouselData will not block and will return the data.
    // Returns false if the object is not currently present because it has not
    // yet appeared and also if it is not present in the containing directory.
    virtual bool CheckCarouselObject(QString objectPath) = 0;

    // Get an object from the carousel.  Returns true and sets the data if
    // it was able to retrieve the named object.  Blocks if the object seems
    // to be present but has not yet appeared.  Returns false if the object
    // cannot be retrieved.
    virtual bool GetCarouselData(QString objectPath, QByteArray &result) = 0;

    // Set the input register.  This sets the keys that are to be handled by MHEG.  Flushes the key queue.
    virtual void SetInputRegister(int nReg) = 0;

    // An area of the screen/image needs to be redrawn.
    virtual void RequireRedraw(const QRegion &region) = 0;

    // Creation functions for various visibles.
    virtual MHDLADisplay *CreateDynamicLineArt(bool isBoxed, MHRgba lineColour, MHRgba fillColour) = 0;
    virtual MHTextDisplay *CreateText(void) = 0;
    virtual MHBitmapDisplay *CreateBitmap(bool tiled) = 0;
    // Additional drawing functions.
    // Draw a rectangle in the specified colour/transparency.
    virtual void DrawRect(int xPos, int yPos, int width, int height, MHRgba colour) = 0;
    virtual void DrawVideo(const QRect &videoRect, const QRect &displayRect) = 0;
    virtual void DrawBackground(const QRegion &reg) = 0;

    // Tuning.  Get the index corresponding to a given channel.
    virtual int GetChannelIndex(const QString &str) = 0;
    // Get netId etc from the channel index.
    virtual bool GetServiceInfo(int channelId, int &netId, int &origNetId,
                                int &transportId, int &serviceId) = 0;
    // Tune to an index returned by GetChannelIndex
    virtual bool TuneTo(int channel, int tuneinfo) = 0;

    // Check whether we have requested a stop.  Returns true and signals
    // the m_stopped condition if we have.
    virtual bool CheckStop(void) = 0;

    // Begin playing the specified stream
    virtual bool BeginStream(const QString &str, MHStream* notify = 0) = 0;
    // Stop playing stream
    virtual void EndStream() = 0;
    // Begin playing audio component
    virtual bool BeginAudio(int tag) = 0;
    // Stop playing audio
    virtual void StopAudio() = 0;
    // Begin displaying video component
    virtual bool BeginVideo(int tag) = 0;
    // Stop displaying video
    virtual void StopVideo() = 0;
    // Get current stream position in mS, -1 if unknown
    virtual long GetStreamPos() = 0;
    // Get current stream size in mS, -1 if unknown
    virtual long GetStreamMaxPos() = 0;
    // Set current stream position in mS
    virtual long SetStreamPos(long) = 0;
    // Play or pause a stream
    virtual void StreamPlay(bool play = true) = 0;

    // Get the context id strings.
    virtual const char *GetReceiverId(void) = 0;
    virtual const char *GetDSMCCId(void) = 0;

    // InteractionChannel
    virtual int GetICStatus() = 0; // 0= Active, 1= Inactive, 2= Disabled
};

// Dynamic Line Art objects record a sequence of drawing actions.
class MHDLADisplay
{
  public:
    virtual ~MHDLADisplay() {}
    // Draw the completed drawing onto the display.
    virtual void Draw(int x, int y) = 0;
    // Set the box size.  Also clears the drawing.
    virtual void SetSize(int width, int height) = 0;
    virtual void SetLineSize(int width) = 0;
    virtual void SetLineColour(MHRgba colour) = 0;
    virtual void SetFillColour(MHRgba colour) = 0;
    // Clear the drawing
    virtual void Clear() = 0;
    // Operations to add items to the drawing.
    virtual void DrawLine(int x1, int y1, int x2, int y2) = 0;
    virtual void DrawBorderedRectangle(int x, int y, int width, int height) = 0;
    virtual void DrawOval(int x, int y, int width, int height) = 0;
    virtual void DrawArcSector(int x, int y, int width, int height, int start, int arc, bool isSector) = 0;
    virtual void DrawPoly(bool isFilled, int nPoints, const int xArray[], const int yArray[]) = 0;
};

class MHTextDisplay {
  public:
    virtual ~MHTextDisplay() {}
    // Draw the completed drawing onto the display.  x and y give the position of the image
    // relative to the screen.  rect gives the bounding box for the image, again relative to
    // the screen.
    virtual void Draw(int x, int y) = 0;
    virtual void SetSize(int width, int height) = 0;
    virtual void SetFont(int size, bool isBold, bool isItalic) = 0;
    // Get the size of a piece of text.  If maxSize is >= 0 it sets strLen to the number
    // of characters that will fit in that number of bits.
    virtual QRect GetBounds(const QString &str, int &strLen, int maxSize = -1) = 0;
    virtual void Clear(void) = 0;
    virtual void AddText(int x, int y, const QString &, MHRgba colour) = 0;
};

class MHBitmapDisplay
{
  public:
    virtual ~MHBitmapDisplay() {}
    // Draw the completed drawing onto the display.  x and y give the position of the image
    // relative to the screen.  rect gives the bounding box for the image, again relative to
    // the screen.
    virtual void Draw(int x, int y, QRect rect, bool tiled, bool bUnder) = 0;
    // Creation functions
    virtual void CreateFromPNG(const unsigned char *data, int length) = 0;
    virtual void CreateFromMPEG(const unsigned char *data, int length) = 0;
    virtual void CreateFromJPEG(const unsigned char *data, int length) = 0;
    // Scale the bitmap.  Only used for image derived from MPEG I-frames.
    virtual void ScaleImage(int newWidth, int newHeight) = 0;
    // Information about the image.
    virtual QSize GetSize() = 0;
    virtual bool IsOpaque() = 0; // True only if the visible area is fully opaque

};

#endif
