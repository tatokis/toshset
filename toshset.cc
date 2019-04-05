/* toshset.cc
 *
 * Copyright (C) 1999, 2000, 2001, 2002 Charles D. Schwieters
 *
 * Based on tools by Jonathan Buzzard
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

static char const rcsid[]="$Id: toshset.cc,v 1.51 2010-02-12 16:30:51 schwitrs Exp $";
static char const rcsversion[]="$State: Exp $";


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<sys/wait.h>
#include <ctype.h>
#include<errno.h>
#include<signal.h>
#include<paths.h>
#include<pwd.h>
#include <termios.h>
#include<sys/ioctl.h>
#ifdef LINUX
#  include<features.h>
#  include<sys/vt.h>
#  include<sys/kd.h>
#else /* LINUX */
#  include <term.h>
#endif
#include <iostream>
#include <streambuf>
#include <iomanip>
#include "cdsList.hh"
#include "cdsString.hh"
#include "cdsSStream.hh"
#include "cdsExcept.hh"
#ifdef __GLIBC__
#include<sys/perm.h>
#endif

#include "kernelInterface.h"
#include "sci.h"
#include "hci.h"
#include "toshibaIDs.hh"
#include "wildmat.h"

using namespace std;

enum { EMPTY, PRIMARY, AUXILLARY };

static int id;

static char versionString[80];
static int  verbose=0;
static int  longQuery=0;
static int  fast=0;




/*
 * Convert a single character to an interger, similar to atoi 
 */
inline int ctoi(char *s)
{
	return ((*s>='0') && (*s<='9')) ? *s-'0' : -1;
}


struct ValueSet {
  const char* iString;
  unsigned short sciCode;
  const char* oString;
  ValueSet(const char*          input,
		 unsigned short sciCode,
	   const char*          output) :
    iString(input), sciCode(sciCode), oString(output) {}
};
 
struct Feature {
  const char* name;
  Feature(const char* name) : name(name) {}
  virtual ~Feature() {}
  // the next two return 0 on success, otherwise return an error code
  // which is interpreted using error()
  virtual int action(const char**) const=0; 
  virtual int query(OStringStream &os) const=0;
  virtual const char* error(int) const=0;
};

struct ToggleFeature : public Feature {
  int&        toggleVar;
  //  const char* name;
  ToggleFeature(      int&  toggleVar,
		const char* name) :
    Feature(name), toggleVar(toggleVar) {}
  virtual ~ToggleFeature() {}
  virtual int action(const char**) const 
    { toggleVar = (toggleVar?0:1); return 1;}
  virtual int query(OStringStream &os) const {return 1;}
  virtual const char* error(int) const {return "";}
};

struct VersionFeature : public Feature {
  VersionFeature() :
    Feature("toshset version") {}
  virtual ~VersionFeature() {}
  virtual int action(const char**) const {return 1;}
  virtual int query(OStringStream &os) const {
   os << "toshset version: " << VERSION << ends; return 0;}
  virtual const char* error(int) const {return "";}
};

struct AccessFeature : public Feature {
  AccessFeature() :
    Feature("HCI/SCI access") {}
  virtual ~AccessFeature() {}
  virtual int action(const char**) const {return 1;}
  virtual int query(OStringStream &os) const {
   os << "HCI/SCI access mode: " 
      << (accessMode==ACCESS_DIRECT?"direct":"kernel") << ends; return 0;}
  virtual const char* error(int) const {return "";}
};

struct ModelFeature : public Feature {
  int id;
  ModelFeature(int id) :
    Feature("hardware model"), id(id) {}
  virtual ~ModelFeature() {}
  virtual int action(const char**) const {return 1;}
  virtual int query(OStringStream &os) const {
   os << "Toshiba Model: " << toshibaModelName(id) << ends; return 0;}
  virtual const char* error(int) const {return "";}
};


struct SciFeature : public Feature {
  unsigned short sciMode;
  //  const char* name;
  SciFeature(unsigned short sciMode,const char* name) :
    Feature(name), sciMode(sciMode) {}
  CDSList<ValueSet*> values;
  void addValue(const char* input,
		      unsigned short sciCode,
		const char* output) 
    { values.append(new ValueSet(input,sciCode,output)); }
  virtual ~SciFeature() 
    { for (int i=0 ; i<values.size() ; i++) delete values[i];}
  virtual int action(const char**) const;
  virtual int query(OStringStream &os) const;
  const char*  error(int code) const;
};

const char*
SciFeature::error(int code) const
{
 const char* ret;
 static char defret[80];
 snprintf(defret,80,"%s (%d)","unknown error.",code);
 ret = defret;
 switch ( code ) {
   case SCI_FAILURE         : ret =  "FAILURE"         ; break;
   case SCI_NOT_SUPPORTED   : ret =  "NOT_SUPPORTED"   ; break;
   case SCI_ALREADY_OPEN    : ret =  "ALREADY_OPEN"    ; break;
   case SCI_NOT_OPENED      : ret =  "NOT_OPENED"      ; break;
   case SCI_INPUT_ERROR     : ret =  "INPUT_ERROR"     ; break;
   case SCI_WRITE_PROTECTED : ret =  "WRITE_PROTECTED" ; break;
   case SCI_NOT_PRESENT     : ret =  "NOT_PRESENT"     ; break;
   case SCI_NOT_READY       : ret =  "NOT_READY"       ; break;
   case SCI_DEVICE_ERROR    : ret =  "DEVICE_ERROR"    ; break;
   case SCI_NOT_INSTALLED   : ret =  "NOT_INSTALLED"   ; break;
 }
 return ret;
} /* SciFeature::error */


int
SciFeature::action(const char **s) const
{
 SMMRegisters reg;
 reg.ebx = sciMode;
 if ( ! **s) {
   cerr << "SCI error: argument required\n";
   return 0;
 }
 int ret = -1;
 for (int i=0 ; i<values.size() ; i++)
   if ( strcmp( values[i]->iString , *s ) == 0   ) {
     reg.ecx =values[i]->sciCode;
     reg.edx = reg.esi = reg.edi = 0;
     ret = SciSet( &reg );
     if ( ret == SCI_SUCCESS ) 
       return 1;
   }
 // try setting indexed setting of option.
 if ( ret<0 ) {
   errno=0;
   int i = strtol(*s,(char **)NULL,10);
   if ( errno==0 && i>=0 && i<values.size() ) {
     reg.ecx =values[i]->sciCode;
     reg.edx = reg.esi = reg.edi = 0;
     ret = SciSet( &reg );
     if ( ret == SCI_SUCCESS ) 
       return 1;
   }
 }
 if ( ret>=0 ) {
   cerr << "SciFeature:action: error setting " << name << '\n';
   cerr << "\tSciSet returned: " << error(ret) <<'\n';
   return 0;
 }
 cerr << "SCIFeature: error when setting feature " << name 
      << ":\n\terror in command-line option: " << *s << "\n";
 cerr << "valid settings are:\n";
 for (int i=0 ; i<values.size() ; i++)
   cerr << "\t(" << i << ") " << values[i]->iString << '\n';
 return 0;
} /* SciFeature::action */

int
SciFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.ebx = sciMode;
 int ret = SciGet( &reg );
 if ( ret == SCI_SUCCESS ) {
   for (int i=0 ; i<values.size() ; i++)
     if ( reg.ecx == values[i]->sciCode ) {
       os << name << ": " << values[i]->oString << ends;
       return 0;
     }
   cerr << "SciFeature::query: received an unexpected response for feature " 
	<< name << ": " << reg.ecx << '\n';
 } 
//else 
//   cerr << "SciFeature:query: error when querying feature " << name << '\n'
//	  << "\tSciGet returned " << error(ret) << '\n';
 return ret;
} /* SciFeature::action */

