# Esp-Idf-Mobile-App
>Esp-Idf-Mobile-App is a complete examples to make Esp32 BLE connected devices with mobile apps (Android and iOS)

The Esp32 is a powerful board with 2 cores, 520K RAM, 34 GPIO, 3 UART,
Wifi and Bluetooth Dual Mode. And all this at an excellent price.

Esp-IDF is very good SDK, to developer Esp32 projects.
With Free-RTOS (with multicore), WiFi, BLE, plenty of GPIOs, peripherals support, etc.

With Esp32, we can develop, in addition to WIFI applications (IoT, etc.),
mobile applications with ble connection with it.

BLE is suitable for connection to Android and iOS.
Nearly 100% of devices run Android >= 4.0.3, and most of them should have BLE
For iOS, we have support for BLE, and for normal Bluetooth, only some modules with Mfi certification (made for i ...)

So BLE is the most viable alternative for Esp32 to communicate with mobile devices.

But still have work to do with the BLE of ESP32.
Espressif still do not have a mobile app samples for the ESP32.
A we still do not have yet the "Low Energy" of the BLE (Bluetooth Low Energy) :-(
Consumption is still more than 10x of competitors.
Espressif is working on this, and in future Esp-Idf versions we will have this working.

So far, for anyone who ventures to make mobile applications (Android and iOS),
with the ESP32, there was not something ready for the beginning of the development.
Ready I say, sample apps working with BLE for ESP32, Android and iOS.

For this I have prepared a set of applications, to serve as a basis,
for those who need to make ble connected mobile projects with the ESP32.

    - Part I - Esp-IDF app - Esp32 firmware example - https://github.com/JoaoLopesF/Esp-Idf-Mobile-App-Esp32
    - Part II - Android app - mobile app example
    - Part III - iOS app - mobile app example

# Part I - Esp-IDF app example

This example app for ESP32 is for latest esp-idf SDK. 
And is entirely made in C++,
with the exception of the C code for the BLE connection, based on the pcbreflux code (https://github.com/pcbreflux/espressif).

Please access the www.esp32.com forum to more information about the Esp32 and esp-idf

## Features

This app example to Esp32, have advanced features like:

- Support for large BLE messages (it is done in C++ code)
  (if necessary, automatically send / receive in small pieces)
  (the default size is only about 20)
- Automatic adjust of MTU (BLE package size)
   (the mobile app, if it is possible, change it to 185 - can be up to 517)
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

Message format: nn:payload
    Where   nn is a message code
    and     payload is a content of message
            (can be have more than one information, with delimiter fields)
Example:

    /** 
    * Bluetooth messages 
    * ----------------------
    * Format: nn:payload (where nn is code of message and payload is content, can be delimited too) 
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
I suggest you use a task to agregate data after send.
(this app supports large messages)

## Structure

Modules of esp-idf example aplication

 -  main                    - main directory of esp-idf
    
    - util                  - utilities 
        - ble_server.*      - ble server C++ wrapper class to ble_uart_server (in C)
        - ble_uart_server.* - code in C, based on pcbreflux code
        - esp_util.*        - general utilities
        - fields.*          - class to split text delimited in fields
        - log.h             - macros to improve esp-idf logging
        - median_filter.h   - running median filter to ADC readings
    
    - ble.*                 - ble code of project (uses ble_server and callbacks)

    - main.*                - main code of project

    - peripherals           - code to treat ESP32 peripherals

Generally you do not need to change anything in the util directory. 

But yes in the other files, to facilitate, I put a comment "// TODO: see it" in the main points, that needs to see. so to start just find it in your IDE.

## Prerequisites 

Is a same for any project with Esp32:

    - A development kit board with Esp32. 
        Can be ESP-WROVER-KIT, DevKitC, lolin32, etc.
    - USB driver of this board installed
    - Latest esp-idf SDK installed 
    - An configured C/C++ IDE (recommended). 
        I suggest Eclipse CDT or VSCode

        *** Experimental: The project already has preconfigurations for 2 IDEs:

        - Eclipse CDT: powerfull but high memory
         
        - VSCode: powerful, lightweight and expandable, but refactoring is poor than eclipse

        Please open and edit files to configure:

          - Eclipse CDT: files: .project, .cproject and .settings/*
          - VSCODE: files: .vscode/*

        *** Not yet full tested

## Install

To install, just download or use the "Github desktop new" app to do it (is good to updatings).

After please find all "TODO: see it" occorences

And enjoy :-)

## Feedback and contribution

If you have a problem, bug, sugesttions to report,
 please do it in GitHub Issues.

Feedbacks and contributions is allways well come

## Release History

* 0.1.0
    * First version


