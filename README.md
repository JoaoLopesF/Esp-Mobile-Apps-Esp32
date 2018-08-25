# Esp-Mobile-Apps
>Esp-Mobile-Apps is a complete examples to make Esp32 BLE connected devices with mobile apps (Android and iOS)

I have prepared a set of applications, to serve as a basis,
for those who need to make ble connected mobile projects with the ESP32.

* Part I    - Firmware on ESP32, can be:

    __ESP-IDF app__ - app example w/ ESP-IDF  - this github repo
    
    __Arduino app__ - app example w/ Arduino  - soon, prevision next sep 03

* Part II   - __Android app__ - mobile app example      - https://github.com/JoaoLopesF/Esp-Mobile-Apps-Android

* Part III  - __iOS app__     - mobile app example      - https://github.com/JoaoLopesF/Esp-Mobile-Apps-iOS

It is a advanced, but simple (ready to go), fully functional set of applications

![Esp-Mobile-App](https://i.imgur.com/FXOFAUC.png)

## Contents

 - [Esp32](#esp32)
 - [BLE](#ble)
 - [Part II - Android app](#part-io---android-app)
 - [Features](#features)
 - [BLE messages](#ble-messages)
 - [Structure](#structure)
 - [Prerequisites](#prerequisites)
 - [Schematics](#schematics)
 - [Install](#install)
 - [For Arduino developers](#for-arduino-developers)
 - [Researchs used](#researchs-used)
 - [To-do](#to-do)
 - [Release History](#eelease-history)

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
    Nearly 100% of devices run Android >= 4.3, and most of them should have BLE.
    For iOS, we have support for BLE, and for normal Bluetooth, only some modules with Mfi certification (made for i ...)

    So BLE is the most viable alternative for Esp32 to communicate with mobile devices.

But still have work to do with the BLE of ESP32:

    A we still do not have yet the "Low Energy" of the BLE (Bluetooth Low Energy) :-(
    Consumption is still more than 10x of competitors.
    Espressif is working on this, and in future Esp-Idf versions we will have low power in BLE (modem-sleep).

# Part I - ESP-IDF app

This app for ESP32 is for latest ESP-IDF SDK. 
And is entirely made in C++,
with the exception of the C code for the BLE connection, based on the pcbreflux code (https://github.com/pcbreflux/espressif).

Please access the www.esp32.com forum to more information about the Esp32 and ESP-IDF

## Features

This app example to Esp32, have advanced features like:

    - Implements a BLE GATT Server of type UART RX/TX to receive and send messages

    - Support for large BLE messages (it is done in C++ code)
      (if necessary, automatically send / receive in small pieces)

    - Automatic adjust of MTU (BLE package size)
      (the EspApp - mobile app, if it is possible, change it to 185 - can be up to 517)
      (For iOS, 185 is default, but not for Android) 
      (the default size of ESP-IDF is only about 20)

    - Modular and advanced programming
    - Based in mature code (I have used in Bluetooth devices and mobile apps, since years ago)
    
    - ESP32 Multicore optimization (if have 2 cores enabled)

    - Stand-by support with ESP32 deep-sleep (essential to battery powered devices)
      (by a button, or by inativity time, no touchpad yet)

    - Support for battery powered devices
      (mobile app gets status of this)
      (3 GPIO for this: external voltage, charging status and VBAT ADC voltage)

    - General utilities to use
    - Logging macros, to automatically put the function name and the core Id, to optimizations

## BLE messages

The BLE connection in this example (project) is a BLE GATT server.
It implements the exchange of messages between ESP32 and the mobile app (BLE GATT client). 

For this project and mobile app, have only text delimited based messages.

Example:

    /**
    * BLE text messages of this app
    * -----------------------------
    * Format: nn:payload
    * (where nn is code of message and payload is content, can be delimited too)
    * -----------------------------
    * Messages codes:
    * 01 Initial
    * 10 Energy status(External or Battery?)
    * 11 Informations about ESP32 device
    * 70 Echo debug
    * 80 Feedback
    * 98 Restart the ESP32
    * 99 Standby (enter in deep sleep)
    **/

If your project needs to send much data,
I recommend changing to send binary messages.

This project is for a few messages per second (less than 20).
It is more a mobile app limit (more to Android).

If your project need send more, 
I suggest you use a FreeRTOS task to agregate data after send.
(this app supports large messages)

## Structure

Modules of esp-idf example aplication

```
    - EspApp                   - The ESP-IDF application
        
        - main                    - main directory of esp-idf
            
            - util                  - utilities 
                - ble_server.*      - ble server C++ wrapper class to ble_uart_server (in C)
                - ble_uart_server.* - code in C, based on @pcbreflux code
                - esp_util.*        - general utilities
                - fields.*          - class to split text delimited in fields
                - log.h             - macros to improve esp-idf logging
                - median_filter.h   - running median filter to ADC readings
            
            - ble.*                 - ble code of project (uses ble_server and callbacks)

            - main.*                - main code of project

            - peripherals.*         - code to treat ESP32 peripherals (GPIOs, ADC, etc.)

    - Extras                 - extra things, as VSCode configurations
```

Generally you do not need to change anything in the util directory. 
If you need, please add a Issue or a commit, to put it in repo, to help a upgrades in util

But yes in the other files, to facilitate, I put a comment "// TODO: see it" in the main points, that needs to see. so to start just find it in your IDE.

## Prerequisites 

Is a same for any project with Esp32:

* A development kit board with Esp32. 

    - Can be ESP-WROVER-KIT, ESP32-Pico-Kit, DevKitC, Lolin32, etc.
    - Or a custom hardware too, with any Esp32 module.

* USB driver of this board installed
    
* Latest esp-idf SDK installed (https://github.com/espressif/esp-idf)
  See info in https://esp-idf.readthedocs.io/en/latest/get-started/index.html#get-started-get-esp-idf  

* An configured C/C++ IDE (recommended). 
        
    I suggest Eclipse CDT or VSCode:

    - Eclipse CDT: powerfull but high memory
    - VSCode: rich and lightweight, but refactoring is poor than eclipse

    I use both, just editing code I use VSCode, if I need more, as better refactoring, I use Eclipse

## Schematics 

Need to standby or battery powered device.

Note: this is disabled by default
If your project need this, please:

    - Edit main.h and enable HAVE_BATTERY and/or HAVE_STANDBY
    - Edit peripherals.h 
    - See notes
    - Enable features (GPIO pin can be changed)
    - For battery powered, I suggest modify ESP32 CPU freq. to 80 MHz (make config)
    - The charging sensor is experimental and only tested on TP4054 charging chip 

Schematics:

![schematics](https://i.imgur.com/CsVax2f.png)

It is made in EasyEDA, and I made a public project in: https://easyeda.com/JoaoLopesF/esp-idf-mobile-app-v1-0

## Install

To install, just download or use the "Github desktop new" app to do it (is good to updatings).

After open it in your IDE

Please find all __"TODO: see it"__ occorences in all files, it needs your attention

And enjoy :-)

## For Arduino developers

I will made this too Arduino developers in future,
after I finished Part II and III (mobile app samples).

## Feedback and contribution

If you have a problem, bug, sugesttions to report,
 please do it in GitHub Issues.

Feedbacks and contributions is allways well come

Please give a star to this repo, if you like this.

## To-do

* Documentation (doxygen)
* Tutorial (guide)
* Arduino version
* Revision of translate to english (typing errors or mistranslated)

## Researchs used 

* ESP-IDF documentation (https://esp-idf.readthedocs.io/en/latest/) and examples
* Esp32 forum (https://www.esp32.com)
* ESP32 Book and esp32-snippets by Neil Kolban

* __ble_uart_server__ example code. 
 Thanks a lot @pcbreflux, it works very well with mobile apps
    
## Release History

* 0.2.0 - 20/08/18
    * Option to disable logging br BLE (used during repeated sends)
    
* 0.1.0
    * First version
