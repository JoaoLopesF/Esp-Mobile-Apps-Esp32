# Esp-Idf-Mobile-App
>Esp-Idf-Mobile-App is a complete examples to make Esp32 BLE connected devices with mobile apps (Android and iOS)

I have prepared a set of applications, to serve as a basis,
for those who need to make ble connected mobile projects with the ESP32.

    - Part I    - Esp-IDF app - Esp32 firmware example - https://github.com/JoaoLopesF/Esp-Idf-Mobile-App-Esp32
    - Part II   - Android app - mobile app example - soon
    - Part III  - iOS app - mobile app example - soon

So far, for anyone who ventures to make mobile applications (Android and iOS),
with the ESP32, there was not yet, something ready for the beginning of the development.
Ready I say, sample apps working with BLE for ESP32, Android and iOS.

For this I made this.

## Esp32

The Esp32 is a powerful board with 2 cores, 520K RAM, 34 GPIO, 3 UART,
Wifi and Bluetooth Dual Mode. And all this at an excellent price.

Esp-IDF is very good SDK, to developer Esp32 projects.
With Free-RTOS (with multicore), WiFi, BLE, plenty of GPIOs, peripherals support, etc.

## BLE

With Esp32, we can develop, in addition to WIFI applications (IoT, etc.),
devices with Bluetooth connection for mobile applications.

BLE is a Bluetooth Low Energy:

    BLE is suitable for connection to Android and iOS.
    Nearly 100% of devices run Android >= 4.0.3, and most of them should have BLE.
    For iOS, we have support for BLE, and for normal Bluetooth, only some modules with Mfi certification (made for i ...)

    So BLE is the most viable alternative for Esp32 to communicate with mobile devices.

But still have work to do with the BLE of ESP32:

    - Espressif still do not have a mobile app samples for the ESP32.
    - A we still do not have yet the "Low Energy" of the BLE (Bluetooth Low Energy) :-(
    
    Consumption is still more than 10x of competitors.
    Espressif is working on this, and in future Esp-Idf versions we will have this working.

# Part I - Esp-IDF app example

This example app for ESP32 is for latest esp-idf SDK. 
And is entirely made in C++,
with the exception of the C code for the BLE connection, based on the pcbreflux code (https://github.com/pcbreflux/espressif).

Please access the www.esp32.com forum to more information about the Esp32 and ESP-IDF

## For Arduino developers

This ESP-IDF project has adapted to works in Arduino too (see below)

## Features

This app example to Esp32, have advanced features like:

    - Support for large BLE messages (it is done in C++ code)
      (if necessary, automatically send / receive in small pieces)
    - Automatic adjust of MTU (BLE package size)
       (the mobile app, if it is possible, change it to 185 - can be up to 517)
      (the default size is only about 20)
    - Modular programming
    - Multicore optimization (if have 2 cores enabled)
    - Stand-by support with ESP32 deep-sleep
      (by a button, or by inativity time, no touchpad yet)
    - Support for battery powered devices
      (mobile app gets status of this)
      (2 GPIO for this: one for VUSB, to know if it is charging and another for VBAT voltage)
    - Logging macros, to automatically put the function name and the core Id, to optimizations

## BLE messages

The BLE connection in this example (project) is a BLE GATT server.
It implements the exchange of messages between ESP32 and the mobile app (BLE GATT client). 

For this project and mobile app, have only text delimited based messages.

Example:

    /** 
    * Bluetooth messages 
    * ----------------------
    * Format: nn:payload 
    * (where nn is code of message and payload is content, can be delimited too) 
    * --------------------------- 
    * Messages codes:
    * 01 Start 
    * 02 Version 
    * 03 Power status(USB or Battery?) 
    * 70 Echo debug
    * 80 Feedback 
    * 98 Reinitialize
    * 99 Standby (enter in deep sleep)
    *
    **/

If your project needs to send much data,
I recommend changing the codes to send binary messages.

This project is for a few messages per second.
It is more a mobile app limit (more to Android).

If your project need send more, 
I suggest you use a FreeRTOS task to agregate data after send.
(this app supports large messages)

## Structure

Modules of esp-idf example aplication

 - main                    - main directory of esp-idf
    
    - util                  - utilities 
        - ble_server.*      - ble server C++ wrapper class to ble_uart_server (in C)
        - ble_uart_server.* - code in C, based on pcbreflux code
        - esp_util.*        - general utilities
        - fields.*          - class to split text delimited in fields
        - log.h             - macros to improve esp-idf logging
        - median_filter.h   - running median filter to ADC readings
    
    - ble.*                 - ble code of project (uses ble_server and callbacks)

    - main.*                - main code of project

    - peripherals.*         - code to treat ESP32 peripherals (GPIOs, ADC, etc.)

 - extras                 - extra things, as Arduino code and VSCode configurations

Generally you do not need to change anything in the util directory. 
If you need, please add a Issue or a commit, to put it in repo, to help a upgrades in util

But yes in the other files, to facilitate, I put a comment "// TODO: see it" in the main points, that needs to see. so to start just find it in your IDE.

## Prerequisites 

Is a same for any project with Esp32:

    - A development kit board with Esp32. 
        Can be ESP-WROVER-KIT, DevKitC, Lolin32, etc.
        Can be a custom hardware too, with any Esp32 module.

    - USB driver of this board installed
    
    - Latest esp-idf SDK installed 
    
    - An configured C/C++ IDE (recommended). 
        
        I suggest Eclipse CDT or VSCode:

            - Eclipse CDT: powerfull but high memory
            - VSCode: rich and lightweight, but refactoring is poor than eclipse

        I use both, just editing code I use VSCode, if I need more, as better refactoring, I use Eclipse


## Install

To install, just download or use the "Github desktop new" app to do it (is good to updatings).

After please find all _"TODO: see it"_ occorences

And enjoy :-)

## For Arduino developers

Just install (see above) and copy the _main_ directory to you Arduino project
(I suggest that you create a empty Arquido project to initial testings)

Edit your _.ino_ file and put this code

## Feedback and contribution

If you have a problem, bug, sugesttions to report,
 please do it in GitHub Issues.

Feedbacks and contributions is allways well come

Please give a star to this repo, if you like this.

## Release History

* 0.1.0
    * First version