struct TimeFeature : public SciFeature {
  TimeFeature(unsigned short sciMode,const char* name) :
    SciFeature(sciMode,name) {}
  virtual ~TimeFeature() 
    { for (int i=0 ; i<values.size() ; i++) delete values[i];}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
TimeFeature::action(const char **s) const
{
 if (!**s) {
   cout << "<dis|HH:MM[/everyday|DD/MM[/YYYY]]> time/date to wake\n";
   return 0;
 }
 SMMRegisters reg;
 enum {
   ALARM_TIME  = 0x0001,
   ALARM_DATE  = 0x0002,
   ALARM_EVERY = 0x0004,
   ALARM_YEAR  = 0x0008
 };
 int support = 0x0000;
 reg.ebx = SCI_ALARM_TIME;
 if (SciGet(&reg)==SCI_SUCCESS) {
   support |= ALARM_TIME;
 }

 reg.ebx = SCI_ALARM_DATE;
 if ( SciGet(&reg) == SCI_SUCCESS ) {
   reg.edx &= 0xffff;
   support |= ALARM_DATE;
   if ( SCI_DATE_EVERYDAY(reg.edx) )
     support |= ALARM_EVERY;
   if (SCI_YEAR(reg.edx)!=1990)
     support |= ALARM_YEAR;
 }
 if ( verbose ) 
   cout << "TimeFeature: supported date elements: " 
	<< (support&ALARM_DATE?"month/day ":"")
	<< (support&ALARM_YEAR?"year ":"")
	<< (support&ALARM_EVERY?"everyday\n":"\n");

 reg.ebx = sciMode;
 int ret=0;
 if ( strncmp(*s,"dis",3) == 0 ) {
   reg.ecx = SCI_ALARM_DISABLED;
   ret = SciSet( &reg );
 } else {
   IStringStream istr(*s);
   char delim; 
   char hour[4];
   istr.get(hour,4,':'); istr.get( delim );
   char minute[4];
   istr.get(minute,4,'/'); istr.get( delim );
   reg.ecx = SCI_TIME( atoi(hour) , atoi(minute) );
   //cout << "setting time\n";
   ret = SciSet( &reg );
   if ( delim == '/' && ret == SCI_SUCCESS && support&ALARM_DATE ) {
     reg.ebx = SCI_ALARM_DATE;
     //reg.ebx = sciMode;
     reg.ecx = 0;
     reg.edx = 0;
     delim = '\0';
     char day[9];
     char month[3];
     char year[9]; year[0]='\0';
     istr.get(day,9,'/'); istr.get( delim );
     if ( delim=='\0' && strcmp(day,"everyday")==0 ) {
       if (support&ALARM_EVERY) 
	 reg.ecx = 1;
       else 
	 cerr << "TimeFeature::action: everyday not supported\n";
     } else {
       delim = '\0';
       istr.get(month,3,'/'); istr.get( delim );
       if ( delim=='/' && support&ALARM_YEAR) {
	 istr.get(year,9,'/');
	 reg.ecx = SCI_FULLDATE( atoi(year) , atoi(month) , atoi(day) );
       } else 
	 reg.ecx = SCI_DATE( atoi(month) , atoi(day) );
     }
     
     //cout << "setting date to " << day << '/' << month << '/' << year << '\n';
     if (reg.ecx) ret=SciSet( &reg );
     if ( ret != SCI_SUCCESS ) {
       cerr << "TimeFeature:action: error when setting date for feature " 
	    << name << '\n'
	    << "\tSciSet returned " << error(ret) << '\n';
       return 0;
     }
   }
 }
 if ( ret == SCI_SUCCESS )
   return 1;
 else 
   cerr << "TimeFeature:action: error when setting feature " << name << '\n'
	<< "\tSciSet returned " << error(ret) << '\n';
 return 0;
} /* TimeFeature::action */

int
TimeFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.ebx = sciMode;
 int ret = SciGet( &reg );
 if ( ret == SCI_SUCCESS ) {
   os << name << ": ";
   if ( reg.ecx == SCI_ALARM_DISABLED ) 
     os << "disabled";
   else {
     os << setfill('0') << setw(2) << SCI_HOUR(reg.ecx) << ':' 
	<< setfill('0') << setw(2) << SCI_MINUTE(reg.ecx) << ' ';
     reg.ebx = SCI_ALARM_DATE;
     SciGet( &reg );
     int year = SCI_YEAR(reg.ecx);
     if ( SCI_DATE_EVERYDAY(reg.ecx) )
       os << "everyday";
     else {
       os << SCI_DAY(reg.ecx) << '/'
	  << SCI_MONTH(reg.ecx);
       if ( year !=1990 ) 
	 os << year << '/';
     }
   }
   os << ends;
   return 0;
 } 
 return ret;
//else 
//   cerr << "TimeFeature:query: error when querying feature " << name << '\n'
//	  << "\tSciGet returned " << error(ret) << '\n';
// return 0;
} /* SciFeature::action */

struct PasswdFeature : public SciFeature {
  unsigned short passwdType;
  PasswdFeature(unsigned short passwdType,const char* name) :
    SciFeature(SCI_PASSWORD,name), passwdType(passwdType) {}
  virtual ~PasswdFeature() {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

/*
 * Put terminal into raw mode and then get a single keypress
 */
int GetScanCode()
{
 struct termio new_key;
 static struct termio saved;
 unsigned char key[1];
 int fd = open("/dev/tty", O_RDWR);

 /* exit if unable to open the console */
 if ( fd<0 ) {
   cerr <<"unable to open console\n";
   throw CDS::exception("unable to open console\n");
 }
 //terminal = 1;
 
 /* get the current terminal state */
 
 int keyboard;
 ioctl(fd, KDGKBMODE, &keyboard);
 ioctl(fd, TCGETA, &saved);
 ioctl(fd, TCGETA, &new_key);
 
 /* set appropriate terminal mode */
 
 new_key.c_lflag &= ~ (ICANON | ECHO | ISIG); 
 new_key.c_iflag = 0;
 new_key.c_cc[VMIN] = 1;
 new_key.c_cc[VTIME] = 1;
 
 ioctl(fd, TCSETAW, &new_key);
 ioctl(fd, KDSKBMODE, K_RAW);
 
 key[0] = 0;
 read(fd, key, 1);
 
 ioctl(fd, TCSETAW, &saved);
 ioctl(fd, KDSKBMODE, keyboard);
 keyboard = -1;

 /* close the connection to the terminal  */
 close(fd);
 //	terminal = 0;

 return (int) key[0];
}

/*
 * Get a password from the user echoing astrix's
 */
void GetPassword(char *password)
{
 /* blank the password */
 for(int loop=0 ; loop<10 ; loop++)
   *(password+loop) = 0;

 fflush(stdout);

 /* now get the password from the user*/
 for (int loop=0;;) {
   int scan = GetScanCode();
   if ((scan<0x02) || (scan>0x39)) continue;
   if ((scan==0x1d) || (scan==0x2a) || (scan==0x36)
       || (scan==0x37) || (scan==0x38)) continue;
   if (scan==0x0f) continue;
   if (scan==0x1c) {
     cout << endl;
     break;
   }
   if (scan==0x0e) {
     if (loop>0) {
       cout << "\b \b";
       fflush(stdout);
       loop--;
       *(password+loop) = 0;
     }
     continue;
   }
   if (loop<10) {
     *(password+loop) = scan;
     cout << '*';
     cout.flush();
     loop++;
   }
 }
 
 return;
}

static int
setPasswd(char*          password,
	  unsigned short passwdType)
{
 SMMRegisters reg;

 reg.eax = 0xf4f4;
 reg.ebx = SCI_PASSWORD;
 reg.ecx = passwdType;
 reg.edx = *(password+3) + *(password+2)*0x100 + *(password+1)*0x10000 +
	   *(password)*0x1000000;
 reg.esi = *(password+7) + *(password+6)*0x100 + *(password+5)*0x10000 +
	   *(password+4)*0x1000000;
 reg.edi = *(password+9) + *(password+8)*0x100;

 int ret = SciSet( &reg );

 int trys = (int) (reg.edx & 0xffff);
 if ( trys <1 )
   cerr << "setPasswd: maximum password trys exceeded.\n"
	<< "\tplease reboot or suspend/resume and try again.\n";

 return ret;
} /* setPasswd */


int
PasswdFeature::action(const char **s) const
{
 int fd = open("/dev/tty", O_RDWR);
 if (fd<0) {
   cerr << "PasswdFeature: unable to open console.\n";
   return 0;
 }
 struct vt_mode vtm;
 if (ioctl(fd, VT_GETMODE, &vtm)!=0) {
   close(fd);
   cerr << "PasswdFeature: must be run from the console.\n";
   return 0;
 }
 close(fd);
 
 SMMRegisters reg;
 reg.ebx = sciMode;
 reg.ecx = passwdType;
 int ret = SciGet( &reg );
 if ( ret != SCI_SUCCESS ) 
   return ret;
 
 if ( reg.ecx == 0 ) {
   // not registered
   char password1[11];
   cout << "enter new password:";
   GetPassword(password1);
   char password2[11];
   cout << "reenter new password:";
   GetPassword(password2);
   if ( String(password1) != password2 ) {
     cerr << "passwords do not match...\n";
     return 1;
   }
   ret = setPasswd(password1,passwdType);
 } else if (reg.ecx == 1) {
   //registered
   char password1[11];
   cout << "enter current password to disable:";
   GetPassword(password1);
   ret = setPasswd(password1,passwdType);
 } else
   ret = SCI_FAILURE;

 if ( ret == SCI_SUCCESS )
   return 1;
 else 
   cerr << "PasswdFeature:action: error when setting feature " << name << '\n'
	  << "\tSciSetPasswd returned " << error(ret) << '\n';
 return 0;
} /* PasswdFeature::action */

int
PasswdFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.ebx = sciMode;
 reg.ecx = passwdType;
 int ret = SciGet( &reg );
 if ( ret == SCI_SUCCESS ) {
   os << name << ": ";
   switch ( reg.ecx ) {
     case 0 : os << "not registered"; break;
     case 1 : os << "registered"; break;
     default : os << "unexpected response: " << reg.ebx;
   }
   os << ends;
   return 0;
 } 
//else 
//   cerr << "PasswdFeature:query: error when querying feature " << name << '\n'
//	  << "\tSciGet returned " << error(ret) << '\n';
// return 0;
 return ret;
} /* PasswdFeature::query */

struct PercentFeature : public SciFeature {
  PercentFeature(unsigned short sciMode,const char* name) :
    SciFeature(sciMode,name) {}
  ~PercentFeature() {}
  int action(const char**) const {return 1;}
  int query(OStringStream& os) const;
};

int
PercentFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.ebx = sciMode;
 int ret = SciGet( &reg );
 if ( ret == SCI_SUCCESS ) {
   int percent = ((100*reg.ecx)/reg.edx);
   os << name << ": " << percent << "\% " << ends;
 }
 return ret;
} /* Feature::action */


