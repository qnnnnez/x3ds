X3DS
============
Use your 3DS console as a XBOX 360 controller for your PC!

Inspried by [3DSController]([https://github.com/CTurt/3DSController), X3DS is a 3DS homebrew application together with a Windows program which makes your 3DS also a wireless XBOX360 controller for your PC.

## Setup
### PC
1. Install [ViGEm Driver](https://github.com/nefarius/ViGEm/wiki/Driver-Installation).
1. Find out your PC's local IP address and remember it.
1. Make sure that your PC and your 3DS is in the same network.
1. Download the `3dsx.exe` and `3dsx.exe.config`. You can then modify the config file.
1. Run the `3dsx.exe`.

### 3DS
1. Put `x3ds.ini` to the root of your 3DS's SD card. An example configuration can be found in the 3ds/ directory of this repo. You need to modify the `x3ds.ini`, and change the `remote` to your PC's address.
1. Put the `x3ds.3dsx` file to the `/3ds` folder, and execute the homebrew from hbmenu.
1. At this moment, you should be able to see message printed out to the `3dsx.exe`'s console every second. You can now checkout the newly connected virtual controller in the Control Pannel.

## Key Map
Circle pad is mapped to left stick.
On New3DS, C-Stick is mapped to right stick, and zL, zR are mapped to trigger keys.

Touch screen is used to map the left stick key and right stick key.
If left 1/3 is pressed, left stick is pressed.
If right 1/3 is pressed, right stick is pressed.
If middle 1/3 is pressed, both left stick and right stick are pressed.
