
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
  64-bit version consists of nonworking stubs.
  This will probably never be made working.

  code for direct access to the Toshiba BIOS registers- this does not use 
/dev/toshiba, and most likely will not work under ACPI.
 */

int dHciFunction(SMMRegisters *reg)
{
 return HCI_FAILURE;
} /* dHciFunction */

int 
dHciGetBiosVersion(void)
{
 int id = 0;
 return id;
} /* dHciGetBiosVersion */


int 
dHciGetMachineID(int *id)
{
 return HCI_FAILURE;
} /* dHciGetMachineID */

int 
dSciSupportCheck(int *version)
{
 return SCI_FAILURE;
} /* dSciSupportCheck */

int 
dSciOpenInterface(void)
{
 return SCI_FAILURE;
} /* dSciOpenInterface */

int 
dSciCloseInterface()
{
 return SCI_FAILURE;
} /* dSciCloseInterface */

int 
dSciGet(SMMRegisters *reg)
{
 return SCI_FAILURE;
} /* dSciGet */



/*
 * Set the setting of a given mode of the laptop
 */
int
dSciSet(SMMRegisters *reg)
{
 return SCI_FAILURE;
} /* dSciSet */


