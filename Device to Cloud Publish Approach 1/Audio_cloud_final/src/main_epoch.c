#include "headers/main_aws.h"
#include "headers/main_audio.h"
#include "headers/main_gsm.h"
#include "headers/main_epoch.h"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/printk.h>
#include <zephyr/zephyr.h>

#include <date_time.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/pm.h>
#include <zephyr/zephyr.h>

#include <stdio.h>
#include <time.h>

static int connection_flag = 0;
static int64_t adv_seconds = 0;
// int hr = 0;

void setting_epoch(void)
{
    struct tm adv_time;
    /* Data validation in if statement date can be between 0 and 31. Hour can be in 0 and 24 and seconds can be less than 60. */
    if (connection_flag < 1 && date_1byte() >= 0 && date_1byte() <= 31 && hour_1byte() >= 0 && hour_1byte() <= 24 && sec_1byte() <= 60)
    {
        /* Adding the time stamp is struct tm adv_time. */
        adv_time.tm_year = year_1byte();
        adv_time.tm_hour = hour_1byte();
        adv_time.tm_min = min_1byte();
        adv_time.tm_mon = mon_1byte();
        adv_time.tm_sec = sec_1byte();
        adv_time.tm_mday = date_1byte();

        // struct tm *timeinfo = adv_time.tm_hour;
        // hr = hour_1byte();
        /* Setting the adv_time as reference time */
        date_time_set(&adv_time);

        /* Time updation for real time access. */
        int error = date_time_update_async(NULL);

        /* To make it run only once. */
        connection_flag++;
    }
    /* Time Updation either Uptime or Epoch time. */
    /* adv_seconds contains the time since epoch unix (1 jan 1970) with resolution in milliseconds. */
    // int64_t adv_seconds = 0;
    /* If the GSM is not connected then this piece will use the k_uptime_get() (--> that is the time (in ms ) after system reboot.) as the time stamp. */
    if (connection_flag < 1)
    {
        // printk("in uptime get");

        /*  In case the GSM is not connected after the system REBOOT.
            Getting the uptime and storing it in the same variable as adv_seconds.
            Resolution of k_uptime_get() is in ms. */
        adv_seconds = 0;
    }
    else
    {
        /* To update the time whenever the advertising packet from node is recieved and then using them as time stamp. */
        printk("epoch time updated\n");
        date_time_now(&adv_seconds);
    }
}

void get_epoch(int arr[])
{
    /* divisor epoch is 10^13. */
    int t_arr[10]={0};
    int64_t divisor_epoch = 1000000000000;
    // k_msleep(100);
    for (int epoch = 0; epoch < 10; epoch++)
    {
        /* Appending the time stamp in  write_array at the last. Only once single digit number goes at one time. */
        arr[epoch] = (uint8_t)((adv_seconds / divisor_epoch) % 10);

        /* Going to lower indices side by side. */
        divisor_epoch = divisor_epoch / 10;
        // t_arr[epoch]=arr[epoch];
    }
    // char time_str[11]; // Adjust the size based on your needs
    // int offset = 0;    // Offset in the string

    // for (int i = 0; i < 10; ++i)
    // {
    //     // Append each element to the string
    //     offset += sprintf(time_str + offset, "%u", t_arr[i]);
    // }

    // time_str[sizeof(time_str) - 1] = '\0';

    // /*Converting string array to integer*/
    // int result = atoi(time_str);

    // time_t epoch_time = result;
    // struct tm *timeinfo = localtime(&epoch_time);
    // hr = ((timeinfo->tm_hour) % 24);
}