struct HciFeature : public Feature {
  unsigned short hciMode;
  HciFeature(unsigned short hciMode,const char* name) :
    Feature(name), hciMode(hciMode) {}
  CDSList<ValueSet*> values;
  void addValue(const char* input,
		      unsigned short hciCode,
		const char* output) 
    { values.append(new ValueSet(input,hciCode,output)); }
  virtual ~HciFeature() 
    { for (int i=0 ; i<values.size() ; i++) delete values[i];}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
  virtual const char* error(int) const;
};

const char*
HciFeature::error(int code) const
{
 const char* ret;
 static char defret[80];
 snprintf(defret,80,"%s (%d)","unknown error.",code);
 ret = defret;
 switch (code) {
   case HCI_FAILURE         : ret = "FAILURE"         ; break;
   case HCI_NOT_SUPPORTED   : ret = "NOT_SUPPORTED"   ; break;
   case HCI_INPUT_ERROR     : ret = "INPUT_ERROR"     ; break;
   case HCI_WRITE_PROTECTED : ret = "WRITE_PROTECTED" ; break;
   case HCI_FIFO_EMPTY      : ret = "FIFO_EMPTY"      ; break;
   case HCI_NOTREADY        : ret = "NOTREADY"        ; break;
 }
 return ret;
} /* HciFeature::error */

int
HciFeature::action(const char **s) const
{
 SMMRegisters reg;
 reg.eax = HCI_SET;
 reg.ebx = hciMode;
 reg.edx = 0;
 if ( ! **s) {
   cerr << "HCI error: argument required\n";
   return 0;
 }
 int ret = -1;
 for (int i=0 ; i<values.size() ; i++)
   if ( strcmp( values[i]->iString , *s ) == 0   ) {
     reg.ecx =values[i]->sciCode;
     ret = HciFunction( &reg );
     if ( ret == HCI_SUCCESS ) 
       return 1;
   }
 // try setting indexed setting of option.
 int i = atoi(*s);
 if ( ret<0 && isdigit(**s) && i>=0 && i<values.size() ) {
   reg.ecx =values[i]->sciCode;
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) {
     //sleep(1);
     return 1;
   }
 }
 if ( ret>=0 ) {
   cerr << "HCI error setting " << name << '\n';
   cerr << "\tHciFunction returned: " << error(ret) <<'\n';
   return 0;
 }
 cerr << "HCI error setting " << name << '\n';
 cerr << "valid settings are:\n";
 for (int i=0 ; i<values.size() ; i++)
   cerr << "\t(" << i << ") " << values[i]->iString << '\n';
 return 0;
} /* Feature::action */

int
HciFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 int retries=3;
 while ( --retries ) {
   ret = HciFunction( &reg );
   if ( ret != HCI_BUSY ) 
     break;
 }
   
 // reg.ecx &= ~0x0080;  //this is some sort of status bit
 if ( ret == HCI_SUCCESS ) {
   for (int i=0 ; i<values.size() ; i++)
     if ( reg.ecx == values[i]->sciCode ) {
       os << name << ": " << values[i]->oString << ends;
       return 0;
     }
   cerr << "HciFeature::query: received an unexpected response for feature " 
	<< name << ": " << reg.ecx << '\n';
 }
 return ret;
} /* HciFeature::query */

// are these really constant??
const int HCI_LCD_BRIGHTNESS_BITS   =		3;
const int HCI_LCD_BRIGHTNESS_SHIFT  =	(16-HCI_LCD_BRIGHTNESS_BITS);
const int HCI_LCD_BRIGHTNESS_LEVELS =	(1 << HCI_LCD_BRIGHTNESS_BITS);

struct LCDIntensityFeature : public HciFeature { 

  LCDIntensityFeature(const char* name) :
    HciFeature(HCI_LCD_BRIGHTNESS,name) {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
LCDIntensityFeature::action(const char **s) const
{
 SMMRegisters reg;
 reg.eax = HCI_SET;
 reg.ebx = hciMode;
 reg.edx = 0;
 if ( ! **s) {
   cerr << "HCI error: argument required\n";
   return 0;
 }
 int ret = -1;
 int val = atoi(*s);

 if (val >=0 && val < HCI_LCD_BRIGHTNESS_LEVELS) {
   reg.ecx =val << HCI_LCD_BRIGHTNESS_SHIFT;
   ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) 
     return 1;
 }
 if ( ret>=0 ) {
   cerr << "HCI error setting " << name << '\n';
   cerr << "\tHciFunction returned: " << error(ret) <<'\n';
   return 0;
 }
 cerr << "HCI error setting " << name << '\n';
 cerr << "valid settings are: 0-" << (HCI_LCD_BRIGHTNESS_LEVELS-1) << '\n';
 return 0;
} /* LCDIntensityFeature::action */

int
LCDIntensityFeature::query(OStringStream& os) const
{

 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 // reg.ecx &= ~0x0080;  //this is some sort of status bit
 if ( ret == HCI_SUCCESS ) {
   int value = reg.ecx >> HCI_LCD_BRIGHTNESS_SHIFT;
   os << name << ": " << value << "/" << (HCI_LCD_BRIGHTNESS_LEVELS-1) << ends;
   return 0;
 }
 return ret;
} /* LCDIntensityFeature::query */

struct WirelessFeature : public HciFeature {
  int mode;
  WirelessFeature(int mode,
		  const char* name) :
    HciFeature(HCI_WIRELESS,name), mode(mode) {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
WirelessFeature::action(const char **s) const
{
 SMMRegisters reg;
 reg.eax = HCI_SET;
 reg.ebx = hciMode;
 reg.edx = mode;
 if ( ! **s) {
   cerr << "HCI error: argument required\n";
   return 0;
 }
 int ret = -1;
 for (int i=0 ; i<values.size() ; i++)
   if ( strcmp( values[i]->iString , *s ) == 0   ) {
     reg.ecx =values[i]->sciCode;
     ret = HciFunction( &reg );
     if ( ret == HCI_SUCCESS ) 
       return 1;
   }
 // try setting indexed setting of option.
 int i = atoi(*s);
 if ( ret<0 && strlen(*s)==1 && isdigit(**s) && i>=0 && i<values.size() ) {
   reg.ecx =values[i]->sciCode;
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) {
     //sleep(1);
     return 1;
   }
 }
 if ( ret>=0 ) {
   cerr << "HCI error setting " << name << '\n';
   cerr << "\tHciFunction returned: " << error(ret) <<'\n';
   return 0;
 }
 cerr << "HCI error setting " << name << '\n';
 cerr << "valid settings are:\n";
 for (int i=0 ; i<values.size() ; i++)
   cerr << "\t(" << i << ") " << values[i]->iString << '\n';
 return 0;
} /* WirelessFeature::action */

int
WirelessFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 1;
 reg.edx = mode;
 int ret=0;
 ret = HciFunction( &reg );
// cerr << "cx: " << reg.ecx <<endl;
// cerr << "dx: " << reg.edx <<endl;
 if (  ret!=0 )  return ret;
 
 if ( ret != HCI_SUCCESS )
   return ret;

 os << name << ": ";
 if (!(reg.ecx & 0x0f))
   os << "unavailable";
 else
   os << (reg.ecx&1 ? "on":"off");
 os << ends;
 return ret;

} /* WirelessFeature::query */

struct BlueToothFeature : public HciFeature {
  BlueToothFeature(const char* name) :
    HciFeature(HCI_WIRELESS,name) {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
BlueToothFeature::action(const char** c) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 if (  ret!=0 ) {
   //   cerr << "error querying bluetooth\n";
   return ret;
 }
 if (!(reg.ecx & 0x0f)) {
   cerr << "Bluetooth unavailable\n";
   return ret;
 }
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 1;
 ret=0;
 ret = HciFunction( &reg );
 if ( ret!=0 ) {
   cerr << "error checking Bluetooth switch status\n";
   return ret;
 }
 if(!(reg.ecx & 0x1)) {
   cerr << "wireless switch is off\n";
   return ret;
 }

 if ( (String(*c) == "on") ||
      (String(*c) == "1")    ) { // turn on

   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 1;
   reg.edx = 0x80;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error activating Bluetooth device\n";
     return ret;
   }
   //cerr << "wireless switch is activated\n";

   // HciFunction( 0 );

   sleep(1);

   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 1;
   reg.edx = 0x40;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error attaching Bluetooth device\n";
     return ret;
   }
   //cerr << "wireless switch is attached\n";
 }
 if ( (String(*c) == "off") ||
      (String(*c) == "0") )  { // turn off
   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 0;
   reg.edx = 0x40;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error detaching Bluetooth device\n";
     return ret;
   }

