/**
 *  FirewireChannelBase
 *  Copyright (c) 2005 by Jim Westfall Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannelbase.h"

bool FirewireChannelBase::SetChannelByString(const QString &chan)
{
    inputs[currentInputID]->startChanNum = chan; 
    curchannelname = chan;

    InputMap::const_iterator it = inputs.find(currentInputID);

    if (!(*it)->externalChanger.isEmpty()) 
        return ChangeExternalChannel(chan);

    return isopen && SetChannelByNumber(chan.toInt());
}

bool FirewireChannelBase::Open()
{
    if (!InitializeInputs()) 
        return false; 

    InputMap::const_iterator it = inputs.find(currentInputID); 
    if (!(*it)->externalChanger.isEmpty()) 
        return true;

    if (!isopen)
    {
        isopen = OpenFirewire(); 
        return isopen; 
    }
    return true;
}

void FirewireChannelBase::Close()
{
    if (isopen)
        CloseFirewire();
    isopen = false;
}
    
bool FirewireChannelBase::SwitchToInput(const QString &input,
                                        const QString &chan)
{
    int inputNum = GetInputByName(input); 
    if (inputNum < 0) 
        return false;

    return SetChannelByString(chan);
}
