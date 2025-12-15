# ESP32-S3 Factory Program

## Overview
This is a factory test program for the ESP32-S3 development board with:
- 3.16" LCD display (320x820, RGB565)
- SD card support
- Button controls (BOOT button)
- Wi-Fi and Bluetooth scanning
- RTC and IMU sensor testing

## New Feature: SD Card Image Display

### Description
When the BOOT button is clicked in the main interface, the program will load and display an image file named `1.jpg` from the SD card.

### Usage
1. Place a JPEG image file named `1.jpg` in the root directory of the SD card (`/sdcard/1.jpg`)
2. Insert the SD card into the device
3. Power on the device and wait for the factory program to reach the main interface
4. Single-click the BOOT button to display the image
5. Single-click again to return to the factory interface

### Technical Details
- LVGL SJPG decoder is enabled for JPEG image support
- LVGL filesystem support (STDIO) is enabled with letter 'S' (ASCII 83)
- Image path: `S:/sdcard/1.jpg`
- Image display object is dynamically created on first use
- Image display toggles on/off with consecutive single clicks

### Configuration
The following settings are configured in `sdkconfig.defaults`:
- `CONFIG_LV_USE_SJPG=y` - Enable SJPG decoder for JPEG images
- `CONFIG_LV_USE_FS_STDIO=y` - Enable LVGL filesystem support
- `CONFIG_LV_FS_STDIO_LETTER=83` - Set filesystem letter to 'S' (ASCII 83)

### Button Controls
- **Single Click**: 
  - In main interface: Display/hide image from SD card
  - In other pages: Navigate through carousel pages
- **Double Click**: Toggle backlight on/off
- **Long Press**: SD card write/read test

### Requirements
- SD card must be inserted and properly formatted
- Image file must be named exactly `1.jpg`
- Image format: JPEG
- Recommended image size: 320x820 pixels (or smaller to fit screen)
