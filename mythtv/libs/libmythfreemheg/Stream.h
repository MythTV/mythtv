/* Stream.h

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

#if !defined(STREAM_H)
#define STREAM_H

#include "Presentable.h"
// Dependencies
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "Actions.h"

class MHStream : public MHPresentable  
{
public:
    MHStream();
    virtual const char *ClassName() { return "Stream"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);
    virtual void Destruction(MHEngine *engine);
    virtual void ContentPreparation(MHEngine *engine);

    virtual MHRoot *FindByObjectNo(int n);
protected:
    MHOwnPtrSequence <MHPresentable> m_Multiplex;
    enum Storage { ST_Mem = 1, ST_Stream = 2 } m_nStorage;
    int         m_nLooping;
};


class MHAudio : public MHPresentable  
{
public:
    MHAudio();
    virtual const char *ClassName() { return "Audio"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    virtual void SetStreamRef(const MHContentRef &);
    virtual void BeginPlaying(MHEngine *engine);
    virtual void StopPlaying(MHEngine *engine);

protected:
    int m_nComponentTag;
    int m_nOriginalVol;

    bool m_fStreamPlaying;
    MHContentRef m_streamContentRef;
};

class MHVideo : public MHVisible  
{
public:
    MHVideo();
    virtual const char *ClassName() { return "Video"; }
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;

    virtual void Preparation(MHEngine *engine);
    virtual void ContentPreparation(MHEngine *engine);

    virtual void Activation(MHEngine *engine);
    virtual void Deactivation(MHEngine *engine);

    virtual void Display(MHEngine *);
    virtual QRegion GetVisibleArea();
    virtual QRegion GetOpaqueArea() { return GetVisibleArea(); } // Fully opaque.

    // Actions.
    virtual void ScaleVideo(int xScale, int yScale, MHEngine *);
    virtual void SetVideoDecodeOffset(int newXOffset, int newYOffset, MHEngine *);
    virtual void GetVideoDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset, MHEngine *);

    virtual void SetStreamRef(const MHContentRef &);
    virtual void BeginPlaying(MHEngine *engine);
    virtual void StopPlaying(MHEngine *engine);

protected:
    int m_nComponentTag;
    enum Termination { VI_Freeze = 1, VI_Disappear } m_Termination;
    // Added in UK MHEG
    int     m_nXDecodeOffset, m_nYDecodeOffset;
    int     m_nDecodeWidth, m_nDecodeHeight;

    bool m_fStreamPlaying;
    MHContentRef m_streamContentRef;
};

// Real-time graphics - not needed for UK MHEG.
class MHRTGraphics : public MHVisible  
{
public:
    MHRTGraphics();
    virtual const char *ClassName() { return "RTGraphics"; }
    virtual ~MHRTGraphics();
    virtual void Initialise(MHParseNode *p, MHEngine *engine);
    virtual void PrintMe(FILE *fd, int nTabs) const;
    virtual void Display(MHEngine *) {} // Not supported
};


class MHScaleVideo: public MHActionIntInt {
public:
    MHScaleVideo(): MHActionIntInt(":ScaleVideo") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) { pTarget->ScaleVideo(nArg1, nArg2, engine); }
};

// Actions added in the UK MHEG profile.
class MHSetVideoDecodeOffset: public MHActionIntInt
{
public:
    MHSetVideoDecodeOffset(): MHActionIntInt(":SetVideoDecodeOffset") {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg1, int nArg2) { pTarget->SetVideoDecodeOffset(nArg1, nArg2, engine); }
};

class MHGetVideoDecodeOffset: public MHActionObjectRef2
{
public:
    MHGetVideoDecodeOffset(): MHActionObjectRef2(":GetVideoDecodeOffset")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg1, MHRoot *pArg2) { pTarget->GetVideoDecodeOffset(pArg1, pArg2, engine); }
};


#endif
