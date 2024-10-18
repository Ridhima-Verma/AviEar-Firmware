/*
 * Copyright (c) 2024 Ridhima Verma Suman Kumar <ridhima.20eez0011@iitrpr.ac.in>
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#include <zephyr/logging/log.h>

#include "creds/creds.h"
#include "headers/main_aws.h"
#include "headers/main_audio.h"
#include "headers/main_gsm.h"
#include "headers/main_epoch.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zephyr/data/json.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/random/rand32.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/pm.h>
#include <poll.h>
#include <unistd.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SPI_FLASH_SECTOR_SIZE 4096
#define READ_ADDRESS 4096
#define WRITE_ADDRESS 8192


/* GPIO led code */
// Initialization LED2 for starting the system
#define LED0_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Defining the mutex as to lock the flash while reading and writing and then unlocking it. */

K_MUTEX_DEFINE(flash_mutex);

int DEV_ID = 0x77;
int write_index = 0;
int read_index = 0;
int max_write_index = 300000; //for 32GB SD Card
int max_read_index = 300000;  //for 32GB SD Card

void main(void)
{
	int rc;
	int temp_read_index, temp_write_index;
	const struct device *flash_dev;
	flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
	int len = sizeof(int);
	if (!device_is_ready(flash_dev))
	{
		printk("%s: flash device device not ready.\n", flash_dev->name);
		return;
	}
	rc = flash_read(flash_dev, READ_ADDRESS, &temp_read_index, len);
	if(rc == 0){
		printk("Successfully read Read_index : %d\n",temp_read_index);
		if(temp_read_index>0 && temp_read_index<max_read_index){
			read_index = temp_read_index;
		}
	}
	else printk("Failed to read Read_index\n");
	rc = flash_read(flash_dev, WRITE_ADDRESS, &temp_write_index, len);
	if(rc == 0){
		printk("Successfully read write_index : %d\n",temp_write_index);
		if(temp_write_index>0 && temp_write_index<max_write_index){
			write_index = temp_write_index;
		}
	}
	else printk("Failed to read write_index\n");
	printk("Device ID is : %x\n", DEV_ID);
	printk("Starting Board! %s\n", CONFIG_BOARD);
	Audio_init();
	/* Observer initializatiion. */
	/* RESET SYSTEM FOR GSM AFTER REBOOT. */
	/* GPIO 28 is set to 3V(HIGH) After 100ms. */
	gpio_pin_configure_dt(&led, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	K_MSEC(100);
	gpio_pin_set_dt(&led, 0);
	K_MSEC(100);
	gpio_pin_set_dt(&led, 1);
	K_SECONDS(1);

	init_gsm(&GSM_CONNECTED, &GSM_DISCONNECTED);
	printk("Intializing GSM.....\n");
	init_aws();
	AWS_loop();
}