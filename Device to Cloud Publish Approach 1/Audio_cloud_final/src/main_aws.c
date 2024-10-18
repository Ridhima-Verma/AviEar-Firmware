#include "creds/creds.h"
#include "headers/main_aws.h"
#include "headers/main_audio.h"
#include "headers/main_gsm.h"
#include "headers/main_epoch.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/modem/gsm_ppp.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/mqtt.h>
// #include <zephyr/net/net_conn_mgr.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ppp.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/random/rand32.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/zephyr.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/pm.h>
#include <poll.h>
#include <unistd.h>
#include <zephyr/zephyr.h>

#define SPI_FLASH_SECTOR_SIZE 4096
#define READ_ADDRESS 4096
#define WRITE_ADDRESS 8192

#define GSM_MODEM_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_gsm_ppp)
#define UART_NODE DT_BUS(GSM_MODEM_NODE)

static const struct device *const gsm_dev = DEVICE_DT_GET(GSM_MODEM_NODE);
static struct net_mgmt_event_callback mgmt_cb;

#if defined(CONFIG_MBEDTLS_MEMORY_DEBUG)
#include <mbedtls/memory_buffer_alloc.h>
#endif
#include <sys/socket.h>

static bool gsm_connected_aws = false;

#define SNTP_SERVER "0.pool.ntp.org"

#define AWS_BROKER_PORT "8883"

#define MQTT_BUFFER_SIZE 256u
#define APP_BUFFER_SIZE 16384u

#define MAX_RETRIES 10u
#define BACKOFF_EXP_BASE_MS 1000u
#define BACKOFF_EXP_MAX_MS 60000u
#define BACKOFF_CONST_MS 5000u

static struct sockaddr_in aws_broker;

static uint8_t rx_buffer[MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_BUFFER_SIZE];
static uint8_t buffer[APP_BUFFER_SIZE]; /* Shared between published and received messages */

static struct mqtt_client client_ctx;

static const char mqtt_client_name[] = CONFIG_AWS_THING_NAME;

static uint32_t messages_received_counter;
static bool do_publish;	  /* Trigger client to publish */
static bool do_subscribe; /* Trigger client to subscribe */

// AWS Certificates --------------------------------------------------

#define TLS_TAG_DEVICE_CERTIFICATE 1
#define TLS_TAG_DEVICE_PRIVATE_KEY 1
#define TLS_TAG_AWS_CA_CERTIFICATE 1

extern struct k_mutex flash_mutex;
// #define PUBLISH_PAYLOAD_SIZE 4096

// Audio MQTT Publish Payload Variable --------------------------------------------------

int PUBLISH_PAYLOAD_SIZE = 96000;
// #define PUBLISH_PAYLOAD_SIZE 79872

extern int read_index;
extern int write_index;
int audio_counter = 0;
int pub_ret = -1;

extern int max_read_index;
uint8_t read_total_frag = 0x00;

extern int counter;
// #define FLASH_MAX_SIZE 16777216
#define FLASH_MAX_SIZE 16704000
// #define FLASH_MAX_SIZE 8388608
K_SEM_DEFINE(ack_done_sem, 0, 1);

#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/zephyr.h>

static int var_memory = 0;

static sec_tag_t sec_tls_tags[] = {
	TLS_TAG_DEVICE_CERTIFICATE,
};

// Setting up AWS Credentials -------------------------------------

static int setup_credentials(void)
{
	int ret;

	ret = tls_credential_add(TLS_TAG_DEVICE_CERTIFICATE, TLS_CREDENTIAL_SERVER_CERTIFICATE,
							 public_cert, public_cert_len);
	if (ret < 0)
	{
		printk("Failed to add device certificate: %d", ret);
		goto exit;
	}

	ret = tls_credential_add(TLS_TAG_DEVICE_PRIVATE_KEY, TLS_CREDENTIAL_PRIVATE_KEY,
							 private_key, private_key_len);
	if (ret < 0)
	{
		printk("Failed to add device private key: %d", ret);
		goto exit;
	}

	ret = tls_credential_add(TLS_TAG_AWS_CA_CERTIFICATE, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert,
							 ca_cert_len);
	if (ret < 0)
	{
		printk("Failed to add device private key: %d", ret);
		goto exit;
	}

exit:
	return ret;
}

