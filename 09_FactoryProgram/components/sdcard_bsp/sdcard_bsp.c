#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdcard_bsp.h"
#include "esp_log.h"
#include "esp_err.h"

#define SDMMC_D0_PIN    42  
#define SDMMC_CLK_PIN   1
#define SDMMC_CMD_PIN   2

static const char *TAG = "_sdcard";

EventGroupHandle_t sdcard_even_ = NULL;

sdcard_bsp_t user_sdcard_bsp;

#define SDlist "/sdcard" //Table of Contents, similar to a standard

sdmmc_card_t *card_host = NULL;

void _sdcard_init(void)
{
  sdcard_even_ = xEventGroupCreate();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = 
  {
    .format_if_mount_failed = false,    
    .max_files = 5,                        //Maximum number of opened files
    .allocation_unit_size = 16 * 1024 *3, 
  };

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;//high speed

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;           //1
  slot_config.clk = SDMMC_CLK_PIN;
  slot_config.cmd = SDMMC_CMD_PIN;
  slot_config.d0 = SDMMC_D0_PIN;

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_fat_sdmmc_mount(SDlist, &host, &slot_config, &mount_config, &card_host));

  if(card_host != NULL)
  {
    sdmmc_card_print_info(stdout, card_host); //Print out the information of the card.
    user_sdcard_bsp.sdcard_size = (float)(card_host->csd.capacity)/2048/1024; //G
    xEventGroupSetBits(sdcard_even_,0x01);
  }
}
/* Write data
path: Path
data: Data */ 
esp_err_t sdcard_file_write(const char *path, const char *data)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    ESP_LOGE(TAG, "SD card not initialized (card == NULL)");
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //First, check if there is an SD card.
  if(err != ESP_OK)
  {
    ESP_LOGE(TAG, "SD card status check failed (card not present or unresponsive)");
    return err;
  }
  FILE *f = fopen(path, "w"); //Obtain the path address
  if(f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return ESP_ERR_NOT_FOUND;
  }
  fprintf(f, data); 
  fclose(f);
  return ESP_OK;
}
/*
Read data
path: path */
esp_err_t sdcard_file_read(const char *path, char *buffer, size_t *out_len)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    ESP_LOGE(TAG, "SD card not initialized (card == NULL)");
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //First, check if there is an SD card.
  if(err != ESP_OK)
  {
    ESP_LOGE(TAG, "SD card status check failed (card not present or unresponsive)");
    return err;
  }
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return ESP_ERR_NOT_FOUND;
  }
  fseek(f, 0, SEEK_END);     //Move the pointer to the very end.
  uint32_t unlen = ftell(f);
  //fgets(pxbuf, unlen, f); //read characters from file
  fseek(f, 0, SEEK_SET); //Move the pointer to the very beginning.
  uint32_t poutLen = fread((void *)buffer,1,unlen,f);
  if(out_len != NULL)
  *out_len = poutLen;
  fclose(f);
  return ESP_OK;
}