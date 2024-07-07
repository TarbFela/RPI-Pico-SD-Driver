// A. Sheaff 2023/01/05
// Interface to reset Pico and boot from ROM
//  This is a neat way to enter "boot" mode 
//  and have the Pico appear as a flash drive
//  for drag and drop programming the flash
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include <stdio.h>

// Reboot RPi is the '*' and 'D' buttons are
// both pushed
void jump_to_MSD(void)
{
	// Setup GPIO to test pression of '*' and 'D'
	sio_hw->gpio_clr=(0xFF<<5);	//
	sio_hw->gpio_oe_clr=0xFF<<5;
	sio_hw->gpio_set=0x01<<5;
	sio_hw->gpio_oe_set=0x01<<5;
	sleep_ms(1);
	// If both pressed
	if (((sio_hw->gpio_in)&(0x08<<5))==(0x08<<5)) {
		printf("Reboot to MSD\n");
		sleep_ms(2000);
		reset_usb_boot(0,0);
	}
	// Clean up if not
	sio_hw->gpio_clr=(0xFF<<5);
	sio_hw->gpio_oe_clr=0x01<<5;

	return;
}