   sleep(1);

   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 0;
   reg.edx = 0x80;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error deactivating Bluetooth device\n";
     return ret;
   }
   //cerr << "wireless switch is activated\n";

   // HciFunction( 0 );


   //cerr << "wireless switch is attached\n";
   return ret;
 }
 return ret;
} /* BlueToothFeature::action */

int
BlueToothFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 if (  ret!=0 ) {
   //   cerr << "error querying bluetooth\n";
   return ret;
 }
 if (!(reg.ecx & 0x0f)) {
   cerr << "Bluetooth unavailable\n";
   return ret;
 }
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 1;
 ret=0;
 ret = HciFunction( &reg );
 if ( ret!=0 ) {
   cerr << "error checking Bluetooth switch status\n";
   return ret;
 }
 os << "bluetooth: ";
 if(!(reg.ecx & 0x1)) {
   os << "wireless switch is off\n";
   return ret;
 }
 if(!(reg.ecx & 0x80)) {
   os << "power is off\n";
   return ret;
 }
 if(!(reg.ecx & 0x40)) {
   os << "interface detached";
   return ret;
 }

 os << "attached";
 return ret;
} /* BlueToothFeature::query */

struct ThreeGRFFeature : public HciFeature {
  ThreeGRFFeature(const char* name) :
    HciFeature(HCI_WIRELESS,name) {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
ThreeGRFFeature::action(const char** c) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 if (  ret!=0 ) {
   //   cerr << "error querying 3g modem\n";
   return ret;
 }
 if (!(reg.ecx & 0x0f)) {
   cerr << "3g modem unavailable\n";
   return ret;
 }
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 1;
 ret=0;
 ret = HciFunction( &reg );
 if ( ret!=0 ) {
   cerr << "error checking 3g modem status\n";
   return ret;
 }
 if(!(reg.ecx & 0x1)) {
   cerr << "wireless switch is off\n";
   return ret;
 }

 if ( (String(*c) == "on") ||
      (String(*c) == "1")    ) { // turn on

   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 1;
   reg.edx = 0x2000;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error activating 3g modem device\n";
     return ret;
   }
 }
 if ( (String(*c) == "off") ||
      (String(*c) == "0") )  { // turn off
   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 0;
   reg.edx = 0x2000;
   ret=0;
   ret = HciFunction( &reg );
   if ( ret!=0 ) {
     cerr << "error deactivating 3g modem device\n";
     return ret;
   }
   return ret;
 }
return ret;
} /* ThreeGRFFeature::action */

int
ThreeGRFFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 if (  ret!=0 ) {
   //   cerr << "error querying 3g modem\n";
   return ret;
 }
 if (!(reg.ecx & 0x0f)) { //At the modem this is the same as bluetooth
	                  //I don't know how to check for 3g modem presence
   cerr << "3g modem unavailable\n";
   return ret;
 }
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.edx = 1;
 ret=0;
 ret = HciFunction( &reg );
 if ( ret!=0 ) {
   cerr << "error checking 3g switch status\n";
   return ret;
 }
 os << "3g modem: ";
 if(!(reg.ecx & 0x1)) {
   os << "wireless switch is off\n";
   return ret;
 }
 if(!(reg.ecx & 0x2000)) {
   os << "off";
   return ret;
 }

 os << "on";
 return ret;
} /* ThreeGRFFeature::query */



struct VideoFeature : public HciFeature {
  VideoFeature(unsigned short hciMode,const char* name) :
    HciFeature(hciMode,name) {}
  virtual int query(OStringStream& os) const;
};

int
VideoFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 int ret=0;
 ret = HciFunction( &reg );
 reg.ecx &= ~0x0080;  //this is some sort of status bit
 if ( ret == HCI_SUCCESS ) {
   for (int i=0 ; i<values.size() ; i++)
     if ( reg.ecx == values[i]->sciCode ) {
       os << name << ": " << values[i]->oString << ends;
       return 0;
     }
   cerr << "VideoFeature::query: received an unexpected response for feature " 
	<< name << ": " << reg.ecx << '\n';
 }
 return ret;
} /* VideoFeature::query */

struct LbaFeature : public HciFeature {
  LbaFeature(unsigned short hciMode,const char* name) :
    HciFeature(hciMode,name) {}
  virtual ~LbaFeature() {}

  //  virtual int action(const char**) const;
  virtual int action(const char **s) const {
   SMMRegisters reg;
   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.edx = 0;
   if ((*s)[0]=='0' &&
       tolower((*s)[1])=='x') {
     IStringStream is( (*s)+2 );
     long tmp = reg.ecx;
     is >> hex >> tmp;
   } else {
     IStringStream is( *s );
     long tmp = reg.ecx;
     is >> tmp;
   }
   cout << "setting lba to " << reg.ecx << '\n';
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) 
     return 1;
   else {
     cerr << "HCI error setting " << name << '\n';
     cerr << "\tHciFunction returned: " << error(ret) <<'\n';
     return 0;
   }
  };

  virtual int query(OStringStream& os) const {
   SMMRegisters reg;
   reg.eax = HCI_GET;
   reg.ebx = hciMode;
   reg.ecx = 0;
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) {
     os << name << ": 0x" << hex << reg.ecx << dec << " (" << reg.ecx << ")";
     return 0;
   } 
   return ret;
  }
};

struct HibInfoFeature : public HciFeature {
  int hibInfoMode;
  HibInfoFeature(int hibInfoMode,const char* name) :
    HciFeature(HCI_HIBERNATION_INFO,name), hibInfoMode(hibInfoMode) {}
  virtual ~HibInfoFeature() {}

  //  virtual int action(const char**) const;
  virtual int action(const char **s) const {
   SMMRegisters reg;
   reg.eax = HCI_SET;
   reg.ebx = hciMode;
   reg.ecx = 0;
   reg.edx = hibInfoMode;
   if ((*s)[0]=='0' &&
	 tolower((*s)[1])=='x') {
     IStringStream is( (*s)+2 );
     long tmp = reg.ecx;
     is >> hex >> tmp;
   } else {
     IStringStream is( *s );
     long tmp = reg.ecx;
     is >> tmp;
   }
   String type = "invalid";
   switch (hibInfoMode) {
     case HCI_BIOS_SIZE   : type = "BIOS"; break;
     case HCI_MEMORY_SIZE : type = "Memory"; break;
     case HCI_VRAM_SIZE   : type = "VRAM"; break;
   }
   cout << "setting " << type << " size to " << reg.ecx << '\n';
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) 
     return 1;
   else {
     cerr << "HCI error setting " << name << '\n';
     cerr << "\tHciFunction returned: " << error(ret) <<'\n';
     return 0;
   }
  };

  virtual int query(OStringStream& os) const {
   SMMRegisters reg;
   reg.eax = HCI_GET;
   reg.ebx = hciMode;
   reg.ecx = hibInfoMode;
   int ret = HciFunction( &reg );
   if ( ret == HCI_SUCCESS ) {
     os << name << ": " << reg.ecx;
     return 0;
   }
   return ret;
  }
}; // HibInfoFeature
   

struct OwnerStringFeature : public HciFeature {
  OwnerStringFeature(unsigned short hciMode,const char* name) :
    HciFeature(hciMode,name) {}
  virtual ~OwnerStringFeature() {}
  virtual int action(const char**) const;
  virtual int query(OStringStream& os) const;
};

int
OwnerStringFeature::action(const char **s) const
{
 SMMRegisters reg;
 String str(*s);
 str.gsub("\\n","\n\r");       //deal w newlines and tabs
 str.gsub("\\t" ,"        "); 
 // str.gsub("\n[^\r]","\n\r"); //doesn't work
 for (int i=0 ; i<(int)str.length() ; i++)
   if ( str[i] == '\n' )
     str = subString(str,0,i) + "\n\r" + subString(str,i+1,-1);
 // str.gsub("	","        "); 
 // str.gsub("\011","        "); 
 // str.gsub("\t","        "); 
 const char* p=str;
 // cout << "setting owner string to " << p << '\n';
 if ( strlen(p) > 512 ) {//FIX: should query for this
   cerr << "string too long\n";
   return 0;
 }
 int allowedFailures=30;
 for (int i=0 ; i<512 ; i+=4) {
   reg.eax = HCI_SET;
   reg.ebx = HCI_OWNERSTRING;
   reg.ecx = 4;
   reg.esi = i;
   reg.edx = 0;
   for (int j=0 ; j<4 ; j++,p++) {
     unsigned int tmp=0;
     tmp |= *p;
     tmp = tmp<<8*j;
     reg.edx |= tmp;
     if ( !*p) {
//	 reg.edx = reg.edx<<(3-j)*8;
	 break;
     }
   }
   //   cout << "edx: " << hex << reg.edx << dec << '\n';
   int ret = HciFunction(&reg);
   if ( ret != HCI_SUCCESS ) {
     allowedFailures--;
     if ( allowedFailures<=0 ) {
       cerr << "too many HCI errors setting feature " << name << '\n'
	    << "\tHciFunction returned " << error(ret) 
	    << " for i= " << i << '\n';
       return 0;
     }
     i -= 4;
     p -= 4;
   }
//     cerr << "HCI error setting feature " << name << '\n'
//	    << "\tHciFunction returned " << error(ret) 
//	    << "for i= " << i << '\n';
//     return 0;
 }
 return 1;
} /* OwnerStringFeature::action */

