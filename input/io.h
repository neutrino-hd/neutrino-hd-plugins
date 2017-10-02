#ifndef __IO_H__
#define __IO_H__

#if defined HAVE_ARM_HARDWARE || !defined RC_DEVICE
#define RC_DEVICE "/dev/input/event1"
#elif definded HAVE_COOL_HARDWARE || !defined RC_DEVICE
#define RC_DEVICE "/dev/input/nevis_ir"
#endif

int InitRC(void);
int CloseRC(void);
int RCKeyPressed(void);
int GetRCCode(int);
void ClearRC(void);

#endif