// MQTT Audio Publish Payload -------------------------------------------
static int publish_message(const char *topic, size_t topic_len, uint8_t *payload,
						   size_t payload_len)
{

	static uint32_t message_id = 1u;

	int ret;
	struct mqtt_publish_param msg;

	msg.retain_flag = 0u;
	msg.message.topic.topic.utf8 = topic;
	msg.message.topic.topic.size = topic_len;
	msg.message.topic.qos = CONFIG_AWS_QOS;
	msg.message.payload.data = payload;
	msg.message.payload.len = payload_len;
	msg.message_id = message_id++;

	ret = mqtt_publish(&client_ctx, &msg);
	if (ret != 0)
	{
		printk("Failed to publish message: %d", ret);
		printk("Address of flash which is not published: %08x \n", read_index);
		/* MQTT not connected */
		// gpio_pin_configure_dt(&led7, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
		// gpio_pin_set_dt(&led7, 0);
		// k_sleep(K_MSEC(100));
	}
	else
	{
		messages_received_counter++;

		printk("PUBLISHED on topic \"%s\" [ id: %u qos: %u ], payload: %u B", topic, msg.message_id,
			   msg.message.topic.qos, payload_len);
	}

	k_sleep(K_MSEC(500));

	return ret;
}

static ssize_t handle_published_message(const struct mqtt_publish_param *pub)
{
	int ret;
	size_t received = 0u;
	const size_t message_size = pub->message.payload.len;
	const bool discarded = message_size > APP_BUFFER_SIZE;

	printk("RECEIVED on topic \"%s\" [ id: %u qos: %u ] payload: %u / %u B",
		   (const char *)pub->message.topic.topic.utf8, pub->message_id, pub->message.topic.qos,
		   message_size, APP_BUFFER_SIZE);

	while (received < message_size)
	{
		uint8_t *p = discarded ? buffer : &buffer[received];

		ret = mqtt_read_publish_payload_blocking(&client_ctx, p, APP_BUFFER_SIZE);
		if (ret < 0)
		{
			return ret;
		}

		received += ret;
	}

	/* Send ACK */
	switch (pub->message.topic.qos)
	{
	case MQTT_QOS_1_AT_LEAST_ONCE:
	{
		struct mqtt_puback_param puback;

		puback.message_id = pub->message_id;
		mqtt_publish_qos1_ack(&client_ctx, &puback);
	}
	break;
	case MQTT_QOS_2_EXACTLY_ONCE: /* unhandled (not supported by AWS) */
	case MQTT_QOS_0_AT_MOST_ONCE: /* nothing to do */
	default:
		break;
	}

	return discarded ? -ENOMEM : received;
}

const char *mqtt_evt_type_to_str(enum mqtt_evt_type type)
{
	static const char *const types[] = {
		"CONNACK",
		"DISCONNECT",
		"PUBLISH",
		"PUBACK",
		"PUBREC",
		"PUBREL",
		"PUBCOMP",
		"SUBACK",
		"UNSUBACK",
		"PINGRESP",
	};

	return (type < ARRAY_SIZE(types)) ? types[type] : "<unknown>";
}

