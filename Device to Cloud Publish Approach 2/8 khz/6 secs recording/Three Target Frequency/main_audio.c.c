/*Audio to Cloud via SD Card*/
#include "headers/main_aws.h"
#include "headers/main_audio.h"
#include "headers/main_gsm.h"
#include "headers/main_epoch.h"
#include <zephyr/devicetree.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

// For audio
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/audio/dmic.h>
#include <complex.h>

// MUTEX flash lock read / write
extern struct k_mutex flash_mutex;

const struct device *flash_dev;

LOG_MODULE_REGISTER(audio_start);

// Audio Fetching Variable Declaration --------------------------------------------------

#define PI 3.14159265358979323846
#define AUDIO_FREQ 8000 // for 8 kHz
#define CHAN_SIZE 16
#define PCM_BLK_SIZE_MS ((AUDIO_FREQ / 1000) * sizeof(int16_t))
#define NUM_MS 6000
K_MEM_SLAB_DEFINE(rx_mem_slab, PCM_BLK_SIZE_MS, NUM_MS + 1, 1);
K_MEM_SLAB_DEFINE(rx_temp_slab, 4, 4, 4);
void *rx_block[NUM_MS];
size_t rx_size = PCM_BLK_SIZE_MS;
extern int write_index;
int audio_info_len = 16;
extern int DEV_ID;
#define READ_TIMEOUT_MS 2000
#define TARGET_FREQUENCY1 1600
#define TARGET_FREQUENCY2 2000
#define TARGET_FREQUENCY3 3000
#define HAMMING_WINDOW_BLOCK_SIZE 128
// Goertzel Detection Algorithm Variable Declaration -------------------------------------

double magnitude1 = 0.0, mean1 = 0.0;
double magnitude2 = 0.0, mean2 = 0.0;
double magnitude3 = 0.0, mean3 = 0.0;
double threshold_mean =  45;
#define AUDIO_FRAGMENT 96000

#define SPI_FLASH_SECTOR_SIZE 4096
#define READ_ADDRESS 4096
#define WRITE_ADDRESS 8192
extern int max_write_index;

//Pre-defined Timer******************************************************************
int64_t timeInterval = 0;
extern int startTime;


// SD params -----------------------------------

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */
static const char *disk_mount_pt = "/SD:";

// Hamming Window Smoothning Function ------------------------------------------------------

static float hamming_window(size_t i)
{
	const float alpha = 0.53836;
	const float beta = 1.0 - alpha;
	float h = (alpha - beta * cosf((2 * PI * i) / (HAMMING_WINDOW_BLOCK_SIZE - 1)));
	return h;
}

//Detection Algorithm Function ------------------------------------------------------------

static void goertzel(const int16_t *pcm_data[], double *mean1_out, double *mean2_out, double *mean3_out)
{
  printk("Entering goertzel function\n");
    const float sampling_rate = (float)AUDIO_FREQ;
    const float omega1 = (2.0 * PI * TARGET_FREQUENCY1) / sampling_rate;
    const float omega2 = (2.0 * PI * TARGET_FREQUENCY2) / sampling_rate;
    const float omega3 = (2.0 * PI * TARGET_FREQUENCY3) / sampling_rate;
	

	//for current, previous and previous than previous value of q
    float q0 = 0.0, q1 = 0.0, q2 = 0.0;
    float cosine1 = cosf(omega1);
	
    float q0_2 = 0.0, q1_2 = 0.0, q2_2 = 0.0;
    float cosine2 = cosf(omega2);

    float q0_3 = 0.0, q1_3 = 0.0, q2_3 = 0.0;
    float cosine3 = cosf(omega3);

  	

	for (size_t i = 0; i < NUM_MS; i++)
	{
		float h = 0.0;
		//Goertzel
		q1 = 0.0;
		q2 = 0.0;

     // Goertzel for the second frequency
        q1_2 = 0.0;
        q2_2 = 0.0;


    // Goertzel for the third frequency
        q1_3 = 0.0;
        q2_3 = 0.0;

		int16_t data = 0;
		for (size_t j = i; j < i + HAMMING_WINDOW_BLOCK_SIZE / 8; j++)
		{
			for (size_t k = 0; k < 8; k++)
			{
				h = hamming_window((j - i) * 8 + k + 1);
				// Extracting data from audio block
				data = pcm_data[j][k];
				q0 = h*(float)data + 2.0 * cosine1 * q1 - q2;
				q2 = q1;
				q1 = q0;
				//printk("q0:%f, q1:%f, q2:%f, Data:%d\n", q0, q1, q2, data);

                // Goertzel calculation for the second frequency
                q0_2 = h*(float)data + 2.0 * cosine2 * q1_2 - q2_2;
                q2_2 = q1_2;
                q1_2 = q0_2;


               // Goertzel calculation for the third frequency
                q0_3 = h*(float)data + 2.0 * cosine3 * q1_3 - q2_3;
                q2_3 = q1_3;
                q1_3 = q0_3;

			}
		}
		// Magnitude calculation
   		// magnitude = sqrtf(q0 * q0 + q1 * q1 - 2.0 * q0 * q1 * cosine);
		magnitude1 = cabsf(q1 - cexp(-I * 2 * PI * TARGET_FREQUENCY1/sampling_rate) * q2);
		mean1 += magnitude1;
		// Magnitude calculation for the second frequency
        magnitude2 = cabsf(q1_2 - cexp(-I * 2 * PI * TARGET_FREQUENCY2/sampling_rate) * q2_2);
        mean2 += magnitude2;

       // Magnitude calculation for the third frequency
        magnitude3 = cabsf(q1_3 - cexp(-I * 2 * PI * TARGET_FREQUENCY3/sampling_rate) * q2_3);
        mean3 += magnitude3;

		i += 15;
	}
	mean1  = 20 * log10(mean1/375);
	mean2  = 20 * log10(mean2/375);
	mean3  = 20 * log10(mean3/375);
	
	printk("Mean for frequency 1 is %f\n", mean1);
	printk("Mean for frequency 2 is %f\n", mean2);
    printk("Mean for frequency 3 is %f\n", mean3);
//	k_sleep(K_MSEC(100));
	// Assign means to output pointers
    *mean1_out = mean1;
    *mean2_out = mean2;
    *mean3_out = mean3; //Mean magnitude in dB
    
}

