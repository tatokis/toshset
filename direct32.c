
#include "kernelInterface.h"
#include "direct.h"
#include "hci.h"
#include "sci.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

/*
  code for direct access to the Toshiba BIOS registers- this does not use 
/dev/toshiba, and most likely will not work under ACPI.
 */

int dHciFunction(SMMRegisters *reg)
{
 int id=0;
 unsigned int eax,ebx,ecx,edx,esi,edi;

 eax = reg->eax;
 ebx = reg->ebx;
 ecx = reg->ecx;
 edx = reg->edx;
 esi = reg->esi;
 edi = reg->edi;

 /* emulate the HCI fan function for the Portage 610 and Tecra 700 */
   
 if ((id==0xfccb) && (ebx==HCI_FAN)) {
   asm("movw %1,%%ax\n\t" \
       "movw %2,%%cx\n\t" \
       "cli\n\t" \
       "movb $0xbe,%%al\n\t" \
       "outb %%al,$0xe4\n\t" \
       "inb $0xe5,%%al\n\t" \
       "cmpb $0xfe,%%ah\n\t" \
       "je 3f\n\t" \
       "cmpw $0x0001,%%cx\n\t" \
       "jne 1f\n\t" \
       "andb $0xfe,%%al\n\t" \
       "jmp 2f\n\t" \
       "1: orb $0x01,%%al\n\t" \
       "2: movb %%al,%%ah\n\t" \
       "movb $0xbe,%%al\n\t" \
       "outb %%al,$0xe4\n\t" \
       "movb %%ah,%%al\n\t" \
       "outb %%al,$0xe5\n\t" \
       "3: sti\n\t" \
       "andw $0x0001,%%ax\n\t" \
       "movw %%ax,%0\n" \
       :"=m" (ecx) : "m" (eax), "m" (ecx) : "memory" );
   ecx = (ecx==0x00) ? 0x01:0x00;
   eax = 0x0000;
 } else if ((id==0xfccb) && (ebx==HCI_FAN)) {
   asm("movw %1,%%ax\n\t" \
       "movw %2,%%cx\n\t" \
       "cli\n\t" \
       "movw $0x00e4,%%dx\n\t" \
       "movb $0xe0,%%al\n\t" \
       "outb %%al,%%dx\n\t" \
       "incw %%dx\n\t" \
       "inb %%dx,%%al\n\t" \
       "cmpb $0xfe,%%ah\n\t" \
       "je 3f\n\t" \
       "cmpw $0x0001,%%cx\n\t" \
       "jne 1f\n\t" \
       "orw $0x0001,%%ax\n\t" \
       "jmp 2f\n\t" \
       "1: andw $0xfffe,%%ax\n\t" \
       "2: decw %%dx\n\t" \
       "movb %%al,%%ah\n\t" \
       "movb $0xe0,%%al\n\t" \
       "outw %%ax,%%dx\n\t" \
       "3: sti\n\t" \
       "andw $0x0001,%%ax\n\t" \
       "movw %%ax,%0\n" \
       :"=m" (ecx) : "m" (eax), "m" (ecx) : "memory" );
   eax = 0x0000;		
 } else {
   /* note that an extra dummy push onto the stack is made.
    *	Somehow this entry sometimes get overwritten causing an
    * eventual segfault. 
    */
   asm("# load the values into the registers\n\t" \
       "pushl %%eax\n\t" \
       "pushl %%eax\n\t" \
       "movl 0(%%eax),%%edx\n\t" \
       "pushl %%edx\n\t" \
       "movl 4(%%eax),%%ebx\n\t" \
       "movl 8(%%eax),%%ecx\n\t" \
       "movl 12(%%eax),%%edx\n\t" \
       "movl 16(%%eax),%%esi\n\t" \
       "movl 20(%%eax),%%edi\n\t" \
       "popl %%eax\n\t" \
       "# call the System Management mode\n\t" \
       "inb $0xb2,%%al\n\t" \
       "# fill out the memory with the values in the registers\n\t" \
       "xchgl %%eax,(%%esp)\n\t" \
       "popl %%eax\n\t" \
       "xchgl %%eax,(%%esp)\n\t" \
       "movl %%ebx,4(%%eax)\n\t" \
       "movl %%ecx,8(%%eax)\n\t" \
       "movl %%edx,12(%%eax)\n\t" \
       "movl %%esi,16(%%eax)\n\t" \
       "movl %%edi,20(%%eax)\n\t" \
       "popl %%edx\n\t" \
       "movl %%edx,0(%%eax)\n\t" \
       :
       : "eax" (reg)
       : "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory", "cc");
 }
	

 return (int) (reg->eax & 0xff00)>>8;
} /* dHciFunction */

