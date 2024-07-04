// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "lora.h"

// void task_tx(void *p)
// {
//    for(;;) {
//       vTaskDelay(pdMS_TO_TICKS(5000));
//       lora_send_packet((uint8_t*)"Hello", 5);
//       printf("packet sent...\n");
//    }
// }

// void app_main()
// {
//    lora_init();
//    lora_set_frequency(866e6);
//    lora_enable_crc();
//    xTaskCreate(&task_tx, "task_tx", 2048, NULL, 5, NULL);
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
#define TX_TASK_STACK_SIZE 3072
#define TX_TASK_PRIORITY 5
#define TX_DELAY_MS 5000

// #define PIN_NUM_MISO 13
// #define PIN_NUM_MOSI 2
// #define PIN_NUM_CLK  14
// #define PIN_NUM_CS   12
// #define PIN_NUM_RST  15
// #define PIN_NUM_DIO0 16

static const char *TAG = "Lora_Encryption";

// Encryption parameters
static const char enc_key[16] = "1234567890123456"; // AES 128-bit key
static const char enc_iv[16] = "1234567890123456";

// Encrypt string with AES
void encrypt_any_length_string(const char *input, uint8_t *key, uint8_t *iv, unsigned char **output, int *output_len) {
    int padded_input_len = 0;
    int input_len = strlen(input) + 1;
    int modulo16 = input_len % 16;

    if (input_len < 16)
        padded_input_len = 16;
    else
        padded_input_len = (input_len / 16 + 1) * 16;

    char *padded_input = (char *)malloc(padded_input_len);
    if (!padded_input) {
        ESP_LOGE(TAG, "Failed to allocate memory for padding");
        return;
    }
    memcpy(padded_input, input, strlen(input));
    uint8_t pkc5_value = (17 - modulo16);
    for (int i = strlen(input); i < padded_input_len; i++) {
        padded_input[i] = pkc5_value;
    }

    *output = (unsigned char *)malloc(padded_input_len);
    if (!*output) {
        ESP_LOGE(TAG, "Failed to allocate memory for encryption output");
        free(padded_input);
        return;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128); // Use AES 128-bit key
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_input_len, iv, (unsigned char *)padded_input, *output);

    *output_len = padded_input_len;

    ESP_LOG_BUFFER_HEX(TAG, *output, *output_len);

    free(padded_input);
}

void task_tx(void *p) {
    const char *message = "Hello World";

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TX_DELAY_MS));

        unsigned char *encrypted_message = NULL;
        int encrypted_message_len = 0;
        uint8_t iv[16];
        memcpy(iv, enc_iv, sizeof(iv));

        encrypt_any_length_string(message, (uint8_t *)enc_key, iv, &encrypted_message, &encrypted_message_len);

        if (encrypted_message != NULL) {
            lora_send_packet(encrypted_message, encrypted_message_len);
            //printf("Packet sent...\n");
            ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent...", encrypted_message_len);
		    int lost = lora_packet_lost();
		    if (lost != 0) {
			    ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
		    }
            free(encrypted_message);
        }
    }
}

void app_main() {
    // Initialize LoRa
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


    // Create the TX task
    xTaskCreate(&task_tx, "task_tx", TX_TASK_STACK_SIZE, NULL, TX_TASK_PRIORITY, NULL);
}

