
#include <i2c/smbus.h>


#ifndef __FUSION_FOCUS_DRIVER_H
#define __FUSION_FOCUS_DRIVER_H

typedef struct _focuser {
  unsigned short cur_pos;
  unsigned short set_pos;
  unsigned short max_move;
  unsigned short microns;
  unsigned char  dir;
  unsigned char  datacount;
} FOCUSER;


class CFusionFocusDriver
{
public:
    int GetPosition();
    void SetPosition(int posn);
    int GetMove();
    void SetMove(int move);
    int GetMax();
    void SetMax(int max);
    int GetMicron();
    void SetMicron(int micron);
    int GetDir();
    void SetDir(int dir);
    void GetSettings(FOCUSER *focus_settings);

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

    int I2CGetWord(__u8 command);
    void I2CSetWord(__u8 command, __u16 value);
    int I2CGetByte(__u8 command);
    void I2CSetByte(__u8 command, __u8 value);
    void I2CGetBuffer(__u8 command, __u8* buffer, int buflen);
};


#endif