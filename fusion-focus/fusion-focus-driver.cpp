#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port
#include <linux/i2c.h>
#include <errno.h>

#include "fusion-focus-driver.h"

#define ADDRESS 			0x08

#define FOCUS_GET_POS     	0X04
#define FOCUS_SET_POS     	0x05
#define FOCUS_GET_MOVE    	0x06
#define FOCUS_SET_MOVE    	0x07
#define FOCUS_SET_STOP    	0x08
#define FOCUS_GET_MAX     	0x09
#define FOCUS_SET_MAX     	0x0A
#define FOCUS_GET_MICRON  	0x0B
#define FOCUS_SET_MICRON  	0x0C
#define FOCUS_GET_DIR     	0x0D
#define FOCUS_SET_DIR     	0x0E
#define FOCUS_GET_SETTINGS 	0x10

//#define FLIP_BITS(A)	(((A & 0xFF00) >> 8 ) + ((A & 0x00FF) << 8 ))
#define FLIP_BITS(A)	(A)

CFusionFocusDriver::CFocusException::CFocusException(int err)
{
	m_err = err;
}

void CFusionFocusDriver::I2CAccess(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;
	__s32 err;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

	err = ioctl(file, I2C_SMBUS, &args);
	if (err == -1)
	{
		throw CFocusException(100);
	}

}

int CFusionFocusDriver::I2COpen()
{
	char *filename = (char*)"/dev/i2c-1";
	int file_i2c;

	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		throw CFocusException(200);
	}

    if (ioctl(file_i2c, I2C_SLAVE, ADDRESS) < 0) {
		throw CFocusException(250);
	}

	return file_i2c;
}

int CFusionFocusDriver::I2CGetWord(__u8 command)
{
	int file = I2COpen();
	if(file > 0)
	{
    	union i2c_smbus_data data;
		I2CAccess(file, I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA, &data);
		
		int result =  0xFFFF & data.word;
    	// Need to swap MSB & LSB

		close(file);

		return FLIP_BITS(result);
	}
	else
	{
		throw CFocusException(350);
	}
}

void CFusionFocusDriver::I2CSetWord(__u8 command, __u16 value)
{
	int file = I2COpen();
	if(file > 0)
	{
		union i2c_smbus_data data;
		data.word = value;
		I2CAccess(file, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, &data);
		close(file);
	}
	else
	{
		throw CFocusException(360);
	}
}

int CFusionFocusDriver::I2CGetByte(__u8 command)
{
	int file = I2COpen();
	if(file > 0)
	{
    	union i2c_smbus_data data;

		I2CAccess(file, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data);
		
		int result =  0x00FF & data.byte;
		close(file);

		return result;
	}
	else
	{
		throw CFocusException(370);
	}
}

void CFusionFocusDriver::I2CSetByte(__u8 command, __u8 value)
{
	int file = I2COpen();
	if(file > 0)
	{
		union i2c_smbus_data data;
		data.byte = (__u8)value;
		I2CAccess(file, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
		close(file);
	}
	else
	{
		throw CFocusException(380);
	}
}

void CFusionFocusDriver::I2CGetBuffer(__u8 command, __u8* buffer, int buflen)
{
	int file = I2COpen();
	if(file > 0)
	{
		struct i2c_msg msgs[2];

		msgs[0].addr = ADDRESS;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &command;
		msgs[1].addr = ADDRESS;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = buflen;
		msgs[1].buf = buffer;

		int nmsgs = 2;

		struct i2c_rdwr_ioctl_data rdwr;
		rdwr.msgs = msgs;
		rdwr.nmsgs = nmsgs;

		int nmsgs_sent = ioctl(file, I2C_RDWR, &rdwr);
		if (nmsgs_sent < 0) {
			throw CFocusException(500);
		} 

		close(file);
	}
	else
	{
		throw CFocusException(380);
	}
}


int CFusionFocusDriver::GetPosition()
{
	return I2CGetWord(FOCUS_GET_POS);
}

void CFusionFocusDriver::SetPosition(int posn)
{
	I2CSetWord(FOCUS_SET_POS, FLIP_BITS(posn));
}

int CFusionFocusDriver::GetMove()
{
	return I2CGetWord(FOCUS_GET_MOVE);
}

void CFusionFocusDriver::SetMove(int move)
{
	I2CSetWord(FOCUS_SET_MOVE, FLIP_BITS(move));
}

int CFusionFocusDriver::GetMax()
{
	return I2CGetWord(FOCUS_GET_MAX);
}

void CFusionFocusDriver::SetMax(int max)
{
	I2CSetWord(FOCUS_SET_MAX, FLIP_BITS(max));
}

int CFusionFocusDriver::GetMicron()
{
	return I2CGetWord(FOCUS_GET_MICRON);
}

void CFusionFocusDriver::SetMicron(int microns)
{
	I2CSetWord(FOCUS_SET_MICRON, FLIP_BITS(microns));
}

int CFusionFocusDriver::GetDir()
{
	return I2CGetByte(FOCUS_GET_DIR);
}

void CFusionFocusDriver::SetDir(int dir)
{
	I2CSetByte(FOCUS_SET_DIR, dir);
}

void CFusionFocusDriver::GetSettings(FOCUSER *focus_settings)
{
	I2CGetBuffer(FOCUS_GET_SETTINGS, (__u8*)focus_settings, sizeof(FOCUSER) );
}
