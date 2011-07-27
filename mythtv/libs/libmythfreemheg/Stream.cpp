/* Stream.cpp

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

#include "Presentable.h"
#include "Ingredients.h"
#include "Root.h"
#include "BaseClasses.h"
#include "ParseNode.h"
#include "ASN1Codes.h"
#include "Engine.h"
#include "Stream.h"
#include "freemheg.h"


MHStream::MHStream()
{
    m_nStorage = ST_Stream;
    m_nLooping = 0; // Infinity
}

void MHStream::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPresentable::Initialise(p, engine);
    MHParseNode *pMultiplex = p->GetNamedArg(C_MULTIPLEX);

    if (pMultiplex)
    {
        for (int i = 0; i < pMultiplex->GetArgCount(); i++)
        {
            MHParseNode *pItem = pMultiplex->GetArgN(i);

            if (pItem->GetTagNo() == C_AUDIO)
            {
                MHAudio *pAudio = new MHAudio;
                m_Multiplex.Append(pAudio);
                pAudio->Initialise(pItem, engine);
            }
            else if (pItem->GetTagNo() == C_VIDEO)
            {
                MHVideo *pVideo = new MHVideo;
                m_Multiplex.Append(pVideo);
                pVideo->Initialise(pItem, engine);
            }
            else if (pItem->GetTagNo() == C_RTGRAPHICS)
            {
                MHRTGraphics *pRtGraph = new MHRTGraphics;
                m_Multiplex.Append(pRtGraph);
                pRtGraph->Initialise(pItem, engine);
            }
            else
            {
                // Ignore unknown items
                MHLOG(MHLogWarning, QString("WARN unknown stream type %1")
                    .arg(pItem->GetTagNo()));
            }
        }
    }

    MHParseNode *pStorage = p->GetNamedArg(C_STORAGE);

    if (pStorage)
    {
        m_nStorage = (enum Storage) pStorage->GetArgN(0)->GetEnumValue();
    }

    MHParseNode *pLooping = p->GetNamedArg(C_LOOPING);

    if (pLooping)
    {
        m_nLooping = pLooping->GetArgN(0)->GetIntValue();
    }
}

void MHStream::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Stream ");
    MHPresentable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":Multiplex (\n");

    for (int i = 0; i < m_Multiplex.Size(); i++)
    {
        m_Multiplex.GetAt(i)->PrintMe(fd, nTabs + 2);
    }

    PrintTabs(fd, nTabs + 1);
    fprintf(fd, " )\n");

    if (m_nStorage != ST_Stream)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":Storage memory\n");
    }

    if (m_nLooping != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, ":Looping %d\n", m_nLooping);
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHStream::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;    // Already prepared
    }

    for (int i = 0; i < m_Multiplex.Size(); i++)
    {
        MHPresentable *pItem = m_Multiplex.GetAt(i);

        if (pItem->InitiallyActive())
        {
            pItem->Activation(engine); // N.B.  This will also call Preparation for the components.
        }
    }

    MHPresentable::Preparation(engine);
}

void MHStream::Destruction(MHEngine *engine)
{
    // Apply Destruction in reverse order.
    for (int j = m_Multiplex.Size(); j > 0; j--)
    {
        m_Multiplex.GetAt(j - 1)->Destruction(engine);
    }

    MHPresentable::Destruction(engine);
}

void MHStream::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHPresentable::Activation(engine);

    // Start playing all active stream components.
    BeginPlaying(engine);
    // subclasses are responsible for setting m_fRunning and generating IsRunning.
    m_fRunning = true;
    engine->EventTriggered(this, EventIsRunning);
}

void MHStream::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    MHPresentable::Deactivation(engine);
    StopPlaying(engine);
}

// The MHEG corrigendum allows SetData to be targeted to a stream so
// the content ref could change while the stream is playing.
void MHStream::ContentPreparation(MHEngine *engine)
{
    engine->EventTriggered(this, EventContentAvailable); // Perhaps test for the streams being available?
    if (m_fRunning)
        BeginPlaying(engine);
}

// Return an object if there is a matching component.
MHRoot *MHStream::FindByObjectNo(int n)
{
    if (n == m_ObjectReference.m_nObjectNo)
    {
        return this;
    }

    for (int i = m_Multiplex.Size(); i > 0; i--)
    {
        MHRoot *pResult = m_Multiplex.GetAt(i - 1)->FindByObjectNo(n);

        if (pResult)
        {
            return pResult;
        }
    }

    return NULL;
}

void MHStream::BeginPlaying(MHEngine *engine)
{
    QString stream;
    MHOctetString &str = m_ContentRef.m_ContentRef;
    if (str.Size() != 0) stream = QString::fromUtf8((const char *)str.Bytes(), str.Size());
    if ( !engine->GetContext()->BeginStream(stream, this))
        engine->EventTriggered(this, EventEngineEvent, 204); // StreamRefError

    // Start playing all active stream components.
    for (int i = 0; i < m_Multiplex.Size(); i++)
        m_Multiplex.GetAt(i)->BeginPlaying(engine);

    //engine->EventTriggered(this, EventStreamPlaying);
}

void MHStream::StopPlaying(MHEngine *engine)
{
    // Stop playing all active Stream components
    for (int i = 0; i < m_Multiplex.Size(); i++)
        m_Multiplex.GetAt(i)->StopPlaying(engine);
    engine->GetContext()->EndStream();
    engine->EventTriggered(this, EventStreamStopped);
}

void MHStream::GetCounterPosition(MHRoot *pResult, MHEngine *engine)
{
    // StreamCounterUnits (mS)
    pResult->SetVariableValue((int)engine->GetContext()->GetStreamPos());
}

void MHStream::GetCounterMaxPosition(MHRoot *pResult, MHEngine *engine)
{
    // StreamCounterUnits (mS)
    pResult->SetVariableValue((int)engine->GetContext()->GetStreamMaxPos());
}

void MHStream::SetCounterPosition(int pos, MHEngine *engine)
{
    // StreamCounterUnits (mS)
    engine->GetContext()->SetStreamPos(pos);
}

void MHStream::SetSpeed(int speed, MHEngine *engine)
{
    engine->GetContext()->StreamPlay(speed);
}

MHAudio::MHAudio()
{
    m_nComponentTag = 0;
    m_nOriginalVol = 0;
    m_fStreamPlaying = false;
}

void MHAudio::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHPresentable::Initialise(p, engine);
    MHParseNode *pComponentTagNode =  p->GetNamedArg(C_COMPONENT_TAG);

    if (pComponentTagNode)
    {
        m_nComponentTag = pComponentTagNode->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pOrigVol = p->GetNamedArg(C_ORIGINAL_VOLUME);

    if (pOrigVol)
    {
        m_nOriginalVol = pOrigVol->GetIntValue();
    }
}

void MHAudio::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Audio ");
    MHPresentable::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":ComponentTag %d\n", m_nComponentTag);

    if (m_nOriginalVol != 0)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, "OriginalVolume %d ", m_nOriginalVol);
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

// Activation for Audio is defined in the corrigendum
void MHAudio::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHPresentable::Activation(engine);
    // Beginning presentation is started by the Stream object.
    m_fRunning = true;
    engine->EventTriggered(this, EventIsRunning);

    if (m_fStreamPlaying)
        engine->GetContext()->BeginAudio(m_nComponentTag);
}

// Deactivation for Audio is defined in the corrigendum
void MHAudio::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    m_fRunning = false;

    // Stop presenting the audio
    if (m_fStreamPlaying)
    {
        engine->GetContext()->StopAudio();
    }

    MHPresentable::Deactivation(engine);
}

void MHAudio::BeginPlaying(MHEngine *engine)
{
    m_fStreamPlaying = true;
    if (m_fRunning)
        engine->GetContext()->BeginAudio(m_nComponentTag);
}

void MHAudio::StopPlaying(MHEngine *engine)
{
    m_fStreamPlaying = false;

    if (m_fRunning)
    {
        engine->GetContext()->StopAudio();
    }
}


MHVideo::MHVideo()
{
    m_nComponentTag = 0;
    m_Termination = VI_Disappear;
    m_nXDecodeOffset = 0;
    m_nYDecodeOffset = 0;
    m_nDecodeWidth = 0;
    m_nDecodeHeight = 0;

    m_fStreamPlaying = false;
}

void MHVideo::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    MHParseNode *pComponentTagNode = p->GetNamedArg(C_COMPONENT_TAG);

    if (pComponentTagNode)
    {
        m_nComponentTag = pComponentTagNode->GetArgN(0)->GetIntValue();
    }

    MHParseNode *pTerm = p->GetNamedArg(C_TERMINATION);

    if (pTerm)
    {
        m_Termination = (enum Termination)pTerm->GetEnumValue();
    }
}

void MHVideo::PrintMe(FILE *fd, int nTabs) const
{
    PrintTabs(fd, nTabs);
    fprintf(fd, "{:Video ");
    MHVisible::PrintMe(fd, nTabs + 1);
    PrintTabs(fd, nTabs + 1);
    fprintf(fd, ":ComponentTag %d\n", m_nComponentTag);

    if (m_Termination != VI_Disappear)
    {
        PrintTabs(fd, nTabs + 1);
        fprintf(fd, "Termination freeze ");
    }

    PrintTabs(fd, nTabs);
    fprintf(fd, "}\n");
}

void MHVideo::Preparation(MHEngine *engine)
{
    if (m_fAvailable)
    {
        return;    // Already prepared
    }

    MHVisible::Preparation(engine); // Prepare the base class.
    // Set up the internal attributes after MHVisible::Preparation.
    m_nDecodeWidth = m_nBoxWidth;
    m_nDecodeHeight = m_nBoxHeight;
}

void MHVideo::ContentPreparation(MHEngine *engine)
{
    // Pretend it's available.
    engine->EventTriggered(this, EventContentAvailable);
}

// Display the video.
void MHVideo::Display(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    if (m_nBoxWidth == 0 || m_nBoxHeight == 0)
    {
        return;    // Can't draw zero sized boxes.
    }

    // The bounding box is assumed always to be True.
    // The full screen video is displayed in this rectangle.  It is therefore scaled to
    // m_nDecodeWidth/720 by m_nDecodeHeight/576.
    QRect videoRect = QRect(m_nPosX + m_nXDecodeOffset, m_nPosY + m_nYDecodeOffset,
                            m_nDecodeWidth, m_nDecodeHeight);
    QRect displayRect = videoRect.intersect(QRect(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight));
    engine->GetContext()->DrawVideo(videoRect, displayRect);
}

void MHVideo::ScaleVideo(int xScale, int yScale, MHEngine *engine)
{
    if (xScale == m_nDecodeWidth && yScale == m_nDecodeHeight)
    {
        return;
    }

    QRegion updateArea = GetVisibleArea(); // Redraw the area before the offset
    m_nDecodeWidth = xScale;
    m_nDecodeHeight = yScale;
    updateArea += GetVisibleArea(); // Redraw this bitmap.
    engine->Redraw(updateArea); // Mark for redrawing
}

// Added action in UK MHEG.
void MHVideo::SetVideoDecodeOffset(int newXOffset, int newYOffset, MHEngine *engine)
{
    QRegion updateArea = GetVisibleArea(); // Redraw the area before the offset
    m_nXDecodeOffset = newXOffset;
    m_nYDecodeOffset = newYOffset;
    updateArea += GetVisibleArea(); // Redraw the resulting area.
    engine->Redraw(updateArea); // Mark for redrawing
}

// Added action in UK MHEG.
void MHVideo::GetVideoDecodeOffset(MHRoot *pXOffset, MHRoot *pYOffset, MHEngine *)
{
    pXOffset->SetVariableValue(m_nXDecodeOffset);
    pYOffset->SetVariableValue(m_nYDecodeOffset);
}

// Return the region drawn by the bitmap.
QRegion MHVideo::GetVisibleArea()
{
    if (! m_fRunning)
    {
        return QRegion();
    }

    // The visible area is the intersection of the containing box with the, possibly offset,
    // video.
    QRegion boxRegion = QRegion(m_nPosX, m_nPosY, m_nBoxWidth, m_nBoxHeight);
    QRegion videoRegion = QRegion(m_nPosX + m_nXDecodeOffset, m_nPosY + m_nYDecodeOffset,
                                  m_nDecodeWidth, m_nDecodeHeight);
    return boxRegion & videoRegion;
}

void MHVideo::Activation(MHEngine *engine)
{
    if (m_fRunning)
    {
        return;
    }

    MHVisible::Activation(engine);
    if (m_fStreamPlaying)
        engine->GetContext()->BeginVideo(m_nComponentTag);
}

void MHVideo::Deactivation(MHEngine *engine)
{
    if (! m_fRunning)
    {
        return;
    }

    MHVisible::Deactivation(engine);

    if (m_fStreamPlaying)
    {
        engine->GetContext()->StopVideo();
    }
}

void MHVideo::BeginPlaying(MHEngine *engine)
{
    m_fStreamPlaying = true;
    if (m_fRunning)
        engine->GetContext()->BeginVideo(m_nComponentTag);
}

void MHVideo::StopPlaying(MHEngine *engine)
{
    m_fStreamPlaying = false;

    if (m_fRunning)
    {
        engine->GetContext()->StopVideo();
    }
}


MHRTGraphics::MHRTGraphics()
{

}

MHRTGraphics::~MHRTGraphics()
{

}

void MHRTGraphics::Initialise(MHParseNode *p, MHEngine *engine)
{
    MHVisible::Initialise(p, engine);
    //
}

void MHRTGraphics::PrintMe(FILE *fd, int nTabs) const
{
    MHVisible::PrintMe(fd, nTabs);
    //
}

// Fix for MHActionGenericObjectRef
void MHActionGenericObjectRefFix::Perform(MHEngine *engine)
{
    MHObjectRef ref;
    if (m_RefObject.m_fIsDirect)
        m_RefObject.GetValue(ref, engine);
    else
        ref.Copy(*m_RefObject.GetReference());
    CallAction(engine, Target(engine), engine->FindObject(ref));
}