int
OwnerStringFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 reg.ecx = 0;
 reg.esi = 0;
 int ret = HciFunction( &reg );
 os << name << ": ";
 if ( ret == HCI_SUCCESS ) {
   int length = (reg.ecx & 0xffff0000)>>16;
   
   os << "[ max length: " << length << ']' << '\n';

   for (int i=0 ; i<length ; i+=4) {
     reg.eax = HCI_GET;
     reg.ebx = hciMode;
     reg.ecx = 4;
     reg.esi = (unsigned long) i;
     if ( HciFunction( &reg ) != HCI_SUCCESS ) {
       // i -= 4;
       cerr << "error in query...\n";
       break;
     } else {
       //int valid = reg.ecx & 0xffff;
       if ( reg.edx == 0x0000 ) {
	 os << '\n' << ends;
	 return 0;
       }
       //printf("index=%d  valid=%d\n", reg.esi, valid);
       unsigned int characters = reg.edx;
       for (int j=0 ; j<4 ; j++) {
	 char c = (char) characters & 0xff;
	 os << c;
	 //printf("%02x ", c);
	 characters = characters >> 8;
       }
       
       //     if ((c>=0x20) && (c<=0x80))
//	 *(string++) = c;
       //     if (c==0x0d)
//	 *(string++) = c;
     }
   }
//   char temp[5];
//   memcpy(temp,&reg.ecx,4);
//   temp[4]='\0';
//   os << name << ": " << temp[0] << temp[1] << temp[2] << temp[3] << '\n';
//   os << reg.ecx << '\n' << ends;
//   cout	<< name << ": " << temp[3] << '\n';
//   cout	<< reg.ecx << '\n';
   //os << name << ':' << ( reg.cx?(char*)reg.cx:(char*)"" ) << ends;
   os << '\n';
 } 
 os << ends;
//else {
//   cerr << "HciFeature::query: HciFunction for feature "
//	  << name << " returned " << error(ret) << '\n';
//   return 0;
// }
 return ret;
} /* OwnerStringFeature::query */

struct LCDFeature : public HciFeature {
  LCDFeature(unsigned short hciMode,const char* name) :
    HciFeature(hciMode,name) {}
  CDSList<ValueSet*> values;
  void addValue(const char* input,
		      unsigned short hciCode,
		const char* output) 
    { values.append(new ValueSet(input,hciCode,output)); }
  virtual ~LCDFeature() {}
  virtual int action(const char**) const { return 0; }
  virtual int query(OStringStream& os) const;
};

int
LCDFeature::query(OStringStream& os) const
{
 SMMRegisters reg;
 reg.eax = HCI_GET;
 reg.ebx = hciMode;
 int ret = HciFunction( &reg );
 if ( ret == HCI_SUCCESS ) {
   os << name << ": ";
   switch ( (reg.ecx & 0xff00)>>8 ) {
     case HCI_640_480 : os << " 640x480, "; break;
     case HCI_800_600 : os << " 800x600, "; break;
     case HCI_1024_768: os << "1024x768, "; break;
     case HCI_1024_600: os << "1024x600, "; break;
     case HCI_800_480 : os << " 800x480, "; break;
     case HCI_1400_1050 : os << " 1400x1050, "; break;
     case HCI_1600_1200 : os << " 1600x1200, "; break;
     case HCI_1280_600  : os << " 1280x600, "; break;
     case HCI_1280_800  : os << " 1280x800, "; break;
     case HCI_1440_900  : os << " 1440x900, "; break;
     case HCI_1920_1200 : os << " 1920x1200, "; break;
     default: os << "resolution (" << ((reg.ecx & 0xff00)>>8 ) << ") unknown";
   }
   switch ( reg.ecx & 0xff ) {
     case HCI_STN_MONO   : os << "mono STN  "; break;
     case HCI_STN_COLOUR : os << "colour STN"; break;
     case HCI_9BIT_TFT   : os << "9 bit TFT "; break; 
     case HCI_12BIT_TFT  : os << "12 bit TFT"; break; 
     case HCI_18BIT_TFT  : os << "18 bit TFT"; break; 
     case HCI_24BIT_TFT  : os << "24 bit TFT"; break;	   
     default: os << "type (" << (reg.ecx&0xff) << ") unknown";
   }
   os << ends;
   return 0;
 }
 return ret;
} /* LCDFeature::query */


class CmdLineArg {
public:
  virtual ~CmdLineArg() {}
  virtual const char* flag() const =0;
  virtual int         numArgs() const =0;
  virtual const char* usage() const =0;
  virtual void        action(const int&,
			     const char**) =0;
  virtual const char* name() const { return ""; }
};

class CmdLineArgs {
  CDSList<const char*> argList;
  const char*        path;
  CmdLineArg** options;
public:
  CmdLineArgs(const char*        path,
		    CmdLineArg** options) : path(path), options(options) {}

  int process(const int          argc,
	      const char**       argv);
  void usage();
  void error(const char *err);
};


void 
CmdLineArgs::usage()
{
 cerr << "usage: " << path << " [args]\n";
 cerr << "\twhere args are one or more of:\n";
 for ( CmdLineArg** opp=options ; *opp ; opp++ ) {
   CmdLineArg* op = *opp;
   cerr << op->flag() << ": " << op->usage() << '\n';
 }
} /* CmdLineArgs::usage */

void 
CmdLineArgs::error(const char *err)
{
 cerr << "Command line error: " << err << '\n';
 usage();
 exit(2);
}


int 
CmdLineArgs::process(const int          argc,
		     const char**       argv)
{
 int  argcnt  = argc-1;
 const char **argvp = argv;
 int ret=1;

 while (argvp++ , (argcnt--) > 0) {
   if (**argvp == '-') {
     int ok=0;
     for ( CmdLineArg** opp=options ; *opp ; opp++ ) {
       CmdLineArg* op = *opp;
       if ( strcmp( op->flag() , argvp[0] ) == 0 ) {
	 const char **p = argvp;
	 const char* empty = "";
	 if (argcnt < op->numArgs())
	   //	   error( op->flag() );
	   p = &empty;
	 op->action( argcnt, p );
	 argcnt-=op->numArgs(); argvp+=op->numArgs();
	 ok=1;
	 break;
       }
     }
     if ( !ok ) {
       error("unrecognized command-line option");
       ret=0;
     }
   }
   // deal with default arguments (no ``-'')
   argList.append( argvp[0] );
 }
 return ret;
} /* CmdLineArg::process */

template<int args>
class ArgSet : public CmdLineArg {
  const char* flag_;
  const char* usage_;
  const Feature* feature;
public:
  ArgSet(const char* flag,
	   const char* usage,
	   const Feature* feature) :
    flag_(flag), usage_(usage), feature(feature) {}
  const char* flag() const    { return flag_; }
  int         numArgs() const { return args; }
  const char* usage() const   { return usage_; }
  void        action(const int&   i,
		     const char** a)
    { 
     const char* empty = "";
     const char* p[args+1];
     String tmpStr;
     for (int j=0 ; j<i&&j<args ; j++)
       p[j] = a[j+1];
     for (int j=i ; j<args ; j++)
       p[j] = empty;
     for (int j=0 ; j<args ; j++) {
       if ( String(p[j]) == "-" ) {
	 read(cin,tmpStr);
	 if ( tmpStr[ tmpStr.length()-1 ] == '\n' ) // strip trailing nl
	   tmpStr = subString(tmpStr,0,tmpStr.length()-1);
	 p[j] = (const char*)tmpStr;
       }
       //cout << "arg[" << j << "]: " << p[j] << ' ' << strlen(p[j]) << '\n';
     }
     feature->action( p ); 
     if ( !fast ) {
       OStringStream os; 
       feature->query(os); 
       os << ends;
       cout << os.rdbuf() << '\n'; 
     }
    }
  const char* name() const { return feature->name; }
};

class ArgQuery : public CmdLineArg {
  const char* flag_;
  const char* usage_;
  int numArgs_;
  CDSList<Feature*> features;
  CmdLineArg** options;
public:
  ArgQuery(const char* flag,
	   const char* usage,
	   const CDSList<Feature*>& features,
		 CmdLineArg** options) :
    flag_(flag), usage_(usage), numArgs_(0), features(features),
    options(options) {}
  const char* flag() const    { return flag_; }
  int         numArgs() const { return numArgs_; }
  const char* usage() const   { return usage_; }
  void        action(const int&   numArgs,
		     const char** a      );
};

//change: query should return a list of strings (or streams) which ArgQuery 
// formats for output