// Audio MQTT Publish Payload Callback --------------------------------------------------
static void mqtt_event_cb(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	printk("MQTT event: %s [%u] result: %d", mqtt_evt_type_to_str(evt->type), evt->type,
		   evt->result);

	switch (evt->type)
	{
	case MQTT_EVT_CONNACK:
	{
		// do_subscribe = true;
		do_publish = true;
	}
	break;

	case MQTT_EVT_PUBLISH:
	{
		const struct mqtt_publish_param *pub = &evt->param.publish;

		handle_published_message(pub);
		messages_received_counter++;
#if !defined(CONFIG_AWS_TEST_SUITE_RECV_QOS1)
		do_publish = true;
#endif
	}
	break;

	case MQTT_EVT_SUBACK:
	{
#if !defined(CONFIG_AWS_TEST_SUITE_RECV_QOS1)
		do_publish = true;
#endif
	}
	break;

	case MQTT_EVT_PUBACK:
	{
		int rc;
		const struct device *flash_dev;
		flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
		if (!device_is_ready(flash_dev))
		{
			printk("%s: flash device device not ready.\n", flash_dev->name);
			return;
		}

		// k_sem_give(&ack_done_sem);
		printk("\nPUBACK received, flash @ %08x -> \n", read_index);
		read_index++;
		k_mutex_lock(&flash_mutex, K_FOREVER);
		rc = flash_erase(flash_dev, READ_ADDRESS, SPI_FLASH_SECTOR_SIZE);
		if (rc != 0)
		{
			printk("Flash erase failed with error code %d\n", rc);
			return;
		}
		const int wr_len = sizeof(int);
		rc = flash_write(flash_dev, READ_ADDRESS, &read_index, wr_len);
		if (rc == 0)
		{
			printk("Updated read_index : %d\n", read_index);
		}
		else
			printk("Failed to update read_index\n");
		k_mutex_unlock(&flash_mutex);
	}
	break;

	case MQTT_EVT_DISCONNECT:
	case MQTT_EVT_PUBREC:
	case MQTT_EVT_PUBREL:
	case MQTT_EVT_PUBCOMP:
	case MQTT_EVT_PINGRESP:
	case MQTT_EVT_UNSUBACK:
	default:
		break;
	}
}

// AWS Client Setup --------------------------------------------------
static void aws_client_setup(void)
{
	mqtt_client_init(&client_ctx);

	client_ctx.broker = &aws_broker;
	client_ctx.evt_cb = mqtt_event_cb;

	client_ctx.client_id.utf8 = (uint8_t *)mqtt_client_name;
	client_ctx.client_id.size = sizeof(mqtt_client_name) - 1;
	client_ctx.password = NULL;
	client_ctx.user_name = NULL;

	client_ctx.keepalive = CONFIG_MQTT_KEEPALIVE;

	client_ctx.protocol_version = MQTT_VERSION_3_1_1;

	client_ctx.rx_buf = rx_buffer;
	client_ctx.rx_buf_size = MQTT_BUFFER_SIZE;
	client_ctx.tx_buf = tx_buffer;
	client_ctx.tx_buf_size = MQTT_BUFFER_SIZE;

	/* setup TLS */
	client_ctx.transport.type = MQTT_TRANSPORT_SECURE;
	struct mqtt_sec_config *const tls_config = &client_ctx.transport.tls.config;

	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = sec_tls_tags;
	tls_config->sec_tag_count = ARRAY_SIZE(sec_tls_tags);
	tls_config->hostname = CONFIG_AWS_ENDPOINT;
	tls_config->cert_nocopy = TLS_CERT_NOCOPY_NONE;
}

struct backoff_context
{
	uint16_t retries_count;
	uint16_t max_retries;

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	uint32_t attempt_max_backoff; /* ms */
	uint32_t max_backoff;		  /* ms */
#endif
};


// Exponential Backoff for Retries -------------------------------------------
static void backoff_context_init(struct backoff_context *bo)
{
	__ASSERT_NO_MSG(bo != NULL);

	bo->retries_count = 0u;
	bo->max_retries = MAX_RETRIES;

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	bo->attempt_max_backoff = BACKOFF_EXP_BASE_MS;
	bo->max_backoff = BACKOFF_EXP_MAX_MS;
#endif
}

/* https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/ */
static void backoff_get_next(struct backoff_context *bo, uint32_t *next_backoff_ms)
{
	__ASSERT_NO_MSG(bo != NULL);
	__ASSERT_NO_MSG(next_backoff_ms != NULL);

#if defined(CONFIG_AWS_EXPONENTIAL_BACKOFF)
	if (bo->retries_count <= bo->max_retries)
	{
		*next_backoff_ms = sys_rand32_get() % (bo->attempt_max_backoff + 1u);

		/* Calculate max backoff for the next attempt (~ 2**attempt) */
		bo->attempt_max_backoff = MIN(bo->attempt_max_backoff * 2u, bo->max_backoff);
		bo->retries_count++;
	}
#else
	*next_backoff_ms = BACKOFF_CONST_MS;
#endif
}

