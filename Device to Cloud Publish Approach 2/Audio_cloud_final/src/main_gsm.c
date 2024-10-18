/*
 * Copyright (c) 2020, Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
// #include <zephyr/net/net_conn_mgr.h>
#include <zephyr/net/net_core.h>
#include <zephyr/drivers/modem/gsm_ppp.h>
#include <zephyr/devicetree.h>
#include <zephyr/net/ppp.h>
#include "headers/main_aws.h"
#include "headers/main_audio.h"
#include "headers/main_epoch.h"

/* GPIO led code */
#include <zephyr/drivers/gpio.h>
#include <zephyr/zephyr.h>
#include <zephyr/pm/pm.h>
#include <zephyr/device.h>
#include <zephyr/zephyr.h>

/* GPIO led code */
// Initialization LED4 for GSM connected
// #define LED0_NODE DT_ALIAS(led2)
// static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#include "headers/main_gsm.h"

#include <zephyr/logging/log.h>

#define GSM_MODEM_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_gsm_ppp)
#define UART_NODE DT_BUS(GSM_MODEM_NODE)

static const struct device *const gsm_dev = DEVICE_DT_GET(GSM_MODEM_NODE);
static struct net_mgmt_event_callback mgmt_cb;

static void (*gsm_connected_gsm)();
static void (*gsm_disconnected_gsm)();

static void event_handler(struct net_mgmt_event_callback *cb,
						  uint32_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if ((mgmt_event & (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)) != mgmt_event)
	{
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED)
	{
		printk("Network connected");
		(*gsm_connected_gsm)(NULL);
		printk("LED 4 IS HIGH\n");
		// gpio_pin_configure_dt(&led, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
		// gpio_pin_set_dt(&led, 1);
		// K_MSEC(100);
		//(void)gsm_query_local_time(gsm_dev);
		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED)
	{
		printk("Network disconnected");
		(*gsm_disconnected_gsm)(NULL);
		printk("LED 4 IS LOW\n");
		// gpio_pin_configure_dt(&led, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
		// gpio_pin_set_dt(&led, 0);
		// K_MSEC(100);
		k_sleep(K_SECONDS(30));
		sys_reboot();

		return;
	}
}

static void modem_on_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	// printk("GSM modem on callback fired");
}

static void modem_off_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	// printk("GSM modem off callback fired");
}

void init_gsm(void *gsm_connected, void *gsm_disconnected)
{
	gsm_connected_gsm = gsm_connected;
	gsm_disconnected_gsm = gsm_disconnected;

	printk("Init... gsm");

	const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

	/* Optional register modem power callbacks */
	gsm_ppp_register_modem_power_callback(gsm_dev, modem_on_cb, modem_off_cb, NULL);

	printk("Board '%s' APN '%s' UART '%s' device %p (%s)",
		   CONFIG_BOARD, CONFIG_MODEM_GSM_APN,
		   uart_dev->name, uart_dev, gsm_dev->name);

	net_mgmt_init_event_callback(&mgmt_cb, event_handler,
								 NET_EVENT_L4_CONNECTED |
									 NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&mgmt_cb);

	printk("Initialised gsm\n");
}
