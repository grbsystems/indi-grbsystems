/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "fusion-focus.h"
#include <memory>
#include <string.h>

#define POLL_MS  1000
#define MAX_STR 255
#define BUF_SIZE 64

std::unique_ptr<FusionFocus> fusion(new FusionFocus());

void ISGetProperties(const char *dev)
{
    fusion->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    fusion->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    fusion->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    fusion->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    fusion->ISSnoopDevice(root);
}

FusionFocus::FusionFocus()
{
    // We use a i2c connection0.000
    setSupportedConnections(CONNECTION_NONE);

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.        
    INDI::Focuser::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC );

    timerid = -1;

    focusDriver = NULL;
}

FusionFocus::~FusionFocus()
{

}

bool FusionFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp (name, FocusMotionSP.name)) {
            FocusMotionSP.s = IPS_OK;
            IUUpdateSwitch(&FocusMotionSP, states, names, n);
            int dir = 0;
            if(FocusMotionS[FOCUS_OUTWARD].s == ISS_ON){
                dir = 1;
            }

            UpdateDirection(dir);
            IDSetSwitch(&FocusMotionSP, nullptr);

            return true;
        }

        if (!strcmp (name, FocusAbortSP.name)) {
            FocusAbortSP.s = IPS_OK;
            
            IUUpdateSwitch(&FocusAbortSP, states, names, n);
            AbortFocuser();
            IDSetSwitch(&FocusAbortSP, nullptr);

            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool FusionFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp (name, FocusMaxPosNP.name)) {
            IUUpdateNumber(&FocusMaxPosNP, values, names, n);
            FocusMaxPosNP.s = IPS_OK;
            IDSetNumber(&FocusMaxPosNP, NULL);

            // Update the max travel required
            return UpdateMaxTravel(values[0]);
        }

        if (!strcmp (name, FocusAbsPosNP.name)) {
            IUUpdateNumber(&FocusAbsPosNP, values, names, n);
            FocusAbsPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusAbsPosNP, NULL);

            // Update the max travel required
            return MoveFocuser(values[0]);
        }

        if (!strcmp (name, FocusSyncNP.name)) {
            IUUpdateNumber(&FocusSyncNP, values, names, n);
            FocusSyncNP.s = IPS_OK;
            IDSetNumber(&FocusSyncNP, NULL);

            // Update the max travel required
            return UpdateCurPos(values[0]);
        }

        return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);;
}


bool FusionFocus::Connect(){
    int res;
    wchar_t wstr[MAX_STR+1];

    if(focusDriver != NULL)
    {
        delete focusDriver;
        focusDriver = NULL;
    }

    focusDriver = new CFusionFocusDriver();
    focusDriver->GetSettings(&focusSettings);

    timerid = SetTimer(POLL_MS);
    
    return true;
}

bool FusionFocus::Disconnect(){

    if(timerid != -1){
        RemoveTimer(timerid);
        timerid = -1;
    }

    if(focusDriver != NULL)
    {
        delete focusDriver;
        focusDriver = NULL;
    }

    return true;
}

bool FusionFocus::initProperties()
{
    INDI::Focuser::initProperties();

    setDefaultPollingPeriod(POLL_MS);

    return true;

}

bool FusionFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    return true;

}

bool FusionFocus::Handshake()
{
    return true;
}

const char * FusionFocus::getDefaultName()
{
    return "Fusion Focuser";
}

bool FusionFocus::MoveFocuser(unsigned int position)
{
    if (position < FocusAbsPosN[0].min || position > FocusAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value out of bound: %d", position);
        return false;
    }

    if(!isConnected()){
        DEBUGF(INDI::Logger::DBG_ERROR, "Focuser not connected!", NULL);
        return false;
    }

    if(focusDriver != NULL)
    {
        focusDriver->SetMove(position);
    }
    else
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NUll in Move Focuser");
    }
    
    return true;
}

bool FusionFocus::UpdateMaxTravel(unsigned int position) 
{
    if(focusDriver != NULL)
    {
        if(position > 65534){
            position = 65534;
        }

        focusDriver->SetMax(position);
    }

    return true;
}

bool FusionFocus::UpdateCurPos(unsigned int position) {
    if (isConnected() == false) { 
        return false;
    }
    
    if(focusDriver != NULL)
    {
        focusDriver->SetPosition(position);
        return true;
    }
    
    return false;

    return true;
}

bool FusionFocus::UpdateDirection(int inOut) {
    if (isConnected() == false) { 
        return false;
    }
    
    if(focusDriver != NULL)
    {
        focusDriver->SetDir(inOut);
        return true;
    }
    
    return false;
}

void FusionFocus::TimerHit() {
    if (isConnected() == false) { 
        return;
    }
    
    if(focusDriver != NULL)
    {
        focusDriver->GetSettings(&focusSettings);

        FocusAbsPosN[0].min = 0.;
        FocusAbsPosN[0].max = focusSettings.max_move;
        FocusAbsPosN[0].value = focusSettings.cur_pos;
        FocusAbsPosN[0].step = 100;

        if(focusSettings.cur_pos != focusSettings.set_pos)
        {
            FocusAbsPosNP.s = IPS_BUSY;
        }
        else
        {
            FocusAbsPosNP.s = IPS_OK;
        }
        
        FocusMaxPosN[0].min = 0.;
        FocusMaxPosN[0].max = 65535;
        FocusMaxPosN[0].value = focusSettings.max_move;
        FocusMaxPosN[0].step = 100;


        FocusSpeedN[0].min = 1;
        FocusSpeedN[0].max = 5;
        FocusSpeedN[0].value = 1;    

        IDSetNumber(&FocusSpeedNP, NULL);
        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&FocusMaxPosNP, NULL);
    }

    SetTimer(POLLMS);
}

bool FusionFocus::AbortFocuser()
{
    if(!isConnected()){
        DEBUGF(INDI::Logger::DBG_ERROR, "Focuser not connected in Abort Focuser!", NULL);
        return false;
    }

    if(focusDriver != NULL)
    {
        focusDriver->Abort();
    }
    else
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NUll in Abort Focuser");
    }

    return true;
}




