# AviEar-Firmware for Avian Species Acoustic Monitoring

Overview
********
The developed AviEar system focuses on monitoring avian activity through acoustic data collection and transmission. The firmware includes source code for local storage of recorded audio and device-to-cloud transfer via MQTT. Bird calls are recorded, and the magnitude of the target frequency is estimated using a detection algorithm. The recorded calls are stored on an SD card and can be accessed remotely via the AWS IoT Cloud platform.

/********************* (Part A) Local Storage*****************/

#. Configure a PDM MEMS Microphone using DMIC driver 

#. Record Audio data at sampling rate of either 8kHz and 16 kHz

#. Implement a Target Frequency Detection Algorithm

#. Estimate Mean of Magnitude of audio calls at Target Frequency

#. Configure the Sd card storage unit

#. Save the Audio Calls Locally on the Sd Card

-------------------------Hardware Requirements--------------------------
************************************************************************
#. nRF5340 SoC by Nordic Semiconductor

#. MP34DT05 MEMS PDM Microphone, SD Card

-----------------------------Pin Configurations-------------------------
************************************************************************
#. MEMS Microphone:

● DMIC.CLK = P1.7
● DMIC.DAT = P1.9
● GND
● VDD 3V
● LR GND

#. SD Card:

● SD.CS - P0.24
● SD.SCK - P0.05
● SD.MISO - P0.27
● SD.MOSI - P0.04
● GND
● VDD 5V

--------------------------Firmware Prerequisites----------------------
**********************************************************************
#. nRF Connect for Desktop, nRF Connect SDK ncs 2.4.2 version 

#. Visual Studio Code Development Environment

----------------------Firmware Building Blocks----------------------
********************************************************************
#. src folder/main.c : Carries the code for microphone trigger, record, detection algorithm, Sd card write operation

#. dts and dtsi files: Contains SPI configuration

#. Overlay: Customized pin configuration

#. Prj.conf: Enables the driver for the microphone

----------------------Building and Running-------------------------
*******************************************************************
#. Navigate to (C:\ncs\v2.4.2\zephyr\boards\arm\nrf5340dk_nrf5340)

#. Replace the given (nrf5340_cpuapp_common.dts, nrf5340_cpuapp_common-pinctrl.dtsi) files with existing files

#. Create (myApps) folder at path (C:\ncs\myApps) for your own project

#. Paste the provided folder (\sd_write_with_mic) at path (C:\ncs\myApps)

#. Open VScode and open project \sd_write_with_mic

#. Build nrf5340dk_nrf5340_cpuapp application

#. Flash the project on the development board

#. Listen to the audio calls recorded in the Sd card using Audacity Software

/***********(Part B.1) Device to Cloud Publish Immediately **************/

-----------------------------------Approach 1--------------------------
***********************************************************************
Record the audio, implement detection algorithm, save in sd card, publish to AWS. The cycle repeats for each incoming audio.

#. Configure a PDM MEMS Microphone using DMIC driver and record Audio data at sampling of 8kHz and 16 kHz

#. Implement a Target Frequency Detection Algorithm

#. Estimate Mean of Magnitude of calls at Target Frequency

#. Configure the Sd card storage unit

#. Connect with AWS IoT Cloud

#. Publish recorded audio data to AWS Cloud for remote monitoring

------------------------------Hardware Requirements----------------------
*************************************************************************
#. nRF5340 SoC by Nordic Semiconductor
#. MP34DT05 MEMS Microphone, SD Card, EC200U Quectel modem, Sim Card

------------------------------Pin Configurations------------------------ 
************************************************************************
#. MEMS Microphone:

● DMIC.CLK = P1.7
● DMIC.DAT = P1.9
● GND
● VDD 3V
● LR GND

#. SD Card:

● SD.CS - P0.24
● SD.SCK - P0.05
● SD.MISO - P0.27
● SD.MOSI - P0.04
● GND
● VDD 5V

----------------------------Firmware Prerequisites------------------------
**************************************************************************
#. nRF Connect for Desktop, nRF Connect SDK ncs 2.4.2 version 

#. Visual Studio Code Development Environment

#. Amazon Web Service Cloud MQTT Publish Topic, Endpoint IoT Core, Thing Name, Modem APN

#. Generate the Certificates for the Thing and put in creds folder and run the convert_keys.py to generate keys


----------------------------Firmware Building Blocks------------------------
****************************************************************************
#. src folder/main.c : Carries all the functions of the actual code for operation

#. src folder/main_audio.c : Carries the code for microphone trigger, record, detection algorithm, Sd card write operation

#. src folder/main_aws.c : Carries the actual code for setting AWS credentials, configure MQTT client Connect and Publish operation

#. src folder/main_gsm.c : Carries the actual code for GSM Network Connection 

#. dts and dtsi files: Contains SPI configuration

#. Overlay: Customized pin configuration

#. Prj.conf: Enables the driver for the microphone

--------------------------------Building and Running----------------------------
********************************************************************************

#. Navigate to (C:\ncs\v2.4.2\zephyr\boards\arm\nrf5340dk_nrf5340)

#. Replace the nrf5340 cpuapp common.dts and nrf5340 cpuapp common-pinctrl.dtsi files with the existing ones

#. Create (myApps) folder at path (C:\ncs\myApps) for your own project

#. Paste the provided folder (\Audio_cloud_final) at path (C:\ncs\myApps)

#. Open VScode and open project \Audio_cloud_final

#. Replace main audio.c in the src folder with the existing one

#. Build nrf5340dk_nrf5340_cpuapp application 

#. View and listen the data recorded on the respective Topic at MQTT Test Client and S3 bucket corresponding to the Device Id

After flashing, the LED starts to blink. If a runtime error occurs, the sample
exits without printing to the console.

/************** (Part B.2) Device to Cloud Publish After Pre-defined Interval ***************/
 
-----------------------------------Approach 2--------------------------
***********************************************************************
Record the audio, implement detection algorithm, save in Sd card. This cycle repeats for each incoming audio.
After a user defined interval the device will Publish the recorded data all together. This is done to save power
due to frequent connection setups.
