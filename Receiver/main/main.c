// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "lora.h"

// uint8_t buf[32];

// void task_rx(void *p)
// {
//    int x;
//    for(;;) {
//       lora_receive();    // put into receive mode
//       while(lora_received()) {
//          x = lora_receive_packet(buf, sizeof(buf));
//          buf[x] = 0;
//          printf("Received: %s\n", buf);
//          lora_receive();
//       }
//       vTaskDelay(1);
//    }
// }

// void app_main()
// {
//    lora_init();
//    lora_set_frequency(866e6);
//    lora_enable_crc();
//    xTaskCreate(&task_rx, "task_rx", 2048, NULL, 5, NULL);
// }


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora.h"
#include "esp_log.h"
#include "mbedtls/aes.h"

#define LORA_FREQUENCY 866E6
#define LORA_CRC_ENABLED true
#define RX_TASK_STACK_SIZE 3072
#define RX_TASK_PRIORITY 5

// #define PIN_NUM_MISO 13
// #define PIN_NUM_MOSI 2
// #define PIN_NUM_CLK  14
// #define PIN_NUM_CS   12
// #define PIN_NUM_RST  15
// #define PIN_NUM_DIO0 16

static const char *TAG = "Lora_Decryption";

// Decryption parameters
static const char dec_key[16] = "1234567890123456"; // AES 128-bit key
static const char dec_iv[16] = "1234567890123456";

// Decrypt string with AES
void decrypt_string(char *input, uint8_t input_len, const char *key, const char *iv, unsigned char **output, int *output_len)
{
    if (input_len % 16 != 0)
    {
        ESP_LOGE(TAG, "Invalid input length for decryption");
        return;
    }

    *output = (unsigned char *)malloc(input_len);
    if (!*output) {
        ESP_LOGE(TAG, "Failed to allocate memory for decryption output");
        return;
    }

    uint8_t key_copy[16] = {0}; // AES 128-bit key size
    uint8_t iv_copy[16] = {0};
    memcpy(key_copy, key, 16);
    memcpy(iv_copy, iv, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key_copy, 128); // Use AES 128-bit decryption

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, input_len, iv_copy, (unsigned char *)input, *output);

    // Remove padding
    int pad_len = (*output)[input_len - 1];
    *output_len = input_len - pad_len;

    ESP_LOG_BUFFER_HEX(TAG, *output, *output_len);
}

void task_rx(void *p)
{
    uint8_t buf[256];
    int packet_len;
    while (1)
    {
        lora_receive(); // Put into receive mode
        while (lora_received())
        {
            packet_len = lora_receive_packet(buf, sizeof(buf));
            buf[packet_len] = '\0';
            ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", packet_len, packet_len, buf);
            printf("Received encrypted packet: %s\n", buf);

            unsigned char *decrypted_message = NULL;
            int decrypted_message_len = 0;

            uint8_t iv[16];
            memcpy(iv, dec_iv, sizeof(iv));

            decrypt_string((char *)buf, packet_len, dec_key, dec_iv, &decrypted_message, &decrypted_message_len);

            if (decrypted_message != NULL) {
                printf("Decrypted message: %.*s\n", decrypted_message_len, decrypted_message);
                free(decrypted_message);
            } else {
                printf("Decryption failed\n");
            }

            lora_receive();
        }
        // vTaskDelay(pdMS_TO_TICKS(1));
        vTaskDelay(1);
    }
}


void app_main()
{
    //lora_init();
    if (lora_init() == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	}

    //lora_set_frequency(866E6);
    ESP_LOGI(pcTaskGetName(NULL), "Frequency is 866MHz");
	lora_set_frequency(866e6);
    lora_enable_crc();

   int cr = 5;
	int bw = 7;
	int sf = 7;
#if CONFIF_ADVANCED
	cr = CONFIG_CODING_RATE
	bw = CONFIG_BANDWIDTH;
	sf = CONFIG_SF_RATE;
#endif

    lora_set_coding_rate(cr);
	ESP_LOGI(pcTaskGetName(NULL), "coding_rate=%d", cr);

	lora_set_bandwidth(bw);
	ESP_LOGI(pcTaskGetName(NULL), "bandwidth=%d", bw);

	lora_set_spreading_factor(sf);
	ESP_LOGI(pcTaskGetName(NULL), "spreading_factor=%d", sf);

    xTaskCreate(&task_rx, "task_rx", RX_TASK_STACK_SIZE, NULL, RX_TASK_PRIORITY, NULL);
}
