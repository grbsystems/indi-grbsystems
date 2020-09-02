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

#ifndef FUSION_FOCUS_H
#define FUSION_FOCUS_H

#include "fusion-focus-driver.h"

#include "indifocuser.h"


class FusionFocus : public INDI::Focuser
{
public:
    FusionFocus();
    ~FusionFocus();

    bool Connect();
    bool Disconnect();

    virtual bool Handshake();
    const char * getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();

    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    virtual IPState MoveAbsFocuser(uint32_t ticks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);

    ISwitchVectorProperty PositiveMotionSP; //  A Switch in the client interface for focus_settings+ve direction of motion
    ISwitch PositiveMotionS[2];

    virtual bool AbortFocuser();
    virtual void TimerHit();

private:

    INumber MaxTravelN[1];
    INumberVectorProperty MaxTravelNP;

    INumber SetPositionN[1];
    INumberVectorProperty SetPositionNP;

    int timerid;

    double targetPos, lastPos;

    struct timeval focusMoveStart;
    float focusMoveRequest;

    CFusionFocusDriver *focusDriver;
    FOCUSER focusSettings;

    void GetFocusParams();

    bool MoveFocuser(unsigned int position);

    bool UpdateMaxTravel(unsigned int position);
    bool UpdateCurPos(unsigned int position);
    bool UpdateDirection(bool outPositive);

    static void* Reader(void *thread_params);
    void DoRead();
};

#endif
