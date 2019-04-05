

#ifndef __kernelInterface_h__
#define __kernelInterface_h__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_KERNEL_INTERFACE
#  include<linux/toshiba.h>
#else
#  include "smm.h"
#endif

typedef struct {
  int major;
  int minor;
  int id;
} ToshProcInfo;

int procAccess(ToshProcInfo* proc);
int smmAccess(SMMRegisters *regs);

enum { ACCESS_DIRECT, ACCESS_KERNEL };
extern int accessMode;
void detAccessMode();


#ifdef __cplusplus
}
#endif

#endif /* __kernelInterface_h__ */