void
ArgQuery::action(const int&   numArgs,
		 const char** a      ) 
{
 String glob = "*";
 if (numArgs == 0) {
   cout << versionString;
   numArgs_=0;
 } else {
   glob = String("*") + a[1] + "*";
   numArgs_=1;
 }

 int cnt=0;
 for (int i=0 ; i<features.size() ; i++) {
   OStringStream os;
   if ( wildmat(features[i]->name , glob,1) ) {
     int ret = features[i]->query(os);
     if ( ret==0 ) {
       cout.setf(ios::left);
       os << ends;
       const char* str = os.str();
       if ( longQuery ) {
	 const char* flag = "";
	 for (CmdLineArg** cl=options ; *cl ; cl++)
	   if ( features[i]->name == (*cl)->name() ) 
	     flag = (*cl)->flag();
	 cout << ' ' << setw(10) << flag << " "
	      << setw(38) << (str?str:"") << '\n';
       } else {
	 cout << ' ' << setfill(' ') <<setw(38)<< (str?str:"");
	 if ( cnt%2==1 )
	   cout << '\n';
       }
       cnt++;
     }
     if ( ret && verbose )
       cerr << "error when querying feature: " << features[i]->name << ": "
	    << features[i]->error(ret) << '\n';
   }
 }
 if ( !longQuery && cnt%2==1 )
   cout << '\n';
} /* ArgQuery::action */


int 
main(      int   argc, 
     const char* argv[])
{
 int version,bios;

// if (ioperm(0xb2, 1, 1)) {
//   cerr << argv[0] << ": can't get I/O permissions.\n" 
//	<< "\tRead/write permission to /dev/toshiba is required.\n";
//   return 1;
// }

 // should be use /dev/toshiba or direct calls to the BIOS
 detAccessMode();

 /* do some quick checks on the laptop */ 

 bios = HciGetBiosVersion();
 if (bios==0) {
   cerr << argv[0] << ": unable to get BIOS version" << endl;
   return 1;
 }

 if (HciGetMachineID(&id)==HCI_FAILURE) {
   cerr << argv[0] <<  ": unable to get machine identification" << endl;
   return 1;
 }

 /* drop root priveleges to minimize the risk of running suid root */

 seteuid(getuid()); //FIX: use setuid/setgid
 setegid(getgid());

 if ( (id>>8) != 0xfc )
   cout << "This machine does not appear to have a Toshiba BIOS.\n\t"
	<< "machine id: " << hex << id << "\n\t"
	<< "Don't expect this program to do anything useful!!\n";

 if (SciSupportCheck(&version)==1) {
   cerr << argv[0] << ": this computer is not supported" << endl;
   return 1;
 }


 if ( checkToshibaID(id) )
   snprintf(versionString,80,
	    " machine id: 0x%04x    BIOS version:"
	    " %d.%d    SCI version: %d.%d\n", id,
	    (bios & 0xff00)>>8, bios & 0xff,
	    (version & 0xff00)>>8, version & 0xff);
 else {
   printf("%s: unrecognized machine "
	  "identification:\n",argv[0]);
   printf("machine id : 0x%04x    BIOS version : %d.%d"
	  "    SCI version: %d.%d\n", id,
	  (bios & 0xff00)>>8, bios & 0xff,
	  (version & 0xff00)>>8, version & 0xff);
   cout << "\nplease email this information to charles@schwieters.org "
	<< " and include the following: \n"
	<< " model\nand model number, eg. Libretto 50CT model number PA1249E\n"
	<< " the output of cat /proc/acpi/toshiba/version\n"
	<< " lsmod |grep tosh\n";
 }

 /* check to see if a copy of wmTuxTime is already running */
 
 SciOpenInterface();

 CDSList<Feature*> features;

 ToggleFeature verboseFeature(verbose,"verbose");
 ToggleFeature longFeature(longQuery,"long query");
 ToggleFeature fastFeature(fast,"run fast");

 VersionFeature versionFeature;
 features.append(&versionFeature);

 ModelFeature modelFeature(id);
 features.append(&modelFeature);

 AccessFeature accessFeature;
 features.append(&accessFeature);

 SciFeature batteryFeature(SCI_BATTERY_SAVE,"battery save mode");
 batteryFeature.addValue( "user"   , SCI_USER_SETTINGS , "user settings");
 batteryFeature.addValue( "full"   , SCI_FULL_POWER    , "full power");
 batteryFeature.addValue( "low"    , SCI_LOW_POWER     , "low power");
 batteryFeature.addValue( "economy", SCI_ECONOMY       , "economy settings");
 batteryFeature.addValue( "normal" , SCI_NORMAL_LIFE   , "normal life");
 batteryFeature.addValue( "long"   , SCI_LONG_LIFE     , "long life");
 batteryFeature.addValue( "full"   , SCI_FULL_LIFE     , "full life");
 features.append(&batteryFeature);

 HciFeature acFeature(HCI_AC_ADAPTOR,"power source");
 acFeature.addValue( ""   , 3   , "battery" );
 acFeature.addValue( ""   , 4   , "external" );
 features.append(&acFeature);

 HciFeature backlightFeature(HCI_BACKLIGHT,"LCD backlight");
 backlightFeature.addValue( "off"  , HCI_DISABLE   , "off" );
 backlightFeature.addValue( "on"   , HCI_ENABLE   , "on" );
 features.append(&backlightFeature);

 HciFeature trBacklightFeature(HCI_TR_BACKLIGHT,"transreflective mode");
 trBacklightFeature.addValue( "off"  , HCI_ENABLE   , "off" );
 trBacklightFeature.addValue( "on"   , HCI_DISABLE   , "on" );
 features.append(&trBacklightFeature);

 HciFeature fanFeature(HCI_FAN,"fan");
 fanFeature.addValue( "off"  , HCI_DISABLE   , "off" );
 fanFeature.addValue( "on"   , HCI_ENABLE   , "on" );
 fanFeature.addValue( "fan1" , HCI_FAN_FAN1  , "fan1" );
 fanFeature.addValue( "fan2" , HCI_FAN_FAN2  , "fan2" );
 fanFeature.addValue( "1/4" , HCI_FAN_LOW2  , "1/4" );
 fanFeature.addValue( "3/4" , HCI_FAN_HIGH2  , "3/4" );
 fanFeature.addValue( "2/4"  , HCI_FAN_LOW3   , "2/4" );
 fanFeature.addValue( "high1" , HCI_FAN_HIGH1  , "high1" );
 fanFeature.addValue( "high" , HCI_FAN_HIGH  , "high" );
 fanFeature.addValue( "low" , HCI_FAN_LOW  , "low" );
 fanFeature.addValue( "1/8" , HCI_FAN_LOW4  , "1/8" );
 features.append(&fanFeature);

 HciFeature selectBayFeature(HCI_SELECT_STATUS,"select bay");
 // fanFeature.addValue( "on"   , HCI_ENABLE   , "on" );
 selectBayFeature.addValue( ""  , HCI_NOTHING  , "empty" );
 selectBayFeature.addValue( ""  , HCI_FLOPPY   , "floppy" );
 selectBayFeature.addValue( ""  , HCI_ATAPI    , "CDROM" );
 selectBayFeature.addValue( ""  , HCI_IDE      , "hard disk" );
 selectBayFeature.addValue( ""  , HCI_BATTERY  , "battery" );
 features.append(&selectBayFeature);

 HciFeature selectBayLockFeature(HCI_LOCK_STATUS,"select bay lock");
 // fanFeature.addValue( "on"   , HCI_ENABLE   , "on" );
 selectBayLockFeature.addValue( "engage"  , HCI_LOCKED   , "engaged" );
 selectBayLockFeature.addValue( "dis"     , HCI_UNLOCKED , "disabled" );
 features.append(&selectBayLockFeature);

 SciFeature irFeature(SCI_INFRARED_PORT,"IR port");
 irFeature.addValue( "off"  , SCI_OFF   , "off" );
 irFeature.addValue( "on"   , SCI_ON    , "on" );
 features.append(&irFeature);

// HciFeature firFeature(HCI_FIR_STATUS,"HCI IR port");
// firFeature.addValue( "on"   , HCI_ENABLE   , "on" );
// firFeature.addValue( "off"  , HCI_DISABLE   , "off" );
// features.append(&firFeature);

 SciFeature legacyUSBFeature(SCI_USB_LEGACY_MODE, "USB legacy mode");
 legacyUSBFeature.addValue("disable" , SCI_DISABLED , "disabled");
 legacyUSBFeature.addValue("enable"  , SCI_ENABLED , "enabled");
 features.append(&legacyUSBFeature);

 SciFeature USBFDDFeature(SCI_USB_FDD_EMULAT, "USB FDD emulation mode");
 USBFDDFeature.addValue("disable" , SCI_DISABLED , "disabled");
 USBFDDFeature.addValue("enable"  , SCI_ENABLED , "enabled");
 features.append(&USBFDDFeature);

 SciFeature LANcontrollerFeature(SCI_LAN_CONTROLLER, "LAN controller");
 LANcontrollerFeature.addValue("disable" , SCI_DISABLED , "disabled");
 LANcontrollerFeature.addValue("enable"  , SCI_ENABLED , "enabled");
 features.append(&LANcontrollerFeature);

 SciFeature soundlogoFeature(SCI_SOUND_LOGO, "sound logo");
 soundlogoFeature.addValue("disable" , SCI_DISABLED , "disabled");
 soundlogoFeature.addValue("enable"  , SCI_ENABLED , "enabled");
 features.append(&soundlogoFeature);

 SciFeature startuplogoFeature(SCI_STARTUP_LOGO, "startup logo");
 startuplogoFeature.addValue("picture" , SCI_PICTURE_LOGO , "picture");
 startuplogoFeature.addValue("animation"  , SCI_ANIMATION_LOGO , "animation");
 features.append(&startuplogoFeature);

 HciFeature videoFeature(HCI_VIDEO_OUT,"Video out");
 videoFeature.addValue("int" ,HCI_INTERNAL , "internal: LCD" );
 videoFeature.addValue("ext" ,HCI_EXTERNAL , "external monitor" );
 videoFeature.addValue("both",HCI_SIMULTANEOUS , 
		       "internal and external monitor");
 videoFeature.addValue("tv",HCI_TVOUT , "tv out");
 videoFeature.addValue("mode5", 0x105 , "mode5 ??");
 videoFeature.addValue("mode6", 0x106 , "mode6 ??");
 videoFeature.addValue("mode7", 0x107 , "mode7 ??");
 features.append(&videoFeature);

 int supportsHibernation = 1;
 HibInfoFeature hibInfoBIOSFeature(HCI_BIOS_SIZE,"HibInfo: BIOS size");
 HibInfoFeature hibInfoMemoryFeature(HCI_MEMORY_SIZE,"HibInfo: memory size");
 HibInfoFeature hibInfoVRAMFeature(HCI_VRAM_SIZE,"HibInfo: VRAM size");
 if ( supportsHibernation ) {
   features.append(&hibInfoBIOSFeature);
   features.append(&hibInfoMemoryFeature);
   features.append(&hibInfoVRAMFeature);
 }

 //FIX: should rename class
 LbaFeature hibLbaFeature(HCI_HIBERNATION_LBA,"Hibernation LBA");
// LbaFeature biosSizeFeature(HCI_BIOS_SIZE,"BIOS size");
// LbaFeature vmemSizeFeature(HCI_MEMORY_SIZE,"virtual Memory size");
// LbaFeature ramSizeFeature(HCI_VRAM_SIZE,"RAM size");
 if ( supportsHibernation ) {
   features.append(&hibLbaFeature);
//   features.append(&biosSizeFeature);
//   features.append(&vmemSizeFeature);
//   features.append(&ramSizeFeature);
 }

 LCDFeature fpanelFeature(HCI_FLAT_PANEL,"flat panel");
 // fpanelFeature.addValue("" , 516 , "1024x768 active matrix");
// fanFeature.addValue( "on"   , HCI_ENABLE   , "on" );
// fanFeature.addValue( "off"  , HCI_DISABLE   , "off" );
 features.append(&fpanelFeature);

 SciFeature beepFeature(SCI_SYSTEM_BEEP,"system beep");
 beepFeature.addValue( "off"  , SCI_OFF  , "off" );
 beepFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&beepFeature);

 SciFeature lcdFeature(SCI_LCD_BRIGHTNESS,"lcd brightness");
 lcdFeature.addValue("semi"   , SCI_SEMI_BRIGHT  , "semi-bright" );
 lcdFeature.addValue("bright" , SCI_BRIGHT       , "bright" );
 lcdFeature.addValue("super"  , SCI_SUPER_BRIGHT , "super-bright" );
 features.append(&lcdFeature);

 LCDIntensityFeature intensityFeature("lcd intensity");
 features.append(&intensityFeature);

 SciFeature processingFeature(SCI_PROCESSING,"CPU speed");
 processingFeature.addValue("slow" , SCI_LOW  , "slow" );
 processingFeature.addValue("fast" , SCI_HIGH  , "fast" );
 features.append(&processingFeature);

 SciFeature sleepFeature(SCI_SLEEP_MODE,"CPU sleep mode");
 sleepFeature.addValue( "off"  , SCI_OFF  , "off" );
 sleepFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&sleepFeature);

 SciFeature dstretchFeature(SCI_SLEEP_MODE,"Display stretch");
 dstretchFeature.addValue( "off"  , SCI_OFF  , "off" );
 dstretchFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&dstretchFeature);

 SciFeature cpuCacheFeature(SCI_CPU_CACHE,"CPU cache");
 cpuCacheFeature.addValue( "off"  , SCI_OFF  , "off" );
 cpuCacheFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&cpuCacheFeature);

 //????
 SciFeature cachePolicyFeature(SCI_CACHE_POLICY,"cache policy");
 cachePolicyFeature.addValue( "write-back"    , 0 , "write back" );
 cachePolicyFeature.addValue( "write-through" , 1 , "write through" );
 features.append(&cachePolicyFeature);

 SciFeature volumeFeature(SCI_SPEAKER_VOLUME,"speaker volume");
 volumeFeature.addValue( "off"    , SCI_VOLUME_OFF    , "off"    );
 volumeFeature.addValue( "low"    , SCI_VOLUME_LOW    , "low"    );
 volumeFeature.addValue( "medium" , SCI_VOLUME_MEDIUM , "medium" );
 volumeFeature.addValue( "high"   , SCI_VOLUME_HIGH   , "high"   );
 features.append(&volumeFeature);

 SciFeature batAlarmFeature(SCI_BATTERY_ALARM,"battery alarm");
 batAlarmFeature.addValue( "off"  , SCI_OFF  , "off" );
 batAlarmFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&batAlarmFeature);

 SciFeature panAlarmFeature(SCI_PANEL_ALARM,"panel alarm");
 panAlarmFeature.addValue( "off"  , SCI_OFF  , "off" );
 panAlarmFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&panAlarmFeature);

 SciFeature panPowerFeature(SCI_PANEL_POWER,"panel power");
 panPowerFeature.addValue( "off"  , SCI_OFF  , "off" );
 panPowerFeature.addValue( "on"   , SCI_ON   , "on" );
 features.append(&panPowerFeature);

 SciFeature hddFeature(SCI_HDD_AUTO_OFF,"hard disk auto-off time");
 hddFeature.addValue( "dis" , SCI_TIME_DISABLED , "disabled");
 hddFeature.addValue( "1"   , SCI_TIME_01 , "1 minute");
 hddFeature.addValue( "3"   , SCI_TIME_03 , "3 minutes");
 hddFeature.addValue( "5"   , SCI_TIME_05 , "5 minutes");
 hddFeature.addValue( "10"  , SCI_TIME_10 , "10 minutes");
 hddFeature.addValue( "15"  , SCI_TIME_15 , "15 minutes");
 hddFeature.addValue( "20"  , SCI_TIME_20 , "20 minutes");
 hddFeature.addValue( "30"  , SCI_TIME_30 , "30 minutes");
 features.append(&hddFeature);

 SciFeature displayFeature(SCI_DISPLAY_AUTO,"display auto-off time");
 displayFeature.addValue( "dis" , SCI_TIME_DISABLED , "disabled");
 displayFeature.addValue( "1"   , SCI_TIME_01 , "1 minute");
 displayFeature.addValue( "3"   , SCI_TIME_03 , "3 minutes");
 displayFeature.addValue( "5"   , SCI_TIME_05 , "5 minutes");
 displayFeature.addValue( "10"  , SCI_TIME_10 , "10 minutes");
 displayFeature.addValue( "15"  , SCI_TIME_15 , "15 minutes");
 displayFeature.addValue( "20"  , SCI_TIME_20 , "20 minutes");
 displayFeature.addValue( "30"  , SCI_TIME_30 , "30 minutes");
 features.append(&displayFeature);