/*
 * Return the BIOS version of the laptop
 *
 *   I may or may not follow the Toshiba version of this function. The
 *   Toshiba function would appear to read from this area of memory, but
 *   exactly what it does I am not sure. This implemenation is impirically
 *   based on the contents of these memory locations on my Satellite Pro
 *   400CS and Libretto 50CT. I have confirmed that it returns the correct
 *   information on a Tecra 750CDT and a T4900 as well.
 */
int 
dHciGetBiosVersion(void)
{
 int id = 0;
 int device,major,minor;
 unsigned char *mem;
 if ((device = open("/dev/mem", O_RDWR))==-1)
   return HCI_FAILURE;
 
 mem = mmap(0, 0x1000, PROT_READ, MAP_SHARED, device, 0xfe000);
 if (mem==(unsigned char *)-1)
   return HCI_FAILURE;

 major = (char) mem[0x0009]-'0';
 minor = (((char) mem[0x000b]-'0')*10)+((char) mem[0x000c]-'0');

 munmap(mem, 0x1000);
 close(device);

 id = (major*0x100)+minor;

 return id;
} /* dHciGetBiosVersion */


int 
dHciGetMachineID(int *id)
{
 int device;
 unsigned char *mem;
 unsigned short bx;
 //	unsigned char fl;

 if ((device = open("/dev/mem", O_RDWR))==-1)
   return HCI_FAILURE;

 mem = mmap(0, 0x100000, PROT_READ, MAP_SHARED, device,0); // 0xfe000);
 if (mem==(unsigned char *)-1)
   return HCI_FAILURE;

 *id = (0x100*((int) mem[0xffffe]))+((int) mem[0xffffa]);
 /* do we have a SCTTable machine identication number on our hands */

 if (*id==0xfc2f) {
   unsigned char ah;
   unsigned short cx;
   unsigned long address;
     
   /* start by getting a pointer into the BIOS */

   asm ("movw $0xc000,%%ax\n\t" \
	"movw $0x0000,%%bx\n\t" \
	"movw $0x0000,%%cx\n\t" \
	"inb $0xb2,%%al\n\t" \
	"movb %%ah,%%al\n" \
	: "=b" (bx), "=a" (ah) : : "%ecx");

   /* printf("bx: %x  mem: %x\n",bx,mem); */

   /* At this point in the Toshiba routines under MS Windows
      the bx register holds 0xe6f5. However my code is producing
      a different value! For the time being I will just fudge the
      value. This has been verified on a Satellite Pro 430CDT,
      Tecra 750CDT, Tecra 780DVD and Satellite 310CDT. */
   bx = 0xe6f5;

   /* now twiddle with our pointer a bit */
     
   address = 0x000f0000+bx;
   //		cx = readw(address);
   //		  address = bx - 0xe000;
   cx = *(unsigned short*)(mem+address);
   address = 0x000f0009+bx+cx;
   //		  address = 0x9+bx+cx - 0xe000;
   //		cx = readw(address);
   cx = *(unsigned short*)(mem+address);
   address = 0x000f000a+cx;
   //		  address = 0xa+cx - 0xe000;
   //		cx = readw(address);
   cx = *(unsigned short*)(mem+address);
   
   /* now construct our machine identification number */

   *id = ((cx & 0xff)<<8)+((cx & 0xff00)>>8);

   munmap(mem, 0x1000);
   close(device);

 }
 return HCI_SUCCESS;
} /* dHciGetMachineID */

/*
 * Is this a supported Machine? (ie. is it a Toshiba)
 */
