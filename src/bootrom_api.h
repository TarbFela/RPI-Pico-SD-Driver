// A. Sheaff 2023/01/05
// Interface to Boot ROM for programming the flash

#ifndef BOOTROM_API_H
#define BOOTROM_API_H

// Checks to see if '*' and 'D' are both pressed at the
// same time.  If so, boot to MSD mode to be programmed.
void jump_to_MSD(void);

#endif // BOOTROM_API_H