// HciFeature hciPowerFeature(HCI_POWER_UP,"HCI power-up mode");
// hciPowerFeature.addValue( "boot"      , SCI_BOOT         	  , "boot");
// hciPowerFeature.addValue( "resume"    , SCI_RESUME       	  , "resume");
// hciPowerFeature.addValue( "hibernate" , SCI_HIBERNATE    	  , "hibernate");
// hciPowerFeature.addValue( "quick"     , SCI_QUICK_HIBERNATE , "quick-hibernate");
// features.append(&hciPowerFeature);

 SciFeature sciPowerFeature(SCI_POWER_UP,"power-up mode");
 sciPowerFeature.addValue( "boot"      , SCI_BOOT         	  , "boot");
 sciPowerFeature.addValue( "resume"    , SCI_RESUME       	  , "resume");
 sciPowerFeature.addValue( "hibernate" , SCI_HIBERNATE    	  , "hibernate");
 sciPowerFeature.addValue( "quick"     , SCI_QUICK_HIBERNATE , "quick-hibernate");
 features.append(&sciPowerFeature);

 PercentFeature batteryPercentFeature(SCI_BATTERY_PERCENT,"battery percent");
 features.append(&batteryPercentFeature);

 PercentFeature secBatteryPercentFeature(SCI_2ND_BATTERY,
					 "second battery");
 features.append(&secBatteryPercentFeature);

// PercentFeature secBatteryPercentFeature(SCI_2ND_BATTERY,"second battery");
// secBatFeature.addValue("disable" , 0 , "disabled");
// secBatFeature.addValue("enable"  , 1 , "enabled");
// secBatFeature.addValue("enable"  , 65 , "present");
// features.append(&secBatFeature);

 SciFeature coolingFeature(SCI_COOLING_METHOD,"cooling method");
 coolingFeature.addValue( "perform"   , SCI_PERFORMANCE	  , "performance");
 coolingFeature.addValue( "quiet"     , SCI_QUIET  	  , "quiet");
 coolingFeature.addValue( "other"     , 2      	          , "other");
 features.append(&coolingFeature);

 TimeFeature wakeAlarmFeature(SCI_ALARM_POWER,"power-up alarm");
