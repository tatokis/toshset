
/*
  code for direct access to the Toshiba BIOS registers- this does not use 
/dev/toshiba, and most likely will not work under ACPI.
 */

//#include "smm.h"

int dHciFunction(SMMRegisters *reg);
int dHciGetBiosVersion();
int dHciGetMachineID(int *id);

int dSciSupportCheck(int *version);
int dSciOpenInterface();
int dSciCloseInterface();
int dSciGet(SMMRegisters *reg);
int dSciSet(SMMRegisters *reg);
