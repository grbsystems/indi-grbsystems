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

#include "grbsystems_focus.h"
#include <memory>
#include <string.h>

#define POLL_MS  1000
#define MAX_STR 255
#define BUF_SIZE 64

std::unique_ptr<GRBSystems> grbSystems(new GRBSystems());

void ISGetProperties(const char *dev)
{
    grbSystems->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    grbSystems->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    grbSystems->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    grbSystems->ISNewNumber(dev, name, values, names, num);
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
    grbSystems->ISSnoopDevice(root);
}

GRBSystems::GRBSystems()
{
    // We use a hid connection
    setFocuserConnection(CONNECTION_NONE);

    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.        
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT );

    lastPos = 0;
    haveReport = false;

    handle = NULL;
    timerid = -1;

    targetPos = -1;
}

GRBSystems::~GRBSystems()
{

}

bool GRBSystems::Connect(){
    int res;
     wchar_t wstr[MAX_STR+1];
    char cstr[MAX_STR+1];

    // Open the device using the VID, PID,
    // and optionally the Serial number.
    handle = hid_open(0x4d8, 0x3f, NULL);

    if(handle != NULL) {
        // Read the Manufacturer String
        res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
        if(res == 0){
            wcstombs(cstr, wstr, MAX_STR);
            IDMessage(getDeviceName(), "Manufacturer: %s", cstr);
        }

        // Read the Product String
        res = hid_get_product_string(handle, wstr, MAX_STR);
        if(res == 0){
            wcstombs(cstr, wstr, MAX_STR);
            IDMessage(getDeviceName(), "Product: %s", cstr);
        }

        IDMessage(getDeviceName(), "GRBSystems focuser connected sucessfully!");

        timerid = SetTimer(POLL_MS);

        // Start the reader thread
        keep_running = true;
        if(pthread_create(&reader_thread, NULL, Reader, this)){
            IDMessage(getDeviceName(), "Error creating reader thread");
            return false;
        }

        return true;
    }

    IDMessage(getDeviceName(), "GRBSystems cannot connect!");

    return false;
}

bool GRBSystems::Disconnect(){

    keep_running = false;
    pthread_join(reader_thread, NULL);

    if(handle != NULL){
        hid_close(handle);

        handle = NULL;
    }

    IDMessage(getDeviceName(), "GRBSystems Focuser disconnected successfully!");

    if(timerid != -1){
        RemoveTimer(timerid);
        timerid = -1;
    }
    return true;
}

bool GRBSystems::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 5;
    FocusSpeedN[0].value = 1;    

    // Maximum Travel
    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 60000., 0., 10000.);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );

    DEBUG(INDI::Logger::DBG_SESSION, "Adding direction properties");
    IUFillSwitch(&PositiveMotionS[0],"NORMAL","Normal",ISS_ON);
    IUFillSwitch(&PositiveMotionS[1],"REVERSE","Reverse",ISS_OFF);
    IUFillSwitchVector(&PositiveMotionSP,PositiveMotionS,2,getDeviceName(),"POSITIVE_MOTION","Direction",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 22500.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 250;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 22500.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 250;

    addDebugControl();

    updatePeriodMS = POLL_MS;

    return true;

}

bool GRBSystems::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&MaxTravelNP);
        defineSwitch(&PositiveMotionSP);

        GetFocusParams();

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "GRBSystems paramaters updated, focuser ready for use.");
    }
    else
    {

        deleteProperty(MaxTravelNP.name);
    }

    return true;

}

bool GRBSystems::Handshake()
{
    if (haveReport)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "GRBSystems is online. Getting focus parameters...");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Error retrieving data from GRBSystems, please ensure GRBSystems controller is powered and the port is correct.");
    return false;
}

const char * GRBSystems::getDefaultName()
{
    return "GRBSystems Focuser";
}

bool GRBSystems::MoveFocuser(unsigned int position)
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

    if(handle == NULL){
        DEBUGF(INDI::Logger::DBG_ERROR, "handle is NULL! This shouldn't happen!", NULL);
        return false;
    }

    if ((PositiveMotionS[0].s == ISS_ON) && (report.direction != 0)){
        // The direction has been flipped so we need to update the prefs to match
        DEBUGF(INDI::Logger::DBG_DEBUG, "Set direction to positive out", NULL);

        UpdateDirection(true);
    } else if((PositiveMotionS[1].s == ISS_ON) && (report.direction == 0)){
        DEBUGF(INDI::Logger::DBG_DEBUG, "Set direction to positive in", NULL);
        UpdateDirection(false);
    }

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
    res = hid_write(handle, buf, BUF_SIZE);
    if(res != BUF_SIZE){
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to write move buffer: %d bytes sent", res);
        return false;
    }

    report.isMoving = true;

    return true;
}

bool GRBSystems::UpdateMaxTravel(unsigned int position) {
    REPORT newRep = report;

    newRep.maximum = position;

    return UpdatePrefs(&newRep);
}