// wakeAlarmFeature.addValue( "on"   , SCI_ALARM_ENABLED	, "on" );
// wakeAlarmFeature.addValue( "off"  , SCI_ALARM_DISABLED	, "off" );
 features.append(&wakeAlarmFeature);

 SciFeature autoOffFeature(SCI_SYSTEM_AUTO,"auto-off time");
 autoOffFeature.addValue( "dis" , SCI_TIME_DISABLED , "disabled");
 autoOffFeature.addValue( "10"  , SCI_TIME_10 , " 10 minutes");
 autoOffFeature.addValue( "20"  , SCI_TIME_20 , " 20 minutes");
 autoOffFeature.addValue( "30"  , SCI_TIME_30 , " 30 minutes");
 autoOffFeature.addValue( "40"  , SCI_TIME_40 , " 40 minutes");
 autoOffFeature.addValue( "50"  , SCI_TIME_50 , " 50 minutes");
 autoOffFeature.addValue( "60"  , SCI_TIME_60 , " 60 minutes");

 features.append(&autoOffFeature);

 SciFeature parallelFeature(SCI_PARALLEL_PORT,"parallel port mode");
 parallelFeature.addValue( "ecp" ,SCI_PARALLEL_ECP, "ecp" );
 parallelFeature.addValue( "spp" ,SCI_PARALLEL_SPP, "spp" );
 parallelFeature.addValue( "ps2" ,SCI_PARALLEL_PS2, "ps2" );
 // parallelFeature.addValue
 features.append(&parallelFeature);


 SciFeature standbyFeature(SCI_STANDBY_TIME,"Standby time");
 features.append(&standbyFeature);

 SciFeature hibernationFeature(SCI_HIBERNATION,"Hibernation");
 hibernationFeature.addValue("disable" , 0 , "not configured");
 hibernationFeature.addValue("enable"  , 1 , "configured");
 features.append(&hibernationFeature);

 // I don't know what this option does, but the following values seem to 
 // be valid on my 8100
 SciFeature pointerFeature(SCI_POINTING_DEVICE,"Pointer");
 pointerFeature.addValue("0" , 0 , "0");
 pointerFeature.addValue("1"  , 1 , "1");
 pointerFeature.addValue("2"  , 2 , "2");
 pointerFeature.addValue("3"  , 3 , "3");
 features.append(&pointerFeature);

 SciFeature bootFeature(SCI_BOOT_METHOD,"boot method");
 bootFeature.addValue( "fdhdcd" , SCI_FD_HD , "floppy->hard disk->CDROM");
 bootFeature.addValue( "hdfdcd" , SCI_HD_FD , "hard disk->floppy->CDROM");
 bootFeature.addValue( "fdcdhd" , 2         , "floppy->CDROM->hard disk");
 bootFeature.addValue( "hdcdfd" , 3         , "hard disk->CDROM->floppy");
 bootFeature.addValue( "cdfdhd" , 4         , "CDROM->floppy->hard disk");
 bootFeature.addValue( "cdhdfd" , 5         , "CDROM->hard disk->floppy");
 features.append(&bootFeature);

 HciFeature wirelessFeature(HCI_WIRELESS,"wireless support");
 wirelessFeature.addValue( "" , 0 , "not present");
 wirelessFeature.addValue( "" , 0xf , "present");
 features.append(&wirelessFeature);

 WirelessFeature wirelessSwitchFeature(0x1, "wireless switch");
 features.append(&wirelessSwitchFeature);

 BlueToothFeature blueToothFeature("bluetooth");
 features.append(&blueToothFeature);

 ThreeGRFFeature threeGRFFeature("3g");
 features.append(&threeGRFFeature);


 //FIX: need to enumerate these
 HciFeature bootDeviceFeature(HCI_BOOT_DEVICE,"boot device");
 //features.append(&bootDeviceFeature);

 PasswdFeature userPasswdFeature(SCI_USER_PASSWORD,"user password");
 features.append(&userPasswdFeature);

 PasswdFeature superPasswdFeature(SCI_SUPER_PASSWORD,"supervisor password");
 features.append(&superPasswdFeature);

 OwnerStringFeature ownerStringFeature(HCI_OWNERSTRING,"owner string");
 features.append(&ownerStringFeature);

 CmdLineArg* argList[] = { 
   new ArgSet<1>("-b","<on|off> enable/disable system beep" ,     &beepFeature),
   new ArgSet<1>("-lcd","<string> set lcd brightness"       ,     &lcdFeature),
   new ArgSet<1>("-inten","<num> set lcd intensity"       ,     &intensityFeature),
   new ArgSet<1>("-pow","<setting> set power-up mode",            
		 &sciPowerFeature),
//   new ArgSet<1>("-hpow","<setting> set power-up mode",          
//		   &hciPowerFeature),
   new ArgSet<1>("-vol","<0-3> set beep (and modem!) volume",       &volumeFeature),
   new ArgSet<1>("-hdd","<num> number of minutes until disk spindown",&hddFeature),
   new ArgSet<1>("-dstretch","<on|off> display stretch",&dstretchFeature),
   new ArgSet<1>("-d","<num> number of minutes until display auto-off",&displayFeature),
   new ArgSet<1>("-c","<quiet|perform> set cooling method",&coolingFeature),
   new ArgSet<1>("-bs","<setting> battery-save mode",&batteryFeature),
   new ArgSet<1>("-bl","<on|off> lcd backlight",&backlightFeature),
   new ArgSet<1>("-trmode","<on|off> transreflective mode",
		 &trBacklightFeature),
   new ArgSet<1>("-fan","<on|off> fan",&fanFeature),
   new ArgSet<1>("-hib","<enable|disable> hibernation",&hibernationFeature),
   new ArgSet<1>("-memsize","memory size in bytes",&hibInfoMemoryFeature),
   new ArgSet<1>("-pointer","<0|1|2> pointer",&pointerFeature),
   new ArgSet<1>("-lba","hibernation address",&hibLbaFeature),
   new ArgSet<1>("-video","<int|ext|both|tv> video out",&videoFeature),
   new ArgSet<1>("-cpu","<fast|slow> CPU speed",&processingFeature),
   new ArgSet<1>("-cpucache","<on|off> CPU cache",&cpuCacheFeature),
   new ArgSet<1>("-sleep","<on|off> sleep mode",&sleepFeature),
   new ArgSet<1>("-balarm","<on|off> battery alarm",&batAlarmFeature),
   new ArgSet<1>("-palarm","<on|off> lid-closed alarm",&panAlarmFeature),
   new ArgSet<1>("-walarm",
		 "<dis|HH:MM[/everyday|DD/MM[/YYYY]]> time/date to wake",
		 &wakeAlarmFeature),
   new ArgSet<1>("-ostring","<string> owner string",&ownerStringFeature),
   new ArgSet<1>("-ppower","<on|off> power-off when lid closed",&panPowerFeature),
   new ArgSet<1>("-boot","<boot sequence>",&bootFeature),
   new ArgSet<1>("-parallel","<option> parallel port mode",&parallelFeature),
   new ArgSet<0>("-upasswd","set user password",&userPasswdFeature),
   new ArgSet<0>("-spasswd","set supervisor password",&superPasswdFeature),
   new ArgSet<1>("-autooff","<dis|min> mins to system auto-off",
		 &autoOffFeature),
   new ArgSet<1>("-bluetooth","<on|off> enable/disable internal bluetooth",
		 &blueToothFeature),
   new ArgSet<1>("-3g","<on|off> enable/disable 3g modem frequency",
                 &threeGRFFeature),
   new ArgSet<1>("-usblegacy","<enable|disable> USB legacy mode",&legacyUSBFeature),
   new ArgSet<1>("-usbfdd","<enable|disable> USB FDD emulation mode",&USBFDDFeature),
   new ArgSet<1>("-lan","<enable|disable> LAN controller",&LANcontrollerFeature),
   new ArgSet<1>("-soundlogo","<enable|disable> sound logo",&soundlogoFeature),
   new ArgSet<1>("-startlogo","<picture|animation> startup logo mode",&soundlogoFeature),
   new ArgQuery("-q",
		"[glob] query option matching glob (or all if no arg)",
		features,argList),
   new ArgSet<0>("-v","toggle verbose mode",&verboseFeature),
   new ArgSet<0>("-l","toggle long query",&longFeature),
   new ArgSet<0>("-fast","skip checks, run faster",&fastFeature),
   0 };

 CmdLineArgs args( argv[0], argList );

 if ( argc<2 ) {
   args.usage();
   return 1;
 } else 
   if ( !args.process(argc,argv) )
     return 1;

 SciCloseInterface();

 return 0;
}

#if __GNUG__
#include "cdsList.cc"
#include "cdsString.cc"
#include "cdsSStream.cc"
#include "cdsMath.cc"
#endif
