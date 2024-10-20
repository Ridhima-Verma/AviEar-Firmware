/*
 * Copyright (c) 2024 Ridhima Verma Suman Kumar <ridhima.20eez0011@iitrpr.ac.in> <suman@iitrpr.ac.in>
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

/* Sample which uses the filesystem API and SDHC driver */

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

// For audio
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/audio/dmic.h>
#include <complex.h>

LOG_MODULE_REGISTER(main);

// #define PCM_OUTPUT_IN_ASCII		1

// Audio Fetching Variable Declaration --------------------------------------------------

#define PI 3.14159265358979323846
#define AUDIO_FREQ 8000 // for 8 kHz sampling rate
#define CHAN_SIZE 16   //  2 bytes to represent each PCM audio sample
#define PCM_BLK_SIZE_MS ((AUDIO_FREQ / 1000) * sizeof(int16_t)) // data in 1 millisecond
#define NUM_MS 6000 //audio recording duration
K_MEM_SLAB_DEFINE(rx_mem_slab, PCM_BLK_SIZE_MS, NUM_MS + 2, 1);
void *rx_block[NUM_MS]; //dynamic buffer to store the recorded PCM Samples
size_t rx_size = PCM_BLK_SIZE_MS;
int file_name = 1;
#define READ_TIMEOUT_MS 2000
#define TARGET_FREQUENCY 3000 //Target Detection Frequnecy in Hz
#define HAMMING_WINDOW_BLOCK_SIZE 128 //samples within each Hamming Block

// Goertzel Detection Algorithm Variable Declaration -------------------------------------

double magnitude = 0.0, mean = 0.0;
double threshold_mean = 35;
int counter = 0;
// magnitude: to calculate each magnitude
// mean: to calculate mean of magnitudes

// SD params ------------------------------------------------------------------------------

static int lsdir(const char *path);

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

// Hamming Window Smoothening Function ------------------------------------------------------

static float hamming_window(size_t i)
{
	const float alpha = 0.53836;
	const float beta = 1.0 - alpha;
	float h = (alpha - beta * cosf((2 * PI * i) / (HAMMING_WINDOW_BLOCK_SIZE - 1)));
	return h;
}

//Detection Algorithm Function --------------------------------------------------------------

static float goertzel(const int16_t *pcm_data[])
{
	printk("Entering goertzel function\n");
	const float sampling_rate = (float)AUDIO_FREQ;
	const float omega = (2.0 * PI * TARGET_FREQUENCY) / sampling_rate;

	// for current, previous and previous than previous value of q
	float q0 = 0.0, q1 = 0.0, q2 = 0.0;
	float cosine = cosf(omega);

	for (size_t i = 0; i < NUM_MS; i++)
	{
		float h = 0.0;
		// Goertzel
		q1 = 0.0;
		q2 = 0.0;
		int16_t data = 0;
		for (size_t j = i; j < i + HAMMING_WINDOW_BLOCK_SIZE / 8; j++)
		{
			for (size_t k = 0; k < 8; k++)
			{
				h = hamming_window((j - i) * 8 + k + 1);
				// Extracting data from audio block
				data = pcm_data[j][k];
				q0 = h * (float)data + 2.0 * cosine * q1 - q2;
				q2 = q1;
				q1 = q0;
				// printk("q0:%f, q1:%f, q2:%f, Data:%d\n", q0, q1, q2, data);
			}
		}

		// Magnitude calculation------------------------------------------------
	
		magnitude = cabsf(q1 - cexp(-I * 2 * PI * TARGET_FREQUENCY / sampling_rate) * q2);
		mean += magnitude;
		// printk("Magnittude %i is %f\n", (i / (16)), 20 * log10(magnitude));
		i += 15;
	}
	mean = 20 * log10(mean / 375);
	//printk("Mean is %f\n", mean);
	counter ++;
	//printk("Recording Number %d\n", counter);
	//k_sleep(K_MSEC(100));
	
	return mean;  // Mean Magnitude in dB----------------------------------------
}

void main(void)
{
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
			/* These fields can be used to limit the PDM clock
			 * configurations that the driver is allowed to use
			 * to those supported by the microphone.
			 */
			//.min_pdm_clk_freq = 1000000,							//For 16kHz
			//.max_pdm_clk_freq = 3500000,							//For 16kHz
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
	
	/* raw disk i/o */
	do
	{
		static const char *disk_pdrv = "SD";
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0)
		{
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
							  DISK_IOCTL_GET_SECTOR_COUNT, &block_count))
		{
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
							  DISK_IOCTL_GET_SECTOR_SIZE, &block_size))
		{
			LOG_ERR("Unable to get sector size");
			break;
		}
		printk("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
	} while (0);

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK)
	{
		printk("Disk mounted.\n");
		lsdir(disk_mount_pt);
	}
	else
	{ 
		printk("Error mounting disk.\n");
	}
	for (;;)
	{
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
		/* Acquire microphone audio */

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

		printf("\n\n");

		float mean = goertzel((const int16_t *)rx_block);

		/* Compare the Magnitude Mean from Detection Algorithm with Threshold value --------*/

		if (mean > threshold_mean)
		{
			printk("Attempting to write to file...\n");
			char filename[30];
			int write_index = 8888;

			struct fs_file_t filep;
			size_t bytes_written;

			fs_file_t_init(&filep);

			sprintf(&filename, "/SD:/%d_%d.raw", write_index, file_name);
			file_name++;
			fs_unlink(filename);

			res = fs_open(&filep, filename, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
			if (res)
			{
				printk("Error opening file %s [%d]\n", filename, res);
				return;
			}
			else
			{
				printk("File %s opened successfully\n", filename);
			}

			for (i = 0; i < NUM_MS; i++)
			{
				uint32_t *pcm_out = rx_block[i];
				for (int j = 0; j < rx_size / 4; j++)
				{
					// printk("Writing 0x%8x, \n", pcm_out[j]);
					
            /*Write the Recorded Audio to Sd Card storage unit --------------------------------*/        
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
			}
			printk("Done Writing on flash\n");
			printk("Done recording\n");

			res = fs_close(&filep);
			if (res < 0)
			{
				printk("%d %d", res, -ENOTSUP);
				printk("Error closing file [%d]\n", res);
				lsdir(disk_mount_pt);
			}
		}
		else
		{
			printk("Done recording\n");
			printk("Mean is too less to write file in card!!!\n");
		}
		// Freeing memory slab buffer
		for (int b = 0; b < NUM_MS; b++)
			k_mem_slab_free(&rx_mem_slab, &rx_block[b]);
		// k_sleep(K_MSEC(2000));
	}

/*Display the raw PCM samples --------------------------------------------*/

#ifdef PCM_OUTPUT_IN_ASCII
	printk("-- start\n");
	int j;

	for (i = 0; i < NUM_MS; i++)
	{
		uint16_t *pcm_out = rx_block[i];

		for (j = 0; j < rx_size / 2; j++)
		{
			printk("0x%04x, \n", pcm_out[j]);
			k_sleep(K_MSEC(1));
		}
		printk("-- mid\n");
	}
	printk("-- end\n");
#endif

}

/*Display the Directory List of SD card --------------------------------------------*/

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res)
	{
		printk("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	printk("\nListing dir %s ...\n", path);
	for (;;)
	{
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0)
		{
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR)
		{
			printk("[DIR ] %s\n", entry.name);
		}
		else
		{
			printk("[FILE] %s (size = %zu)\n",
				   entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}