bool GRBSystems::UpdateDirection(bool outPositive) {
    REPORT newRep = report;

    newRep.direction = outPositive ? 0 : 1;

    return UpdatePrefs(&newRep);
}

bool GRBSystems::UpdatePrefs(REPORT *prefs)
{
    // Build out the HID report for a move absolute
    unsigned char buf[BUF_SIZE];

    buf[0] = 0x00;      // Header byte
    buf[1] = 0x2A;      // Update Prefs
    buf[2] = 0x00;      // channel 0

    // Setting Max
    buf[3] = ((prefs->maximum & 0xff00) >> 8);
    buf[4] = (prefs->maximum & 0x00ff);

    // Set pulse
    buf[5] = prefs->pulse;

    // Set Direction
    buf[6] = prefs->direction;

    // Set Backlash
    buf[7] = ((prefs->backlash & 0xff00) >> 8);
    buf[8] = (prefs->backlash & 0x00ff);

    // Set Microns (int scaled by 100)
    buf[9] = ((prefs->microns & 0xff00) >> 8);
    buf[10] = (prefs->microns & 0x00ff);

    int res;
    res = hid_write(handle, buf, BUF_SIZE);
    if(res != BUF_SIZE){
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to write preferences: %d bytes sent", res);
        return false;
    }

    return true;
}


bool GRBSystems::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
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

bool GRBSystems::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
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
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void GRBSystems::GetFocusParams ()
{
    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetSwitch(&PositiveMotionSP,NULL);

}

IPState GRBSystems::MoveAbsFocuser(uint32_t targetTicks)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "MoveAbsFocuser", NULL);

    bool rc;

    rc = MoveFocuser(targetTicks);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState GRBSystems::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
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

void GRBSystems::TimerHit() {

    if (isConnected() == false) {
        timerid = SetTimer(POLL_MS);
        return;
    }

    if (handle == NULL) {
        DEBUGF(INDI::Logger::DBG_ERROR, "isConnected is true, but there is no hid handle!", NULL);
        timerid = SetTimer(POLL_MS);
        return;
    }


//    DEBUGF(INDI::Logger::DBG_DEBUG, "Is Moving: %d\n", report.isMoving);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Position: %d\n", report.position);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Maximum: %d\n", report.maximum);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Pulse: %d\n", report.pulse);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Direction: %d\n", report.direction);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Backlash: %d\n", report.backlash);
//    DEBUGF(INDI::Logger::DBG_DEBUG, "Microns: %d\n", report.microns);

    FocusAbsPosN[0].value = report.position;
    FocusRelPosN[0].value = report.position;

//    DEBUGF(INDI::Logger::DBG_DEBUG, "Resetting Maximum to: %d\n", report.maximum);
    MaxTravelN[0].value = report.maximum;

    FocusAbsPosN[0].max = report.maximum;
    FocusRelPosN[0].max = report.maximum;

    if (report.isMoving || (targetPos != report.position)) {
        FocusRelPosNP.s = IPS_BUSY;
        FocusAbsPosNP.s = IPS_BUSY;
    } else {
        FocusRelPosNP.s = IPS_OK;
        FocusAbsPosNP.s = IPS_OK;
    }

    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusRelPosNP, NULL);
    IDSetNumber(&MaxTravelNP, NULL);

    timerid = SetTimer(POLL_MS);
}

void* GRBSystems::Reader(void *thread_params)
{
    GRBSystems* sys = (GRBSystems*)thread_params;

    sys->DoRead();

    return NULL;
}

#define DATA_OFFSET  4

void GRBSystems::DoRead()
{
    int res;
    unsigned char buf[BUF_SIZE];

    while(keep_running){
        haveReport = false;

        res = hid_read(handle, buf, BUF_SIZE);
        if (res == BUF_SIZE) {
            report.isMoving = (buf[DATA_OFFSET] != 0);
            report.position = buf[DATA_OFFSET + 2] + (buf[DATA_OFFSET + 1] << 8);
            report.maximum = buf[DATA_OFFSET + 4] + (buf[DATA_OFFSET + 3] << 8);
            report.pulse = buf[DATA_OFFSET + 5];
            report.direction = buf[DATA_OFFSET + 6];
            report.backlash = buf[DATA_OFFSET + 8] + (buf[DATA_OFFSET + 7] << 8);
            report.microns = buf[DATA_OFFSET + 10] + (buf[DATA_OFFSET + 9] << 8);

            if(targetPos == -1){
                targetPos = report.position;
            }

            haveReport = true;
        }
    }
}



bool GRBSystems::AbortFocuser()
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "Aborting Move", NULL);

    unsigned char buf[BUF_SIZE];

    buf[0] = 0x00;      // Header byte
    buf[1] = 0x13;      // Stop
    buf[2] = 0x00;      // Channel 0

    int res;
    res = hid_write(handle, buf, BUF_SIZE);
    if(res != BUF_SIZE){
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to stop: %d bytes sent", res);
        return false;
    }

    // Force a position reset
    targetPos = -1;

    return true;
}