static int aws_client_try_connect(void)
{
	int ret = -1;
	uint32_t backoff_ms;
	struct backoff_context bo;

	backoff_context_init(&bo);

	while (bo.retries_count <= bo.max_retries)
	{

		// Sytem reboot if backoff delay is greater than 5
		if (bo.retries_count >= 3)
		{ // changed from 3 to 5
			printk("Inside the rebbot");
			sys_reboot();
		}

		ret = mqtt_connect(&client_ctx);
		if (ret == 0)
		{
			/* MQTT  connected */
			// gpio_pin_configure_dt(&led7, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
			// gpio_pin_set_dt(&led7, 1);
			K_MSEC(100);
			goto exit;
		}

		backoff_get_next(&bo, &backoff_ms);

		printk("Failed to connect: %d backoff delay: %u ms", ret, backoff_ms);
		k_msleep(backoff_ms);
		// gpio_pin_configure_dt(&led7, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
		// gpio_pin_set_dt(&led7, 0);

		K_MSEC(100);
	}

exit:
	return ret;
}

struct publish_payload
{
	uint32_t counter;
};

void publish(void)
{
	/* The length of data reading from flash is 1024*64 bytes */
	int ret;
	int rc;
	const size_t len = PUBLISH_PAYLOAD_SIZE;

	uint8_t buf[PUBLISH_PAYLOAD_SIZE];
	struct fs_file_t file;
	fs_file_t_init(&file);
	char read_filename[30];
	if (read_index >= max_read_index)
	{
		read_index = 0;
	}
	if (read_index < write_index)
	{
		sprintf(&read_filename, "/SD:/%d.raw", read_index);
		ret = fs_open(&file, read_filename, FS_O_READ);
		// ret = fs_seek(&file, read_index * PUBLISH_PAYLOAD_SIZE, FS_SEEK_SET);
		
		rc = fs_read(&file, &buf, sizeof(buf));
		if (rc < 0)
		{
			printk("FAIL: read %s: [rd:%d]\n", read_filename, rc);
		}
		ret = fs_close(&file);
	}

	publish_message(CONFIG_AWS_PUBLISH_TOPIC, strlen(CONFIG_AWS_PUBLISH_TOPIC), buf,
					len);
}

void aws_client_loop(void)
{
	int rc;
	int timeout;
	struct pollfd fds;

	// printk("setting up client\n");
	aws_client_setup();
	// printk("client setup done\n");

   // GSM Modem Connection Setup -------------------------------------------
	if (gsm_connected_aws)
	{
		printk("GSM_CONNECTED_AWS ..............\n");
		printk("connecting...\n");
		rc = aws_client_try_connect();

		if (rc != 0)
		{
			goto cleanup;
		}
	}
	else
	{
		goto cleanup;
	}

	printk("client connected\n");

	fds.fd = client_ctx.transport.tcp.sock;
	fds.events = POLLIN | POLLHUP | POLLERR;

	for (;;)
	{
		printk("\nTrying publishing....\n");
		timeout = mqtt_keepalive_time_left(&client_ctx);
		rc = poll(&fds, 1u, timeout);
		if (rc >= 0)
		{
			if (fds.revents & POLLIN)
			{
				rc = mqtt_input(&client_ctx);
				if (rc != 0)
				{
					printk("Failed to read MQTT input: %d\n", rc);
					/* MQTT not connected */
					break;
				}
			}

			rc = mqtt_live(&client_ctx);
			if ((rc != 0) && (rc != -EAGAIN))
			{
				printk("Failed to live MQTT: %d\n", rc);
				/* MQTT not connected */

				break;
			}
		}
		else
		{
			printk("poll failed: %d", rc);
			break;
		}

		if (do_publish)
		{
			printk("\ndo publish true! \n\n");
			if (audio_counter < 2)
			{
				if ((write_index - read_index) >= 1)
				{
					audio_counter++;
				}
				else
				{
					audio_counter = 0;
					audio_start();
				}
			}
			else
			{
				audio_counter = 0;
				audio_start();
			} // from here it will enter main_audio.c
			while (1)
			{

				if (write_index < read_index)
				{
					var_memory = max_read_index;
				}
				else
				{
					var_memory = 0;
				}

				if (((write_index - read_index + var_memory) >= 1))
				{

					break;
				}
				k_msleep(100);
			}

			{
				printk("\n publishing data address: %08x \n", read_index);
				publish();
			}
		}
	}

cleanup:
	mqtt_disconnect(&client_ctx);
	close(fds.fd);
	fds.fd = -1;
}

