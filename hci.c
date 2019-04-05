/* hci.c -- Hardware Configuration Interface
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
 * $Log: hci.c,v $
 * Revision 1.7  2004-04-20 20:42:21  schwitrs
 * moved all direct asm calls from sci.c and hci.c to direct.c
 *
 * Revision 1.6  2002/11/24 19:46:25  schwitrs
 * added optional support for kernel hardware interface
 *
 * Revision 1.5  2002/01/30 18:31:37  schwitrs
 * cleanup
 *
 * Revision 1.4  2002/01/29 01:13:26  schwitrs
 * removed segfault signal hack
 * replaced with extra element on stack hack
 *
 * Revision 1.3  2002/01/23 19:47:53  schwitrs
 * HciFunction: now set all SSM registers
 * involves major hack to get around segfault in asm: catch signal and restart.
 *
 * Revision 1.2  2001/05/14 16:39:51  schwitrs
 * redid HciGetMachineID- stuff from Jonathan's kernel module
 *
 * Revision 1.1  2000/02/03 02:49:03  schwitrs
 * Initial revision
 *
 * Revision 1.2  1999/08/15 10:43:28  jab
 * removed the HciGet and HciSet and replaced with HciFunction
 *
 * Revision 1.1  1999/03/11 20:27:06  jab
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

static const char rcsid[]="$Id: hci.c,v 1.7 2004-04-20 20:42:21 schwitrs Exp $";

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/ioctl.h>

#include <setjmp.h>
#include <signal.h>

#include "hci.h"
#include "kernelInterface.h"
#include "direct.h"

static int id=0xfc2f;

int HciFunction(SMMRegisters *reg)
{
 if ( accessMode==ACCESS_DIRECT ) 
   return dHciFunction(reg);
 else
   return smmAccess(reg);
}


int 
HciGetBiosVersion()
{
 id = 0;
 if ( accessMode==ACCESS_DIRECT ) {
   id = dHciGetBiosVersion();
 } else {
   ToshProcInfo procInfo;
   if ( !procAccess(&procInfo) )
     return HCI_FAILURE;
   id = (procInfo.major*0x100)+procInfo.minor;
 }

 return id;
} /* HciGetBiosVersion */


int 
HciGetMachineID(int *id)
{
 if ( accessMode==ACCESS_DIRECT ) {
   return dHciGetMachineID(id);
 } else {
   ToshProcInfo procInfo;
   if ( procAccess(&procInfo) )
     *id = procInfo.id;
   else 
     return HCI_FAILURE;
 }
 return HCI_SUCCESS;
} /* HciGetMachineID */

