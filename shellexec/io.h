#ifndef __IO_H__
#define __IO_H__

#define RC_DEVICE "/dev/input/event1"

int InitRC(void);
int CloseRC(void);
int RCKeyPressed(void);
int GetRCCode(int);
void ClearRC(void);

#endif
