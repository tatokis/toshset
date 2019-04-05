
#include "kernelInterface.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

int accessMode=ACCESS_DIRECT;

void
detAccessMode()
{
 accessMode=ACCESS_DIRECT;
#ifdef USE_KERNEL_INTERFACE
 if ( 0==access(TOSH_PROC, R_OK) )
   accessMode=ACCESS_KERNEL;
 else if ( 0==access("/proc/acpi",R_OK) ) {
   fprintf(stderr,"required kernel toshiba support not enabled.\n");
   exit(1);
 }
#endif /* USE_KERNEL_INTERFACE */
} /* detAccessMode */

#ifdef USE_KERNEL_INTERFACE
int 
procAccess(ToshProcInfo* proc)
{
 FILE *str;
 char buffer[64];

 if (access(TOSH_PROC, R_OK)) {
   //fprintf(stderr,
   //	     "can't open %s: falling back to direct access\n",
   //	     TOSH_PROC);
   return 0;
 }

 /* open /proc/toshiba for reading */

 if (!(str = fopen(TOSH_PROC, "r")))
   return 0;

 /* scan in the information */

 fgets(buffer, sizeof(buffer)-1, str);
 fclose(str);
 buffer[sizeof(buffer)-1] = '\0';
 sscanf(buffer, "%*s %x %*d.%*d %d.%d %*x %*x\n", 
	&proc->id, &proc->major, &proc->minor);

 return 1;
} /* procAccess */

int 
smmAccess(SMMRegisters *regs)
{
 int fd;
 
 if ((fd=open(TOSH_DEVICE, O_RDWR))<0) {
   printf("can't open %s: do you have read/write access?\n",
	  TOSH_DEVICE);
   //fprintf(stderr,
   //	     "can't open %s: falling back to direct access\n",
   //	     TOSH_DEVICE);
   return 1;
 }

 if (access(TOSH_PROC, R_OK)) {
   close(fd);
   return 1;
 }
 if (ioctl(fd, TOSH_SMM, regs)<0) {
   close(fd);
   return 1;
 }

 close(fd);

 return (int) (regs->eax & 0xff00)>>8;
} /* smmAccess */

#else 
int smmAccess(SMMRegisters* regs) { return 1; } /* failure */
int procAccess(ToshProcInfo* proc) { return 0; }
#endif
