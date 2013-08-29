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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

    virtual void BeginPlaying(MHEngine *engine);
    virtual void StopPlaying(MHEngine *engine);

    // Actions
    virtual void GetCounterPosition(MHRoot *, MHEngine *);
    virtual void GetCounterMaxPosition(MHRoot *, MHEngine *);
    virtual void SetCounterPosition(int /*pos*/, MHEngine *);
    virtual void SetSpeed(int, MHEngine *engine);

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

    virtual void BeginPlaying(MHEngine *engine);
    virtual void StopPlaying(MHEngine *engine);

  protected:
    int m_nComponentTag;
    int m_nOriginalVol;

    bool m_fStreamPlaying;
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

    virtual void BeginPlaying(MHEngine *engine);
    virtual void StopPlaying(MHEngine *engine);

  protected:
    int m_nComponentTag;
    enum Termination { VI_Freeze = 1, VI_Disappear } m_Termination;
    // Added in UK MHEG
    int     m_nXDecodeOffset, m_nYDecodeOffset;
    int     m_nDecodeWidth, m_nDecodeHeight;

    bool m_fStreamPlaying;
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

class MHActionGenericObjectRefFix: public MHActionGenericObjectRef
{
public:
    MHActionGenericObjectRefFix(const char *name) : MHActionGenericObjectRef(name) {}
    virtual void Perform(MHEngine *engine);
};

class MHGetCounterPosition: public MHActionGenericObjectRefFix
{
public:
    MHGetCounterPosition(): MHActionGenericObjectRefFix(":GetCounterPosition")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg)
        { pTarget->GetCounterPosition(pArg, engine); }
};

class MHGetCounterMaxPosition: public MHActionGenericObjectRefFix
{
public:
    MHGetCounterMaxPosition(): MHActionGenericObjectRefFix(":GetCounterMaxPosition")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, MHRoot *pArg)
        { pTarget->GetCounterMaxPosition(pArg, engine); }
};

class MHSetCounterPosition: public MHActionInt
{
public:
    MHSetCounterPosition(): MHActionInt(":SetCounterPosition")  {}
    virtual void CallAction(MHEngine *engine, MHRoot *pTarget, int nArg)
        { pTarget->SetCounterPosition(nArg, engine); }
};


class MHSetSpeed: public MHElemAction
{
    typedef MHElemAction base;
public:
    MHSetSpeed(): base(":SetSpeed") {}
    virtual void Initialise(MHParseNode *p, MHEngine *engine) {
        //printf("SetSpeed Initialise args: "); p->PrintMe(stdout);
        base::Initialise(p, engine);
        MHParseNode *pn = p->GetArgN(1);
        if (pn->m_nNodeType == MHParseNode::PNSeq) pn = pn->GetArgN(0);
        m_Argument.Initialise(pn, engine);
    }
    virtual void Perform(MHEngine *engine) {
        Target(engine)->SetSpeed(m_Argument.GetValue(engine), engine);
    }
protected:
    virtual void PrintArgs(FILE *fd, int) const { m_Argument.PrintMe(fd, 0); }
    MHGenericInteger m_Argument;
};


#endif
