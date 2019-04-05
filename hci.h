/* hci.h -- Hardware Configuration Interface
 *
 * Copyright (c) 1998,1999  Jonathan A. Buzzard (jab@hex.prestel.co.uk)
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 *   This code is covered by the GNU GPL and you are free to make any
 *   changes you wish to it under the terms of the license. However the
 *   code has the potential to render your computer and/or someone else's
 *   unuseable. Unless you truely understand what is going on, I urge you
 *   not to make any modifications and use it as it stands.
 *
 * $Log: hci.h,v $
 * Revision 1.17  2009-06-04 22:03:02  schwitrs
 * added support for internal 3g modem - thanks to Michele Valzelli
 *
 * Revision 1.16  2009-06-04 21:13:12  schwitrs
 * added new fan speed - thanks to the efforts of Rob Rutten
 * updated argument processing to use numeric specifier >9
 *
 * Revision 1.15  2009-02-15 13:38:58  schwitrs
 * added fan modes. The naming should be rationalized.
 *
 * Revision 1.14  2009-01-10 20:39:52  schwitrs
 * added three new resolutions
 *
 * Revision 1.13  2008-03-21 02:00:01  schwitrs
 * added HCI_TR_BACKLIGHT
 *
 * Revision 1.12  2004/09/06 04:00:31  schwitrs
 * added HCI_BUSY entry
 *
 * Revision 1.11  2004/03/06 18:18:19  schwitrs
 * added HCI_LCD_BRIGHTNESS def
 *
 * Revision 1.10  2004/02/28 15:16:07  schwitrs
 * added 1280x600 flat panel resolution
 *
 * Revision 1.9  2003/09/28 00:39:55  schwitrs
 * added HCI_1400_1050
 *
 * Revision 1.8  2003/09/20 22:03:49  schwitrs
 * added experimental fan modes
 *
 * Revision 1.7  2003/01/02 22:37:24  schwitrs
 * typo fix
 *
 * Revision 1.6  2003/01/02 22:31:19  schwitrs
 * LCDFeature::query: added resolution
 *
 * Revision 1.5  2002/11/24 19:46:25  schwitrs
 * mods for kernle interface
 *
 * Revision 1.4  2002/01/23 19:50:26  schwitrs
 * replaced HciRegisters w/ SMMRegisters
 * added HCI_POWER_UP (from toshiba hibernation file docs)
 *
 * Revision 1.3  2002/01/19 23:00:56  schwitrs
 * added HCI_TVOUT
 *
 * Revision 1.2  2001/05/14 16:39:51  schwitrs
 * changed HCI codes for video out mode
 *
 * Revision 1.1  2000/02/03 02:49:03  schwitrs
 * Initial revision
 *
 * Revision 1.1  1999/03/11 20:29:11  jab
 * Initial revision
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef HCI_H
#define HCI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_KERNEL_INTERFACE
#  include<linux/toshiba.h>
#else
#  include "smm.h"
#endif

#ifdef NOTUSED
typedef struct {
	  unsigned long ax;
	  unsigned long bx;
	  unsigned long cx;
	  unsigned long dx;
} HciRegisters;
#endif /*NOTUSED*/

enum {
	HCI_GET             = 0xfeC0,
	HCI_SET             = 0xffC0
};


enum {
	HCI_BACKLIGHT       = 0x0002,
	HCI_AC_ADAPTOR      = 0x0003,
	HCI_FAN             = 0x0004,
	HCI_TR_BACKLIGHT    = 0x0005,
	HCI_SOFTWARE_SUSPEND = 0x0010,
	HCI_FLAT_PANEL       = 0x0011,
	HCI_SELECT_STATUS    = 0x0014,
	HCI_SYSTEM_EVENT     = 0x0016,
	HCI_FIR_STATUS       = 0x001b,
	HCI_VIDEO_OUT        = 0x001c,
	HCI_HOTKEY_EVENT     = 0x001e,
	HCI_UNUSED_MEMORY    = 0x0021,
	HCI_LOCK_STATUS      = 0x0022,
	HCI_BOOT_DEVICE      = 0x0026,
	HCI_OWNERSTRING      = 0x0029,
	HCI_LCD_BRIGHTNESS   = 0x002a,
	HCI_HIBERNATION_INFO = 0x002d,
	HCI_HIBERNATION_LBA  = 0x002e,
	HCI_POWER_UP         = 0x0048,
	HCI_WIRELESS         = 0x0056
};


