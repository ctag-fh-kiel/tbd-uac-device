#include "spi_api.h"
#include "freertos/FreeRTOS.h"
#include "task.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static TaskHandle_t hTask;
static spi_slave_transaction_t transaction;
static uint8_t *send_buffer, *receive_buffer;

#define RCV_HOST    SPI3_HOST // SPI2 connects to rp2350 spi1
#define GPIO_HANDSHAKE GPIO_NUM_50 // GPIO50 is used for handshake line, P4_PICO_02 which is GPIO18 on rp2350
#define GPIO_MOSI GPIO_NUM_23
#define GPIO_MISO GPIO_NUM_22
#define GPIO_SCLK GPIO_NUM_21
#define GPIO_CS GPIO_NUM_20

typedef enum{
    Reboot = 0x13, // reboots the device
    GetFirmwareInfo = 0x19, // returns json {"HWV": hardware version, "FWV": firmware version, "OTA": active ota partition}
    RebootToOTAX = 0x22, // reboots the device to OTAX, args [X (uint8_t)]
} RequestType;

static void boot_into_slot(int slot) { // slot 0 or 1
    esp_partition_subtype_t st = (slot == 0)
        ? ESP_PARTITION_SUBTYPE_APP_OTA_0
        : ESP_PARTITION_SUBTYPE_APP_OTA_1;
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, st, NULL);
    if (!p) return;
    printf("Try to boot into %s\n", p->label);
    if (esp_ota_set_boot_partition(p) == ESP_OK) esp_restart();
    printf("Boot into %s\n not successful", p->label);
}

static const char* esp_get_current_ota_label(void){
    static char label[8] = {0};

    const esp_partition_t* running = esp_ota_get_running_partition();

    if (running->type == ESP_PARTITION_TYPE_APP &&
        running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
        running->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX){
        int ota_num = running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
        snprintf(label, sizeof(label), "ota%d", ota_num);
        return label;
    }
    else{
        return "factory"; // or return NULL if you prefer
    }
}

static int count_bootable_ota_partitions(void) {
    int count = 0;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        if (p->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
            p->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
            count++;
            }
        it = esp_partition_next(it);
    }

    esp_partition_iterator_release(it);
    return count;
}

static bool transmitCString(const RequestType reqType, const char* str){
    uint32_t len = strlen(str);
    // fields are: // 0xCA, 0xFE, request type, length (uint32_t), cstring
    uint8_t* requestTypeField = send_buffer + 2;
    *requestTypeField = (uint8_t)(reqType);
    uint32_t* lengthField = (uint32_t*)(send_buffer + 3);
    uint32_t bytes_to_send = 0;
    uint32_t bytes_sent = 0;
    while (len > 0){
        *lengthField = len;
        bytes_to_send = len > 2048 - 7 ? 2048 - 7 : len; // 7 bytes for header
        const char* ptr_cstring_section = str + bytes_sent;
        memcpy(send_buffer + 7, ptr_cstring_section, bytes_to_send);
        len -= bytes_to_send;
        bytes_sent += bytes_to_send;
        spi_slave_transmit(RCV_HOST, &transaction, portMAX_DELAY);
        // fingerprint check
        if (receive_buffer[0] != 0xCA || receive_buffer[1] != 0xFE){
            return false;
        }
        // check request type acknowledgment
        const uint8_t requestType = receive_buffer[2];
        if (requestType != (uint8_t)reqType){
            return false;
        }
    }
    return true;
}

static void api_task(void* pvParameters){
    bool result = true;
    ESP_LOGI("spi_api", "api_task()");
    while (1){
        if (result) spi_slave_transmit(RCV_HOST, &transaction, portMAX_DELAY);
        // recycle last transaction, if previous was not successful, sometimes data gets stuck
        const uint8_t* rcv_data = (uint8_t*)transaction.rx_buffer;

        // check integrity of transaction
        if (transaction.trans_len != 2048 * 8){
            ESP_LOGE("spiapi", "Received transaction length %d, expected 2048 * 8", transaction.trans_len);
            result = true;
            continue;
        }
        if (rcv_data[0] != 0xCA || rcv_data[1] != 0xFE){
            ESP_LOGE("spiapi", "Received data %x %x, expected 0xCA 0xFE", rcv_data[0], rcv_data[1]);
            result = true;
            continue;
        }

        // parse request
        RequestType requestType = (RequestType)(rcv_data[2]);
        const int uint8_param_0 = rcv_data[3]; // first request parameter, e.g. channel, favorite number, ...

        // handle request
        if (requestType == GetFirmwareInfo){
            ESP_LOGI("SpiAPI", "GetFirmwareInfo");
            {
                char info[1024] = "{\"HWV\": \"DADA\", \"FWV\": \"usb_uac_1.0\", \"OTA\": \"";
                const char* ota_label = esp_get_current_ota_label();
                strcat(info, ota_label);
                strcat(info, "\"}");
                ESP_LOGI("SpiAPI", "Firmware info: %s", info);
                result = transmitCString(requestType, info);
            }
        }else if (requestType == Reboot){
            ESP_LOGI("SpiAPI", "Rebooting device!");
            // TODO: dismount sd-card, filesystem etc!
            esp_restart();
        }else if (requestType == RebootToOTAX){
            int num_ota = count_bootable_ota_partitions();
            if (uint8_param_0 >= num_ota){
                ESP_LOGE("SpiAPI", "Requested OTA %d but only %d OTAs available!", uint8_param_0, num_ota);
            }else{
                // TODO: dismount sd-card, filesystem etc!
                boot_into_slot(uint8_param_0);
                ESP_LOGI("SpiAPI", "Rebooting device to OTA %d!", uint8_param_0);
            }
        }else{
            ESP_LOGE("SpiAPI", "Unknown request type %d", (uint8_t)requestType);
            result = true;
        }
    }
}

// Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
IRAM_ATTR static void spi_post_setup_cb(spi_slave_transaction_t *trans){
    gpio_set_level(GPIO_HANDSHAKE, 1);
}

// Called after transaction is sent/received. We use this to set the handshake line low.
IRAM_ATTR static void spi_post_trans_cb(spi_slave_transaction_t *trans){
    gpio_set_level(GPIO_HANDSHAKE, 0);
}

void spi_start(){
    ESP_LOGI("spi_api", "spi_start()");
    //Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .data_io_default_level = false,
        .max_transfer_sz = 2048,
        .flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_0,
        .intr_flags = 0
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .spics_io_num = GPIO_CS,
        .flags = 0,
        .queue_size = 1,
        .mode = 3,
        .post_setup_cb = spi_post_setup_cb,
        .post_trans_cb = spi_post_trans_cb
    };

    //Configuration for the handshake line
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(GPIO_HANDSHAKE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .hys_ctrl_mode = GPIO_HYS_SOFT_DISABLE
    };

    //Configure handshake line as output
    gpio_config(&io_conf);
    gpio_set_level(GPIO_HANDSHAKE, 0);

    send_buffer = (uint8_t*)spi_bus_dma_memory_alloc(RCV_HOST, 2048, 0);
    send_buffer[0] = 0xCA;
    send_buffer[1] = 0xFE;
    receive_buffer = (uint8_t*)spi_bus_dma_memory_alloc(RCV_HOST, 2048, 0);
    transaction.length = 2048 * 8;
    transaction.tx_buffer = send_buffer;
    transaction.rx_buffer = receive_buffer;

    esp_err_t ret = spi_slave_initialize(RCV_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    assert(ret == ESP_OK);

    xTaskCreatePinnedToCore(api_task, "spi_task", 4096 * 2, NULL, 10, &hTask, 1);
}
