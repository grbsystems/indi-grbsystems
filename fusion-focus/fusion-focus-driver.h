
#include <i2c/smbus.h>


#ifndef __FUSION_FOCUS_DRIVER_H
#define __FUSION_FOCUS_DRIVER_H

typedef unsigned char byte;

typedef struct _focuser {
  unsigned short cur_pos;
  unsigned short set_pos;
  unsigned short max_move;
  unsigned short microns;
  unsigned short backlash;
  byte           dir;
  unsigned short adc1_mean;
  unsigned short adc2_mean;  
  byte           datacount;   
} FOCUSER;


class CFusionFocusDriver
{
public:
    unsigned int GetPosition();
    void SetPosition(unsigned int posn);
    unsigned int GetMove();
    void SetMove(unsigned int move);
    unsigned int GetMax();
    void SetMax(unsigned int max);
    unsigned int GetBacklash();
    void SetBacklash(unsigned int max);
    unsigned int GetMicron();
    void SetMicron(unsigned int micron);
    unsigned int GetDir();
    void SetDir(unsigned int dir);
    void GetSettings(FOCUSER *focus_settings);
    void Abort();
    unsigned int GetSpeed();
    void SetSpeed(unsigned int speed);

    class CFocusException
    {
        public:
            CFocusException(int err);
        
        protected:
            int m_err;
    };

protected:


private:
    void I2CAccess(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data);
    int I2COpen();

    unsigned int I2CGetWord(__u8 command);
    void I2CSetWord(__u8 command, __u16 value);
    unsigned int I2CGetByte(__u8 command);
    void I2CSetByte(__u8 command, __u8 value);
    void I2CGetBuffer(__u8 command, __u8* buffer, int buflen);
};


#endif