/*
 * the different states the various modes can be set to
 */

enum {
	HCI_DISABLE         = 0x0000,
	HCI_ENABLE          = 0x0001
};

enum {
	HCI_640_480         = 0x00,
	HCI_800_600         = 0x01,
	HCI_1024_768        = 0x02,
	HCI_1024_600        = 0x03,
	HCI_800_480         = 0x04,
	HCI_1400_1050       = 0x06,
	HCI_1600_1200       = 0x07,
	HCI_1280_600        = 0x08,
	HCI_1280_800        = 0x09,
	HCI_1440_900        = 0x0a,
	HCI_1920_1200       = 0x0c
};

enum {
	HCI_STN_MONO        = 0x00,
	HCI_STN_COLOUR      = 0x01,
	HCI_9BIT_TFT        = 0x02,
	HCI_12BIT_TFT       = 0x03,
	HCI_18BIT_TFT       = 0x04,
	HCI_24BIT_TFT       = 0x05	
};

//enum {
//	  HCI_INTERNAL        = 0x0000,
//	  HCI_EXTERNAL        = 0x0001,
//	  HCI_SIMULTANEOUS    = 0x0002
//};
//enum {
//	    HCI_INTERNAL        = 0x0101,
//	    HCI_EXTERNAL        = 0x0102,
//	    HCI_SIMULTANEOUS    = 0x0103,
//	    HCI_TVOUT           = 0x0104,
//	    HCI_BUSY            = 0x0181,
//};
enum {
	    HCI_INTERNAL        = 0x0101,
	    HCI_EXTERNAL        = 0x0102,
	    HCI_SIMULTANEOUS    = 0x0103,
	    HCI_TVOUT           = 0x0104,
	    //	    HCI_BUSY            = 0x01,
	    HCI_BUSY            = 0x40,
};
enum {
  HCI_FAN_FAN1       = 0x20, // - on?   0010 0000
  HCI_FAN_FAN2       = 0x40,  // - off?  0100 0000
  HCI_FAN_LOW        = 0x0080, //1000 0000
  HCI_FAN_HIGH       = 0x00ff, //1111 1111
  HCI_FAN_LOW2       = 0x0030,    //0011 0000
  HCI_FAN_LOW3       = 0x0090, //1001 0000
  HCI_FAN_LOW4       = 0x18, //0001 0010
  HCI_FAN_HIGH2      = 0x60, //0110 0000
  HCI_FAN_HIGH1      = 0xc0  // - low3? 1100 0000

};

enum {
	HCI_BIOS_SIZE       = 0x0000,
	HCI_MEMORY_SIZE     = 0x0001,
	HCI_VRAM_SIZE       = 0x0002
};

enum {
	HCI_BUILT_IN        = 0x0000,
	HCI_SELECT_INT      = 0x0001,
	HCI_SELECT_DOCK     = 0x0002,
	HCI_5INCH_DOCK      = 0x0003
};

enum {
	HCI_LOCKED          = 0x0000,
	HCI_UNLOCKED        = 0x0001
};

enum {
	HCI_NOTHING         = 0x0000,
	HCI_FLOPPY          = 0x0001,
	HCI_ATAPI           = 0x0002,
	HCI_IDE             = 0x0003,
	HCI_BATTERY         = 0x0004
};

/*
 * HCI error codes
 */
enum {
	HCI_SUCCESS         = 0x00,
	HCI_FAILURE         = 0x01,
	HCI_NOT_SUPPORTED   = 0x80,
	HCI_INPUT_ERROR     = 0x83,
	HCI_WRITE_PROTECTED = 0x84,
	HCI_FIFO_EMPTY      = 0x8c,
	HCI_NOTREADY        = 0xff
};




/*
 * function prototypes
 */
int HciGet(unsigned short mode, unsigned short *status);
int HciSet(unsigned short mode, unsigned short status);
int HciFunction(SMMRegisters *reg);
int HciGetBiosVersion(void);
int HciGetMachineID(int *id);
int HciFnStatus(void);


#ifdef __cplusplus
}
#endif

#endif
