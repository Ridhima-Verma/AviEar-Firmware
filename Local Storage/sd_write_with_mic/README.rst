/**********************************************************************/
          (Part A) AviEar: Local Storage
########################################################################

#. Configure a PDM MEMS Microphone using DMIC driver 
#. Record Audio data at sampling rate of either 8kHz and 16 kHz
#. Implement a Target Frequency Detection Algorithm
#. Estimate Mean of Magnitude of audio calls at Target Frequency
#. Configure the Sd card storage unit
#. Save the Audio Calls Locally on the Sd Card

Hardware Requirements
*********************
#. nRF5340 SoC by Nordic Semiconductor
#. MP34DT05 MEMS PDM Microphone, SD Card

Pin Configurations 
*********************
#. MEMS Microphone:
● DMIC.CLK = P0.25
● DMIC.DAT = P0.26
● GND
● VDD 3V
● LR GND

#. SD Card:
● SD.CS - 0.24
● SD.SCK - 1.15
● SD.MISO - 1.14
● SD.MOSI - 1.13
● GND
● VDD 5V

Firmware Prerequisites
**********************
#. nRF Connect for Desktop, nRF Connect SDK ncs 2.4.2 version 
#. Visual Studio Code Development Environment

Firmware Building Blocks
************************
#. src folder/main.c : Carries the code for microphone trigger, record, detection algorithm, Sd card write operation
#. Dts and dtsi files: Contains SPI configuration
#. Overlay: Customized pin configuration
#. Prj.conf: Enables the driver for the microphone

Building and Running
********************
#. Navigate to (C:\ncs\v2.4.2\zephyr\boards\arm\nrf5340dk_nrf5340)
#. Replace the given (nrf5340_cpuapp_common.dts, nrf5340_cpuapp_common-pinctrl.dtsi) files with existing files
#. Create (myApps) folder at path (C:\ncs\myApps) for your own project
#. Paste the provided folder (\sd_write_with_mic) at path (C:\ncs\myApps)
#. Open VScode and open project \sd_write_with_mic
#. Build nrf5340dk_nrf5340_cpuapp application
#. Flash the project on the development board
#. Listen to the audio calls recorded in the Sd card using Audacity Software


After flashing, the LED starts to blink. If a runtime error occurs, the sample
exits without printing to the console.

