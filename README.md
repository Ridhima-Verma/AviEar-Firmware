# AviEar-Firmware for Avian Species Acoustic Monitoring

Overview
********
The project includes source code for Local Storage of recorded audio and Device-to-Cloud transfer via MQTT.

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

● DMIC.CLK = P1.7
● DMIC.DAT = P1.9
● GND
● VDD 3V
● LR GND

#. SD Card:

● SD.CS - 0.24
● SD.SCK - 0.05
● SD.MISO - 0.27
● SD.MOSI - 0.04
● GND
● VDD 5V

Firmware Prerequisites
**********************
#. nRF Connect Sdk ncs 2.4.2 version 
#. Visual Studio Code Development Environment

Firmware Building Blocks
************************
#. src folder/main.c : Carries the code for microphone trigger, record, detection algorithm, Sd card write operation
#. Dts and dtsi files: Contains default pin configuration
#. Overlay: Customized pin configuration
#. Prj.conf: Enables the driver for the microphone

Building and Running
********************
#. Navigate to (C:\ncs\v2.4.0\zephyr\boards\arm\nrf5340dk_nrf5340)
#. Replace the given (nrf5340_cpuapp_common.dts, nrf5340_cpuapp_common-pinctrl.dtsi) files with existing files
#. Create (myApps) folder at path (C:\ncs\myApps) for your own project
#. Paste the provided folder (\sd_write_with_mic) at path (C:\ncs\myApps)
#. Open VScode and open project \sd_write_with_mic
#. Build nrf5340dk_nrf5340_cpuapp application
#. Flash the project on the development board
#. Listen to the audio calls recorded in the Sd card using Audacity Software

/******************************************************************/
     (Part B.1) AviEar: Device to Cloud Publish Immediately 
####################################################################

Approach 1
*************
Record the audio, implement detection algorithm, save in sd card, publish to AWS. The cycle repeats for each incoming audio.

#. Configure a PDM MEMS Microphone using DMIC driver and record Audio data at sampling of 8kHz and 16 kHz
#. Implement a Target Frequency Detection Algorithm
#. Estimate Mean of Magnitude of calls at Target Frequency
#. Configure the Sd card storage unit
#. Connect with AWS IoT Cloud
#. Publish recorded audio data to AWS Cloud for remote monitoring

Hardware Requirements
*********************
#. nRF5340 SoC by Nordic Semiconductor
#. MP34DT05 MEMS Microphone, SD Card, EC200U Quectel modem, Sim Card

Pin Configurations 
*********************
#. MEMS Microphone:

● DMIC.CLK = P1.7
● DMIC.DAT = P1.9
● GND
● VDD 3V
● LR GND

#. SD Card:

● SD.CS - 0.24
● SD.SCK - 0.05
● SD.MISO - 0.27
● SD.MOSI - 0.04
● GND
● VDD 5V

Firmware Prerequisites
**********************
#. nRF Connect Sdk ncs 2.4.2 version toolchain
#. Visual Studio Code
#. Amazon Web Service Cloud MQTT Publish Topic, Endpoint IoT Core, Thing Name, Modem APN
#. Generate the Certificates for the Thing and put in creds folder and run the convert_keys.py to generate keys


Firmware Building Blocks
************************
#. src folder/main.c : Carries all the functions of the actual code for operation
#. src folder/main_audio.c : Carries the code for microphone trigger, record, detection algorithm, Sd card write operation
#. src folder/main_aws.c : Carries the actual code for setting AWS credentials, configure MQTT client Connect and Publish operation
#. src folder/main_gsm.c : Carries the actual code for GSM Network Connection operation
#. Dts and dtsi files: Contains default pin configuration
#. Overlay: Customized pin configuration
#. Prj.conf: Enables the driver for the microphone

Building and Running
********************

#. Navigate to (C:\ncs\v2.4.0\zephyr\boards\arm\nrf5340dk_nrf5340)
#. Replace the given (nrf5340_cpuapp_common.dts,nrf5340_cpuapp_common-pinctrl.dtsi, main_audio.c) files with existing files
#. Create (myApps) folder at path (C:\ncs\myApps) for your own project
#. Paste the provided folder (\Audio_cloud_final) at path (C:\ncs\myApps)
#. Open VScode and open project \Audio_cloud_final
#. Build nrf5340dk_nrf5340_cpuapp application 
#. View and listen the data recorded on the respective Topic at MQTT Test Client and S3 bucket corresponding to the Device Id

After flashing, the LED starts to blink. If a runtime error occurs, the sample
exits without printing to the console.

/**********************************************************************/
  (Part B.2)AviEar: Device to Cloud Publish After Pre-defined Interval 
########################################################################

Approach 2
*************
Record the audio, implement detection algorithm, save in Sd card. This cycle repeats for each incoming audio.
After a user defined interval the device will Publish the recorded data all together. This is done to save power
due to frequent connection setups.