int 
dSciSupportCheck(int *version)
{
 unsigned short dx;
 unsigned char ah;

 asm ("pushl %%eax\n\t" \
      "pushl %%ebx\n\t" \
      "pushl %%ecx\n\t" \
      "pushl %%edx\n\t" \
      "movw $0xf0f0,%%ax\n\t" \
      "movw $0x0000,%%bx\n\t" \
      "movw $0x0000,%%cx\n\t" \
      "inb $0xb2,%%al\n\t" \
      "popl %%ecx\n\t" \
      "popl %%ebx\n\t" \
      "popl %%eax\n" \
      :"=d" (dx), "=a" (ah) : : "memory" );

 *version = (int) dx;

 return (int) (ah & 0xff);
} /* dSciSupportCheck */

int 
dSciOpenInterface(void)
{
 unsigned char ah;

 asm ("pushl %%eax\n\t" \
      "pushl %%ebx\n\t" \
      "pushl %%ecx\n\t" \
      "movw $0xf1f1,%%ax\n\t" \
      "movw $0x0000,%%bx\n\t" \
      "movw $0x0000,%%cx\n\t" \
      "inb $0xb2,%%al\n\t" \
      "movb %%ah,%0\n\t" \
      "popl %%ecx\n\t" \
      "popl %%ebx\n\t" \
      "popl %%eax\n" \
      :"=m" (ah) : : "memory" );

 return (int) ah;
} /* dSciOpenInterface */

int 
dSciCloseInterface()
{
 unsigned char ah;

 asm ("pushl %%eax\n\t" \
      "pushl %%ebx\n\t" \
      "pushl %%ecx\n\t" \
      "movw $0xf2f2,%%ax\n\t" \
      "movw $0x0000,%%bx\n\t" \
      "movw $0x0000,%%cx\n\t" \
      "inb $0xb2,%%al\n\t" \
      "popl %%ecx\n\t" \
      "popl %%ebx\n\t" \
      "popl %%eax\n" \
      :"=a" (ah) : : "memory" );
 
 return (int) (ah & 0xff);
} /* dSciCloseInterface */

int 
dSciGet(SMMRegisters *reg)
{
 unsigned short ax,bx,cx,dx;

 // note confusing mapping
 //ax = 0;
 bx = reg->ebx;
 cx = reg->ecx;
 dx = reg->edx;

 asm ("pushl %%esi\n\t" \
      "pushl %%edi\n\t" \
      "movw $0xf3f3,%%ax\n\t" \
      "inb $0xb2,%%al\n\t" \
      "popl %%edi\n\t" \
      "popl %%esi\n" \
      :"=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) \
      :"b" (bx), "c" (cx), "d" (dx) \
      : "memory" );

 reg->esi = (ax & 0xff);
 reg->ebx = bx;
 reg->ecx = cx;
 reg->edx = dx;

 return (int) (ax & 0xff00)>>8;
} /* dSciGet */



/*
 * Set the setting of a given mode of the laptop
 */
int
dSciSet(SMMRegisters *reg)
{
 int eax;

 reg->eax = 0xf4f4;

 asm ("# load the values into the registers\n\t" \
      "pushl %%eax\n\t" \
      "pushl %%eax\n\t" \
      "movl 0(%%eax),%%edx\n\t" \
      "push %%edx\n\t" \
      "movl 4(%%eax),%%ebx\n\t" \
      "movl 8(%%eax),%%ecx\n\t" \
      "movl 12(%%eax),%%edx\n\t" \
      "movl 16(%%eax),%%esi\n\t" \
      "movl 20(%%eax),%%edi\n\t" \
      "popl %%eax\n\t" \
      "# call the System Management mode\n\t" \
      "inb $0xb2,%%al\n\t"
      "# fill out the memory with the values in the registers\n\t" \
      "xchgl %%eax,(%%esp)\n\t" \
      "popl %%eax\n\t" \
      "xchgl %%eax,(%%esp)\n\t"
      "movl %%ebx,4(%%eax)\n\t" \
      "movl %%ecx,8(%%eax)\n\t" \
      "movl %%edx,12(%%eax)\n\t" \
      "movl %%esi,16(%%eax)\n\t" \
      "movl %%edi,20(%%eax)\n\t" \
      "popl %%edx\n\t" \
      "movl %%edx,0(%%eax)\n\t" \
      "# setup the return value to the carry flag\n\t" \
      "lahf\n\t" \
      "shrl $8,%%eax\n\t" \
      "andl $1,%%eax\n" \
      : "=a" (eax)
      : "a" (reg)
      : "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory");

 return eax;
} /* dSciSet */


