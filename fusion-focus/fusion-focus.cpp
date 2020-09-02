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
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT );

    lastPos = 0;
    timerid = -1;
    targetPos = -1;

    focusDriver = NULL;
}

FusionFocus::~FusionFocus()
{

}

bool FusionFocus::Connect(){
    int res;
    wchar_t wstr[MAX_STR+1];
    char cstr[MAX_STR+1];

    timerid = SetTimer(POLL_MS);

    if(focusDriver != NULL)
    {
        delete focusDriver;
        focusDriver = NULL;
    }

    focusDriver = new CFusionFocusDriver();
    focusDriver->GetSettings(&focusSettings);

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

    // Maximum Travel
    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1.,
                 60000, 0., focusSettings.max_move);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    IUFillNumber(&SetPositionN[0], "CURPOS", "Current Position", "%6.0f", 1.,
                 60000., 0., focusSettings.cur_pos);
    IUFillNumberVector(&SetPositionNP, SetPositionN, 1, getDeviceName(), "FOCUS_CURPOS", "Cur. Position", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    //DEBUG(INDI::Logger::DBG_SESSION, "Adding direction properties");
    IUFillSwitch(&PositiveMotionS[0],"NORMAL","Normal",ISS_ON);
    IUFillSwitch(&PositiveMotionS[1],"REVERSE","Reverse",ISS_OFF);
    IUFillSwitchVector(&PositiveMotionSP,PositiveMotionS,2,getDeviceName(),"POSITIVE_MOTION","Direction",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusMaxPosNP, NULL);
    IDSetNumber(&SetPositionNP, NULL);
    IDSetNumber(&MaxTravelNP, NULL);
    setDefaultPollingPeriod(POLL_MS);

    return true;

}

bool FusionFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&MaxTravelNP);
        defineNumber(&SetPositionNP);
        defineSwitch(&PositiveMotionSP);

        GetFocusParams();

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "GRBSystems paramaters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(SetPositionNP.name);
        deleteProperty(MaxTravelNP.name);
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
    DEBUGF(INDI::Logger::DBG_ERROR, "MoveFocuser", NULL);

    if (position < FocusAbsPosN[0].min || position > FocusAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Requested position value out of bound: %d", position);
        return false;
    }

    if(!isConnected()){
        DEBUGF(INDI::Logger::DBG_ERROR, "Focuser not connected!", NULL);
        return false;
    }

    // if(handle == NULL){
    //     DEBUGF(INDI::Logger::DBG_ERROR, "handle is NULL! This shouldn't happen!", NULL);
    //     return false;
    // }

    // if ((PositiveMotionS[0].s == ISS_ON) && (report.direction != 0)){
    //     // The direction has been flipped so we need to update the prefs to match
    //     //DEBUGF(INDI::Logger::DBG_DEBUG, "Set direction to positive out", NULL);

    //     UpdateDirection(true);
    // } else if((PositiveMotionS[1].s == ISS_ON) && (report.direction == 0)){
    //     //DEBUGF(INDI::Logger::DBG_DEBUG, "Set direction to positive in", NULL);
    //     UpdateDirection(false);
    // }

    targetPos = position;
    DEBUGF(INDI::Logger::DBG_ERROR, "TargetPos set to %d", targetPos);

    // Build out the HID report for a move absolute
    unsigned char buf[BUF_SIZE];
    unsigned char top = ((position & 0xff00) >> 8);
    unsigned char bottom = (position & 0x00ff);

    buf[0] = 0x00;      // Header byte
    buf[1] = 0x11;      // Move Abs
    buf[2] = 0x00;      // channel 0
    buf[3] = top;
    buf[4] = bottom;

    DEBUGF(INDI::Logger::DBG_ERROR, "Writing top, bottom: %d, %d", top, bottom);

    int res;
    //res = hid_write(handle, buf, BUF_SIZE);
    if(res != BUF_SIZE){
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to write move buffer: %d bytes sent", res);
        return false;
    }

    //report.isMoving = true;

    return true;
}

bool FusionFocus::UpdateMaxTravel(unsigned int position) {
    //REPORT newRep = report;

    //newRep.maximum = position;

    //return UpdatePrefs(&newRep);

    return true;
}

bool FusionFocus::UpdateCurPos(unsigned int position) {
    //REPORT newRep = report;

    //newRep.position = position;

    //return UpdatePrefs(&newRep);

    return true;
}

bool FusionFocus::UpdateDirection(bool outPositive) {
    // REPORT newRep = report;

    // newRep.direction = outPositive ? 0 : 1;

    // return UpdatePrefs(&newRep);

    return true;
}

