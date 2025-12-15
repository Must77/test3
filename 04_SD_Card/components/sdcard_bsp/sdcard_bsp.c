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
#include "driver/sdmmc_host.h"

static const char *TAG = "_sdcard";

#define SDMMC_CMD_PIN   (gpio_num_t)2
#define SDMMC_D0_PIN    (gpio_num_t)42
#define SDMMC_CLK_PIN   (gpio_num_t)1

#define SDlist "/sdcard" //A table of contents, similar to a standard.

sdmmc_card_t *card_host = NULL;


static float sdcard_get_value(void)
{
  if(card_host != NULL)
  {
    return (float)(card_host->csd.capacity)/2048/1024; //G
  }
  else
  return 0;
}

void sdcard_init(void)
{
  esp_vfs_fat_sdmmc_mount_config_t mount_config = 
  {
    .format_if_mount_failed = false,       //If the mounting fails, create a partition table and format the SD card.
    .max_files = 5,                        //Maximum number of opened files
    .allocation_unit_size = 16 * 1024 *3,  //Similar to sector size
  };

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;//high speed

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;           
  slot_config.clk = SDMMC_CLK_PIN;
  slot_config.cmd = SDMMC_CMD_PIN;
  slot_config.d0 = SDMMC_D0_PIN;

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_vfs_fat_sdmmc_mount(SDlist, &host, &slot_config, &mount_config, &card_host));

  if(card_host != NULL)
  {
    sdmmc_card_print_info(stdout, card_host); //Print out the information of the card.
    printf("practical_size:%.2fG\n",sdcard_get_value());//g
  }
}
/* Write data
path: Path
data: Data */
esp_err_t s_example_write_file(const char *path, char *data)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //First, check if there is an SD card
  if(err != ESP_OK)
  {
    return err;
  }
  FILE *f = fopen(path, "w"); //Obtain the path address
  if(f == NULL)
  {
    printf("path:Write Wrong path\n");
    return ESP_ERR_NOT_FOUND;
  }
  fprintf(f, data); 
  fclose(f);
  return ESP_OK;
}
/*
Read data
path: path */
esp_err_t s_example_read_file(const char *path,char *pxbuf,uint32_t *outLen)
{
  esp_err_t err;
  if(card_host == NULL)
  {
    printf("path:card == NULL\n");
    return ESP_ERR_NOT_FOUND;
  }
  err = sdmmc_get_status(card_host); //First, check if there is an SD card
  if(err != ESP_OK)
  {
    printf("path:card == NO\n");
    return err;
  }
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    printf("path:Read Wrong path\n");
    return ESP_ERR_NOT_FOUND;
  }
  fseek(f, 0, SEEK_END);     //Move the pointer to the very end.
  uint32_t unlen = ftell(f);
  //fgets(pxbuf, unlen, f); 
  fseek(f, 0, SEEK_SET); //read characters from file
  uint32_t poutLen = fread((void *)pxbuf,1,unlen,f);
  //printf("pxlen: %ld,outLen: %ld\n",unlen,poutLen);
  if(outLen != NULL)
  *outLen = poutLen;
  fclose(f);
  return ESP_OK;
}