void audio_start(void)
{
	int rc;
	const struct device *flash_dev;
	flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
	// Audio ------------------
	int i;
	uint32_t ms;
	int ret;

	const struct device *const mic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));

    /* Verify MEMS Microphone Availability -------------------------------------------------*/

	if (!device_is_ready(mic_dev))
	{
		LOG_ERR("%s: device not ready.\n", mic_dev->name);
		return 0;
	}

	struct pcm_stream_cfg stream = {
		.pcm_width = CHAN_SIZE,
		.mem_slab = &rx_mem_slab,
	};

	struct dmic_cfg cfg = {
		.io = {
			.min_pdm_clk_freq = 512000, // For 8kHz = 64(decimation ratio)*8
			.max_pdm_clk_freq = 640000, // For 8kHz = 80(decimation ratio)*8
			.min_pdm_clk_dc = 40,
			.max_pdm_clk_dc = 60,
		},
		.streams = &stream,
		.channel = {
			.req_num_streams = 1,
		},
	};

	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo =
		dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	cfg.streams[0].pcm_rate = AUDIO_FREQ;
	cfg.streams[0].block_size = PCM_BLK_SIZE_MS;

	ret = dmic_configure(mic_dev, &cfg);
	if (ret < 0)
	{
		LOG_ERR("microphone configuration error\n");
		return 0;
	}
	else
	{
		LOG_INF("microphone configuration success\n");
	}

	for (;;)
	{
		int res;
		int epoch_array[10] = {0};
		uint8_t audio_info_part[16] = {0};
		/* Audio get data ---------------------------------------------------------------*/

		ret = dmic_trigger(mic_dev, DMIC_TRIGGER_START);
		if (ret < 0)
		{
			LOG_ERR("microphone start trigger error\n");
			return 0;
		}
		else
		{
			LOG_INF("microphone start trigger success\n");
		}
		/* Acquire microphone audio ----------------------------------------------------*/
		printf("Recording...\n");
		for (ms = 0; ms < NUM_MS; ms++)
		{
			ret = dmic_read(mic_dev, 0, &rx_block[ms], &rx_size, READ_TIMEOUT_MS);
			if (ret < 0)
			{
				LOG_ERR("%d microphone audio read error %p %u.\n", ms, rx_block[ms], rx_size);
				return 0;
			}
		}

double mean1, mean2, mean3;
goertzel(rx_block, &mean1, &mean2, &mean3);	
/* Compare the Magnitude Mean from Detection Algorithm with Threshold value --------*/

	if (mean1 > threshold_mean || mean2 > threshold_mean || mean3 > threshold_mean) 
		{
		
			printk("Attempting to write to file...\n");
			char filename[30];

			struct fs_file_t filep;
			size_t bytes_written;

			fs_file_t_init(&filep);
            /*Write the Recorded Audio to Sd Card storage unit --------------------------------*/
			sprintf(&filename, "/SD:/%d.raw", write_index);

			fs_unlink(filename);

			res = fs_open(&filep, filename, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
			if (res)
			{
				printk("Error opening file %s [%d]\n", filename, res);
				// return;
			}
			else
			{
				printk("File %s opened successfully\n", filename);
				setting_epoch();
				get_epoch(epoch_array);
				/*Append Device ID and Epoch Timestamp --------------------------------------------*/
				audio_info_part[0] = DEV_ID;
				for (int epoch_count = 0; epoch_count < 10; epoch_count++)
				{
					audio_info_part[epoch_count + 1] = epoch_array[epoch_count];
				}
				fs_write(&filep, &audio_info_part, sizeof(audio_info_part));
			}

/*Identify the PCM samples recorded --------------------------------------------*/

			for (i = 0; i < NUM_MS; i++)
			{
				uint32_t *pcm_out = rx_block[i];
				for (int j = 0; j < rx_size / 4; j++)
				{
					// printk("Writing 0x%8x, \n", pcm_out[j]);

					res = fs_write(&filep, &pcm_out[j], sizeof(uint32_t));
					if (res < 0)
					{
						printk("Error writing file [%d]\n", res);
					}
					else
					{
						// printk("%d bytes written to file\n", res);
					}
				}
				k_mem_slab_free(&rx_temp_slab, &pcm_out);
			}
			write_index++;
			if(write_index > max_write_index){
				write_index = 0;
			}

			k_mutex_lock(&flash_mutex, K_FOREVER);
			rc = flash_erase(flash_dev, WRITE_ADDRESS, SPI_FLASH_SECTOR_SIZE);
			if (rc != 0)
			{
				printk("Flash erase failed with error code %d\n", rc);
				return;
			}
			const int wr_len = sizeof(int);
			rc = flash_write(flash_dev, WRITE_ADDRESS, &write_index, wr_len);
			if (rc == 0)
			{
				printk("Updated write_index : %d\n", write_index);
			}
			else
				printk("Failed to update write_index\n");
			k_mutex_unlock(&flash_mutex);

			printk("Done Writing\n");
			printk("Done recording\n");

			res = fs_close(&filep);
			if (res < 0)
			{
				printk("%d %d", res, -ENOTSUP);
				printk("Error closing file [%d]\n", res);
				// lsdir(disk_mount_pt);
			}
			for (int b = 0; b < NUM_MS; b++)
				k_mem_slab_free(&rx_mem_slab, &rx_block[b]);

//Default 1 minute predefined interval****************************************************************

				timeInterval = k_uptime_get();
                        if (timeInterval - startTime >= 60000)
                        {
			break;
		}
		else
                        {
                                // nothing to do
                        }
                }
		else
		{
			printk("Done recording\n");
			printk("Mean is too less to write file in card!!!\n");

			// Freeing memory slab buffer
			for (int b = 0; b < NUM_MS; b++)
			{

				k_mem_slab_free(&rx_mem_slab, &rx_block[b]);
			}
			
			timeInterval = k_uptime_get();
			if (timeInterval - startTime >= 60000)
			{

				break;
			}
			else
			{
				// nothing to do
			}
		}
	}
}