bool FusionFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"POSITIVE_MOTION")==0)
        {
            //  client is telling us what to do with focus direction
            PositiveMotionSP.s=IPS_OK;
            IUUpdateSwitch(&PositiveMotionSP,states,names,n);
            //  Update client display
            IDSetSwitch(&PositiveMotionSP,NULL);

            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool FusionFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp (name, MaxTravelNP.name)) {
            IUUpdateNumber(&MaxTravelNP, values, names, n);
            MaxTravelNP.s = IPS_OK;
            IDSetNumber(&MaxTravelNP, NULL);

            // Update the max travel required

            return UpdateMaxTravel(MaxTravelN[0].value);
        }

        if (!strcmp (name, SetPositionNP.name)) {
            IUUpdateNumber(&SetPositionNP, values, names, n);
            SetPositionNP.s = IPS_OK;
            IDSetNumber(&SetPositionNP, NULL);

            // Update the max travel required

            return UpdateCurPos(SetPositionN[0].value);
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void FusionFocus::GetFocusParams ()
{
    //IDSetNumber(&FocusAbsPosNP, NULL);
    //IDSetNumber(&SetPositionNP, NULL);
    //IDSetSwitch(&PositiveMotionSP,NULL);
}

IPState FusionFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "MoveAbsFocuser", NULL);    // if (handle == NULL) {
    //     DEBUGF(INDI::Logger::DBG_ERROR, "isConnected is true, but there is no hid handle!", NULL);
    //     timerid = SetTimer(POLL_MS);
    //     return;
    // }

    bool rc;

    rc = MoveFocuser(targetTicks);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState FusionFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "MoveRelFocuser", NULL);

    double newPosition=0;
    bool rc=false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = MoveFocuser(newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

void FusionFocus::TimerHit() {

    if (isConnected() == false) {
        timerid = SetTimer(POLL_MS);
        return;
    }

    if(focusDriver != NULL)
    {
        focusDriver->GetSettings(&focusSettings);

        // FocusAbsPosN[0].value = focusSettings.cur_pos;
        // FocusRelPosN[0].value = focusSettings.cur_pos;

        // SetPositionN[0].value = focusSettings.set_pos;

        // MaxTravelN[0].value = focusSettings.max_move;

        // FocusAbsPosN[0].max = focusSettings.max_move;
        // FocusRelPosN[0].max = focusSettings.max_move;

        // if (focusSettings.set_pos != focusSettings.cur_pos) {
        //     FocusRelPosNP.s = IPS_BUSY;
        //     FocusAbsPosNP.s = IPS_BUSY;
        // } else {
        //     FocusRelPosNP.s = IPS_OK;
        //     FocusAbsPosNP.s = IPS_OK;
        // }


        FocusAbsPosN[0].min = 0;
        FocusAbsPosN[0].max = focusSettings.max_move;
        FocusAbsPosN[0].value = focusSettings.cur_pos;
        FocusAbsPosN[0].step = 100;

        FocusMaxPosN[0].min = 0.;
        FocusMaxPosN[0].max = focusSettings.max_move;
        FocusMaxPosN[0].value = focusSettings.max_move;
        FocusMaxPosN[0].step = 250;


        FocusSpeedN[0].min = 1;
        FocusSpeedN[0].max = 5;
        FocusSpeedN[0].value = 1;    

        IDSetNumber(&FocusSpeedNP, NULL);
        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&FocusMaxPosNP, NULL);
//         IDSetNumber(&MaxTravelNP, NULL);

        timerid = SetTimer(POLL_MS);
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "isConnected is true, but there is no focuser object!", NULL);
        timerid = SetTimer(POLL_MS);
        return;
    }
}

void* FusionFocus::Reader(void *thread_params)
{
    // GRBSystems* sys = (GRBSystems*)thread_params;

    // sys->DoRead();

    return NULL;
}

#define DATA_OFFSET  4

void FusionFocus::DoRead()
{
    int res;
    unsigned char buf[BUF_SIZE];

    // while(keep_running){
    //     haveReport = false;

    //     res = hid_read(handle, buf, BUF_SIZE);
    //     if (res == BUF_SIZE) {
    //         report.isMoving = (buf[DATA_OFFSET] != 0);
    //         report.position = buf[DATA_OFFSET + 2] + (buf[DATA_OFFSET + 1] << 8);
    //         report.maximum = buf[DATA_OFFSET + 4] + (buf[DATA_OFFSET + 3] << 8);
    //         report.pulse = buf[DATA_OFFSET + 5];
    //         report.direction = buf[DATA_OFFSET + 6];
    //         report.backlash = buf[DATA_OFFSET + 8] + (buf[DATA_OFFSET + 7] << 8);
    //         report.microns = buf[DATA_OFFSET + 10] + (buf[DATA_OFFSET + 9] << 8);

    //         if(targetPos == -1){
    //             targetPos = report.position;
    //         }

    //         haveReport = true;
    //     }
    // }
}



bool FusionFocus::AbortFocuser()
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Aborting Move", NULL);

    unsigned char buf[BUF_SIZE];

    buf[0] = 0x00;      // Header byte
    buf[1] = 0x13;      // Stop
    buf[2] = 0x00;      // Channel 0

    // int res;
    // res = hid_write(handle, buf, BUF_SIZE);
    // if(res != BUF_SIZE){
    //     DEBUGF(INDI::Logger::DBG_ERROR, "Failed to stop: %d bytes sent", res);
    //     return false;
    // }

    // Force a position reset
    targetPos = -1;

    return true;
}