int sntp_sync_time(void)
{

	int rc;
	struct sntp_time now;
	struct timespec tspec;

	rc = sntp_simple(SNTP_SERVER, SYS_FOREVER_MS, &now);
	// printk("\n Value in SNTP = %x\n", now.seconds);
	if (rc == 0)
	{
		printk("\n Value in SNTP = %d", now.seconds);
		tspec.tv_sec = now.seconds;
		tspec.tv_nsec = ((uint64_t)now.fraction * (1000lu * 1000lu * 1000lu)) >> 32;

		clock_settime(CLOCK_REALTIME, &tspec);

		printk("Acquired time from NTP server: %u", (uint32_t)tspec.tv_sec);
	}
	else
	{
		printk("Failed to acquire SNTP, code %d\n", rc);
	}
	return rc;
}


// AWS Broker Address for Connection Setup-------------------------------------------
static int resolve_broker_addr(struct sockaddr_in *broker)
{
	// printk("entered broke//r\n");/

	int ret;
	struct zsock_addrinfo *ai = NULL;

	const struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
	};

	// printk("zsock calling\n");
	ret = zsock_getaddrinfo(CONFIG_AWS_ENDPOINT, AWS_BROKER_PORT, &hints, &ai);

	if (ret == 0)
	{
		printk("copying memory\n");
		memcpy(broker, ai->ai_addr, MIN(ai->ai_addrlen, sizeof(struct sockaddr_storage)));

		char addr_str[INET_ADDRSTRLEN];

		zsock_inet_ntop(AF_INET, &broker->sin_addr, addr_str, sizeof(addr_str));
		printk("Resolved: %s:%u", addr_str, htons(broker->sin_port));
	}
	else
	{
		// printk("failed to resolve hostname err = %d (errno = %d)", ret, errno);
	}

	zsock_freeaddrinfo(ai);
	// printk("left broker\n");

	return ret;
}

void init_aws(void)
{
	printk("Initializing AWS...\n");

	dns_init_resolver();

	sntp_sync_time();

	setup_credentials();

	setting_epoch();

	printk("_____AWS Initialised____ \n");
}

void GSM_CONNECTED(void)
{
	gsm_connected_aws = true;
}

void GSM_DISCONNECTED(void)
{
	gsm_connected_aws = false;
}

void AWS_loop(void)
{

	for (;;)
	{
		// printk("Calling broker\n");
		resolve_broker_addr(&aws_broker);

		// printk("Calling client\n");
		aws_client_loop();
#if defined(CONFIG_MBEDTLS_MEMORY_DEBUG)
		size_t cur_used, cur_blocks, max_used, max_blocks;

		mbedtls_memory_buffer_alloc_cur_get(&cur_used, &cur_blocks);
		mbedtls_memory_buffer_alloc_max_get(&max_used, &max_blocks);
		printk("mbedTLS heap usage: MAX %u/%u (%u) CUR %u (%u)", max_used,
			   CONFIG_MBEDTLS_HEAP_SIZE, max_blocks, cur_used, cur_blocks);
#endif

		k_sleep(K_SECONDS(1));
	}

	return;
}