void Audio_init()
{

	static const char *disk_pdrv = "SD";
	uint64_t memory_size_mb;
	uint32_t block_count;
	uint32_t block_size;

	if (disk_access_init(disk_pdrv) != 0)
	{
		LOG_ERR("Storage init ERROR!");
		return;
	}

	if (disk_access_ioctl(disk_pdrv,
						  DISK_IOCTL_GET_SECTOR_COUNT, &block_count))
	{
		LOG_ERR("Unable to get sector count");
		return;
	}
	LOG_INF("Block count %u", block_count);

	if (disk_access_ioctl(disk_pdrv,
						  DISK_IOCTL_GET_SECTOR_SIZE, &block_size))
	{
		LOG_ERR("Unable to get sector size");
		return;
	}
	printk("Sector size %u\n", block_size);

	memory_size_mb = (uint64_t)block_count * block_size;
	printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
	// }

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK)
	{
		printk("Disk mounted.\n");
	}
	else
	{
		printk("Error mounting disk.\n");
	}
}

/*Display the Directory List of SD card --------------------------------------------*/
// static int lsdir(const char *path)
// {
// 	int res;
// 	struct fs_dir_t dirp;
// 	static struct fs_dirent entry;

// 	fs_dir_t_init(&dirp);

// 	/* Verify fs_opendir() */
// 	res = fs_opendir(&dirp, path);
// 	if (res)
// 	{
// 		printk("Error opening dir %s [%d]\n", path, res);
// 		return res;
// 	}

// 	printk("\nListing dir %s ...\n", path);
// 	for (;;)
// 	{
// 		/* Verify fs_readdir() */
// 		res = fs_readdir(&dirp, &entry);

// 		/* entry.name[0] == 0 means end-of-dir */
// 		if (res || entry.name[0] == 0)
// 		{
// 			break;
// 		}

// 		if (entry.type == FS_DIR_ENTRY_DIR)
// 		{
// 			printk("[DIR ] %s\n", entry.name);
// 		}
// 		else
// 		{
// 			printk("[FILE] %s (size = %zu)\n",
// 				   entry.name, entry.size);
// 		}
// 	}

// 	/* Verify fs_closedir() */
// 	fs_closedir(&dirp);

// 	return res;
// }