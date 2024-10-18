import boto3
from pydub import AudioSegment
import os
import base64
from base64 import b64decode
import datetime

def download_file_from_s3(bucket_name, key, local_path):
    s3 = boto3.client('s3')
    try:
        s3.download_file(bucket_name, key, local_path)
    except NoCredentialsError as e:
        print(f"Error: {e}")

def convert_to_mp3(input_file_path, output_file_path):
    # Check if the file is already in MP3 format
    if input_file_path.lower().endswith(".raw"):
        raw_audio = AudioSegment.from_raw(input_file_path, sample_width=2, frame_rate=44100, channels=2)
        raw_audio.export(output_file_path, format="mp3")

    return output_file_path

def upload_file_to_s3(bucket_name, key, local_path):
    s3 = boto3.client('s3')
    try:
        s3.upload_file(local_path, bucket_name, key)
    except NoCredentialsError as e:
        print(f"Error: {e}")

def convert_to_raw(mp3_file_path, raw_file_path):
    mp3_audio = AudioSegment.from_file(mp3_file_path, format="mp3")
    
    # Ensure the sample width is 2 (16-bit PCM), sample rate is 44100 Hz, and channels are 2 (stereo)
    mp3_audio = mp3_audio.set_sample_width(2)
    mp3_audio = mp3_audio.set_frame_rate(44100)
    mp3_audio = mp3_audio.set_channels(2)
    
    # Export as raw audio
    mp3_audio.export(raw_file_path, format="raw")

def time_conversion(timestamp):
    output_string = ""
    for i, char in enumerate(timestamp):
        if i % 2 != 0:
            output_string += char
    print(output_string)
    if all(char == '0' for char in output_string):
        standard_time = datetime.datetime.now()
        time_difference = datetime.timedelta(hours=5, minutes=30)
        standard_time = standard_time + time_difference
    else:
        standard_time = datetime.datetime.fromtimestamp(int(output_string))
        time_difference = datetime.timedelta(hours=5, minutes=30)
        standard_time = standard_time + time_difference

    formatted_time = standard_time.strftime("%d-%m-%Y_%H-%M-%S")
    print(formatted_time)
    return formatted_time

#/*Lambda For RAW audio file to .mp3 conversion*/
def lambda_handler(event, context):
    try:
        s3_bucket_name = 'birdnet-sagemaker'
        message = event['payload']
        decoded_bytes = b64decode(message)
        base16_data = decoded_bytes.hex()
        # Decode the Base16 data to binary
        
        info_part = base16_data[0:32]
        data_part = base16_data[32:]
        
        device_id = info_part[0:2]
        timestamp = info_part[2:22]
        binary_data = base64.b16decode(data_part.upper())
        
        audio = AudioSegment(
            data=binary_data,
            sample_width=2,  # 16-bit
            frame_rate=8000,
            channels=1
        )
        
        # Export the audio to an MP3 file
        audio.export("/tmp/converted_audio.mp3", format="mp3")
        
        s3_key = device_id + "_" + time_conversion(timestamp) + ".mp3"
        
        upload_file_to_s3(s3_bucket_name,s3_key ,'/tmp/converted_audio.mp3')
    
    except Exception as e:
        # Print the error message
        print(f"Error: {e}")