
/**********************************************************************/
  (Part B.2) AviEar: Device to Cloud Publish After Pre-defined Interval 
########################################################################

Approach 2
*************
Record the audio, implement detection algorithm, save in Sd card. This cycle repeats for each incoming audio.
After a user defined interval the device will Publish the recorded data all together. This is done to save power
due to frequent connection setups.
