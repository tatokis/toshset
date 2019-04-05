/* sci.c -- System Configuration Interface
 *
 * Copyright (c) 1998/99  Jonathan A. Buzzard (jonathan@buzzard.org.uk)
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 *   This code is covered by the GNU GPL and you are free to make any
 *   changes you wish to it under the terms of the license. However the
 *   code has the potential to render your computer and/or someone else's
 *   unuseable. Unless you truely understand what is going on, I urge you
 *   not to make any modifications and use it as it stands.
 *   
 * $Log: sci.c,v $
 * Revision 1.9  2004-04-30 18:18:36  schwitrs
 * SciSet kernel access fixup
 *
 * Revision 1.8  2004/04/20 20:42:21  schwitrs
 * moved all direct asm calls from sci.c and hci.c to direct.c
 *
 * Revision 1.7  2004/02/28 16:21:05  schwitrs
 * removed unused code
 *
 * Revision 1.6  2002/11/24 19:46:25  schwitrs
 * added optional support for kernel hardware interface
 * required some rearrangement of sci use of SMMRegisters.
 *
 * Revision 1.5  2002/05/18 23:47:59  schwitrs
 * fixes for asm code: now compiles under gcc-3.0
 *
 * Revision 1.4  2002/01/30 18:31:37  schwitrs
 * cleanup
 * added extra element to stack, as done in hciFunction
 * added memory to asm protected area
 *
 * Revision 1.3  2002/01/19 23:00:56  schwitrs
 * work to try to get password stuff working
 *
 * Revision 1.2  2001/05/14 16:39:51  schwitrs
 * copied Jonathan's new code for SciSet. Motivated by passwd stuff.
 *
 * Revision 1.1  2000/02/03 02:49:03  schwitrs
 * Initial revision
 *
 * Revision 1.12  1999/11/17 16:00:42  jab
 * changed assembler to manually save registers, hopefully this should
 * make the programs more stable
 *
 * Revision 1.11  1999/07/25 14:39:49  jab
 * fixed bugs in SciOpenInterface and SciSetPassword
 * updated email address
 *
 * Revision 1.10  1999/03/11 20:22:14  jab
 * changed get and set routines to use SciRegisters
 *
 * Revision 1.9  1999/03/06 16:46:30  jab
 * moved the BiosVersion and MachineID functions to hci.c
 *
 * Revision 1.8  1998/09/07 18:23:36  jab
 * removed SciGetMachineID2 as no longer required
 * added a routine to extract the model string from the BIOS
 *
 * Revision 1.7  1998/09/04 18:14:31  jab
 * fixed the compile warning about rcsid
 *
 * Revision 1.6  1998/08/24 18:05:04  jab
 * implemented the SciGetBiosVersion function
 *
 * Revision 1.5  1998/08/23 12:16:45  jab
 * fixed the SciACPower function so it now works
 *
 * Revision 1.4  1998/08/19 08:45:10  jab
 * changed GetMachineId to return SCI_SUCCESS/SCI_FAILURE
 *
 * Revision 1.3  1998/08/18 16:54:54  jab
 * fixes to SciGetMachineId curtesy of Patrick Reyonlds <patrick@cs.virgina.edu>
 *
 * Revision 1.2  1998/08/06 08:27:14  jab
 * prepended all functions with Sci
 *
 * Revision 1.1  1998/05/23 08:08:53  jab
 * Initial revision
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * 
 */

static const char rcsid[]="$Id: sci.c,v 1.9 2004-04-30 18:18:36 schwitrs Exp $";

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>

#include"sci.h"
#include "kernelInterface.h"
#include "direct.h"


/*
 * Is this a supported Machine? (ie. is it a Toshiba)
 */
int 
SciSupportCheck(int *version)
{

 if ( accessMode==ACCESS_DIRECT )
   return dSciSupportCheck(version);
 else {
   SMMRegisters regs;

   regs.eax = 0xf0f0;
   regs.ebx = 0x0000;
   regs.ecx = 0x0000;
   regs.edx = 0x0000;

   if ( smmAccess(&regs) != 1 ) {
     *version = (int) regs.edx;
     return (int) (regs.eax & 0xff00)>>8;
   }
 }
 return SCI_FAILURE;
} /* SciSupportCheck */


/*
 * Open an interface to the Toshiba hardware.
 *
 *   Note: Set and Get will not work unless an interface has been opened.
 */
int 
SciOpenInterface()
{
 if ( accessMode==ACCESS_DIRECT )
   return dSciOpenInterface();
 else {
   SMMRegisters regs;
   regs.eax = 0xf1f1;
   regs.ebx = 0x0000;
   regs.ecx = 0x0000;

   if ( smmAccess(&regs) != 1 ) {
     return (int) (regs.eax & 0xff00)>>8;
   }
 }
 return SCI_FAILURE;
} /* SciOpenInterface */


/*
 * Close any open interface to the hardware
 */
int 
SciCloseInterface()
{
 if ( accessMode==ACCESS_DIRECT )
   return dSciCloseInterface();
 else {
   SMMRegisters regs;
   regs.eax = 0xf2f2;
   regs.ebx = 0x0000;
   regs.ecx = 0x0000;
   
   if ( smmAccess(&regs) != 1 ) {
     return (int) (regs.eax & 0xff00)>>8;
   }
 }
 return SCI_FAILURE;
} /* SciCloseInterface */


/*
 * Get the setting of a given mode of the laptop
 */

int 
SciGet(SMMRegisters *reg)
{
 if ( accessMode==ACCESS_DIRECT ) 
   return dSciGet(reg);
 else {
   reg->eax = 0xf3f3;

   if ( smmAccess(reg) != 1 ) {
     reg->ebx  &= 0xffff;
     reg->ecx  &= 0xffff;
     reg->edx  &= 0xffff;
     return (int) (reg->eax & 0xff00)>>8;
   }
 }
 return SCI_FAILURE;
} /* SciGet */



/*
 * Set the setting of a given mode of the laptop
 */
int 
SciSet(SMMRegisters *reg)
{
 if ( accessMode==ACCESS_DIRECT )
   return dSciSet(reg);
 else {
   reg->eax = 0xf4f4;
   if ( smmAccess(reg) != 1 ) 
     return (int) (reg->eax & 0xff00)>>8;
 }
 return SCI_FAILURE;
} /* SciSet */


