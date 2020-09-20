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

#include <unistd.h>
#include <memory>
#include <string.h>

#include "fusion-focus.h"

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

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed
    INDI::Focuser::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE|
                                 FOCUSER_CAN_SYNC | FOCUSER_HAS_VARIABLE_SPEED | FOCUSER_HAS_BACKLASH );

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

        if (!strcmp (name, FocusReverseSP.name)) {
            FocusReverseSP.s = IPS_OK;
            IUUpdateSwitch(&FocusReverseSP, states, names, n);
            int dir = 0;
            if(FocusReverseS[0].s == ISS_ON){
                dir = 1;
            }

            UpdateDirection(dir);
            IDSetSwitch(&FocusReverseSP, nullptr);

            return true;
        }

        if (!strcmp (name, FocusAbortSP.name)) {
            FocusAbortSP.s = IPS_OK;

            IUUpdateSwitch(&FocusAbortSP, states, names, n);
            AbortFocuser();
            IDSetSwitch(&FocusAbortSP, nullptr);

            return true;
        }

       if (strcmp(name, "FOCUS_BACKLASH_TOGGLE") == 0)
        {
            FocusBacklashSP.s = IPS_OK;
            IUUpdateSwitch(&FocusBacklashSP, states, names, n);
            //  Update client display
            IDSetSwitch(&FocusBacklashSP, NULL);

            if(FocusBacklashS[1].s == ISS_ON){
                UpdateBacklash(0);
            }

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

       if (!strcmp (name, FocusSyncNP.name)) {
            IUUpdateNumber(&FocusSyncNP, values, names, n);
            FocusSyncNP.s = IPS_OK;
            IDSetNumber(&FocusSyncNP, NULL);

            return UpdateCurPos(FocusSyncN[0].value);
        }

        if (!strcmp (name, FocusAbsPosNP.name)) {
            IUUpdateNumber(&FocusAbsPosNP, values, names, n);
            FocusAbsPosNP.s = IPS_BUSY;
            IDSetNumber(&FocusAbsPosNP, NULL);

            // Update the max travel required
            return MoveFocuser(values[0]);
        }


        if (!strcmp (name, FocusBacklashNP.name)) {
            IUUpdateNumber(&FocusBacklashNP, values, names, n);
            FocusBacklashNP.s = IPS_OK;
            IDSetNumber(&FocusBacklashNP, NULL);

            // Update the max travel required
            return UpdateBacklash(values[0]);
        }

       if (!strcmp (name, FocusSpeedNP.name)) {
            IUUpdateNumber(&FocusSpeedNP, values, names, n);
            FocusSpeedNP.s = IPS_OK;
            IDSetNumber(&FocusSpeedNP, NULL);

            return UpdateSpeed(FocusSpeedN[0].value);
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

    DEBUG(INDI::Logger::DBG_SESSION, "Fusion Focuser has connected");

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

    DEBUG(INDI::Logger::DBG_SESSION, "Fusion Focuser has disconnected");

    return true;
}

bool FusionFocus::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 5;
    FocusSpeedN[0].value = 1;    

    setDefaultPollingPeriod(POLL_MS);

    DEBUG(INDI::Logger::DBG_DEBUG, "Fusion Focuser initProperties called");

    return true;
}

bool FusionFocus::updateProperties()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "Fusion Focuser updateProperties called");

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
    }
    else
    {
    }

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
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser commanded move to %u", position);

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
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetMove(position);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "Move Focuser failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::UpdateMaxTravel(unsigned int position) 
{
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser Update Max to %u", position);

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_ERROR, "Not Connected!");
        return false;
    }

    if(focusDriver != NULL)
    {
        if(position > 65534){
            position = 65534;
            DEBUGF(INDI::Logger::DBG_DEBUG, "Position truncated to %d", position);
        }

        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetMax(position);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "Set max failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::UpdateCurPos(unsigned int position) {
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser CurPos Max to %u", position);

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_ERROR, "Not Connected!");
        return false;
    }
    
    if(focusDriver != NULL)
    {
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetPosition(position);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "Set position failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::UpdateBacklash(unsigned int backlash) {
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser Update Backlash to %d", backlash);

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_ERROR, "Not Connected!");
        return false;
    }
    
    if(focusDriver != NULL)
    {
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetBacklash(backlash);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "SetBacklash failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::UpdateDirection(int inOut) {
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser Update direction %d", inOut);

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_ERROR, "Not Connected!");
        return false;
    }
    
    if(focusDriver != NULL)
    {
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetDir(inOut);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "SetDir failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::UpdateSpeed(unsigned int speed) {
    DEBUGF(INDI::Logger::DBG_SESSION, "Fusion Focuser Update speed to %u", speed);

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_ERROR, "Not Connected!");
        return false;
    }

    if(speed > 5)
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Speed capped a 5");
        speed = 5;
    }

    if(focusDriver != NULL)
    {
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->SetSpeed(speed);
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "SetSpeed failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}

bool FusionFocus::AbortFocuser()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Aborting focus");

    if(!isConnected()){
        DEBUGF(INDI::Logger::DBG_ERROR, "Focuser not connected in Abort Focuser!", NULL);
        return false;
    }

    if(focusDriver != NULL)
    {
        int retry = 3;
        while(retry != 0) {
            try {
                focusDriver->Abort();
                break;
            } catch (CFusionFocusDriver::CFocusException e) {
                retry--;
                DEBUGF(INDI::Logger::DBG_ERROR, "Abort failed with error %d, retry %d times", e.m_err);
                sleep(0.05);
            }
        }

        if(retry==0){
            return false;
        }

        return true;
    }

    DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL");
    return false;
}



void FusionFocus::TimerHit() {
    // This causes log spamming
    //DEBUG(INDI::Logger::DBG_DEBUG, "TimerHit");

    if (isConnected() == false) {
        DEBUG(INDI::Logger::DBG_DEBUG, "Not Connected!");
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
            DEBUGF(INDI::Logger::DBG_SESSION, "Focus Driver is at %d moving to %d", focusSettings.cur_pos, focusSettings.set_pos);
        }
        else
        {
            FocusAbsPosNP.s = IPS_OK;
        }
        
        FocusMaxPosN[0].min = 0.;
        FocusMaxPosN[0].max = 65535;
        FocusMaxPosN[0].value = focusSettings.max_move;
        FocusMaxPosN[0].step = 100;

        FocusBacklashN[0].value = focusSettings.backlash;
        
        FocusSpeedN[0].value = focusSettings.step_timer;

        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&FocusMaxPosNP, NULL);
        IDSetNumber(&FocusBacklashNP, NULL);
        IDSetNumber(&FocusSpeedNP, NULL);

        IDSetSwitch(&FocusReverseSP, NULL);
    }
    else
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Focus Driver is NULL in TimerHit");
    }

    SetTimer(POLLMS);
}




