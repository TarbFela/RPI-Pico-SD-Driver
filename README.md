Note: NOT ALL CODE IS MINE. HAVE YET TO DO ALL OF THAT LICENSING STUFF. THIS IS NOT FOR DISTRIBUTION, BUT IS INSTEAD FOR VERSION CONTROL, REMOTE ACCESS, AND GENERAL INTEREST. 

  This is part of a project in which I aim replay .wav format (raw) audio files from a raspberry pi pico. I already got the DAC working and reading from registers. 
Now I just need to get the SD card to work in SPI mode, which is proving to be difficult. I have so far discovered that PNY Elite SD cards don't like to get into SPI mode.
I'm having better luck with a SanDisk Pro 64 GB SDXC card, though it is not responding to write or read commands after initialization. It is responding, however, to CMD58 
after initializing, though I have yet to verify that its response is cogeant with SD documentation.
