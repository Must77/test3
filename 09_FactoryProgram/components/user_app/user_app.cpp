#include <stdio.h>
#include "user_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "button_bsp.h"
#include "sdcard_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "esp_log.h"
#include "adc_bsp.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "ble_scan_bsp.h"
#include "esp_wifi_bsp.h"
#include "sdmmc_cmd.h"
#include "user_config.h"

extern sdmmc_card_t *card_host;  // Extern declaration from sdcard_bsp.c

lv_ui user_ui;

// SD card image file path constant
#define SDCARD_IMAGE_PATH "S:/sdcard/1.jpg"

// Screen dimensions (from user_config.h)
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH EXAMPLE_LCD_H_RES
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT EXAMPLE_LCD_V_RES
#endif

// Function to load and display image from SD card
void load_and_display_sdcard_image(lv_ui *ui);

void user_color_task(void *arg);
void example_user_task(void *arg);
void example_sdcard_task(void *arg);
void example_button_task(void *arg);
void example_scan_wifi_ble_task(void *arg);
void User_LCD_Before_Init(void)
{
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  i2c_master_Init();
}
void User_LCD_After_Init(void)
{
  setup_ui(&user_ui);
  user_button_init();
  _sdcard_init();
  adc_bsp_init();
  i2c_rtc_setup();
  i2c_rtc_setTime(2025,6,20,19,1,30);
  i2c_qmi_setup();
  espwifi_init();
  xTaskCreatePinnedToCore(user_color_task, "user_color_task", 3 * 1024, &user_ui , 2, NULL,0); //color
  xTaskCreatePinnedToCore(example_sdcard_task, "example_sdcard_task", 3 * 1024, &user_ui, 2, NULL,0);  // sd
  xTaskCreatePinnedToCore(example_user_task, "example_user_task", 3 * 1024, &user_ui, 2, NULL,0);   // user
  xTaskCreatePinnedToCore(example_button_task, "example_button_task", 3000, (void *)&user_ui, 2, NULL,0);   
  xTaskCreatePinnedToCore(example_scan_wifi_ble_task, "example_scan_wifi_ble_task", 3000, (void *)&user_ui, 2, NULL,0);   
}
void example_scan_wifi_ble_task(void *arg)
{
  lv_ui *Send_ui = (lv_ui *)arg;
  char send_lvgl[50] = {""};
  uint8_t ble_scan_count = 0;
  uint8_t ble_mac[6];
  EventBits_t even = xEventGroupWaitBits(wifi_even_,0x02,pdTRUE,pdTRUE,pdMS_TO_TICKS(30000)); 
  espwifi_deinit(); //释放WIFI
  ble_scan_prepare();
  ble_stack_init();
  ble_scan_start();
  for(;xQueueReceive(ble_queue,ble_mac,3500) == pdTRUE;)
  {
    //ESP_LOGI(TAG, "%d",connt);
    ble_scan_count++;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if(READ_BIT(even,1))
  {
    snprintf(send_lvgl,45,"ble : %d wifi : %d",ble_scan_count,user_esp_bsp.apNum);
  }
  else
  {
    snprintf(send_lvgl,45,"ble : %d wifi : P",ble_scan_count);
  }
  lv_label_set_text(Send_ui->screen_label_8, send_lvgl);
  ble_stack_deinit();//释放BLE
  vTaskDelete(NULL);
}

// Function to load and display image from SD card
void load_and_display_sdcard_image(lv_ui *ui)
{
  static const char *TAG = "ImageLoader";
  
  // Check if SD card is available
  if(card_host == NULL)
  {
    ESP_LOGE(TAG, "SD card not initialized");
    return;
  }
  
  // Check if image object already exists, if not create it
  if(ui->screen_img_sdcard == NULL)
  {
    ui->screen_img_sdcard = lv_img_create(ui->screen);
    if(ui->screen_img_sdcard == NULL)
    {
      ESP_LOGE(TAG, "Failed to create image object (out of memory)");
      return;
    }
    lv_obj_set_pos(ui->screen_img_sdcard, 0, 0);
    lv_obj_set_size(ui->screen_img_sdcard, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_add_flag(ui->screen_img_sdcard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_img_opa(ui->screen_img_sdcard, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
  }
  
  // Try to load the image from SD card
  // LVGL uses filesystem with letter prefix, 'S' = 83 for STDIO
  // Note: In LVGL 8.x, lv_img_set_src returns void, so we can't check return value
  // The image will show error symbol if file doesn't exist or is invalid
  ESP_LOGI(TAG, "Attempting to load image from SD card: %s", SDCARD_IMAGE_PATH);
  lv_img_set_src(ui->screen_img_sdcard, SDCARD_IMAGE_PATH);
  
  // Show the image and hide carousel
  lv_obj_clear_flag(ui->screen_img_sdcard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_carousel_1, LV_OBJ_FLAG_HIDDEN);
  
  ESP_LOGI(TAG, "Image display command sent. If image doesn't show, check:");
  ESP_LOGI(TAG, "  1. SD card is properly inserted and formatted");
  ESP_LOGI(TAG, "  2. File /sdcard/1.jpg exists");
  ESP_LOGI(TAG, "  3. Image format is valid JPEG");
}

void example_button_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  uint8_t ui_over = 2;
  uint8_t bl_test = 255;
  uint32_t sdcard_test = 0;
  char sdcard_send_buf[50] = {""};
  char sdcard_read_buf[50] = {""};
  uint8_t even_set_bit = 0;
  uint8_t image_displayed = 0;  // Flag to track if image is displayed
  SET_BIT(even_set_bit,0);
  SET_BIT(even_set_bit,1);
  SET_BIT(even_set_bit,5);
  for(;;)
  {
    EventBits_t even = xEventGroupWaitBits(key_groups,even_set_bit,pdTRUE,pdFALSE,pdMS_TO_TICKS(2500));
    if(READ_BIT(even,0))    //单击
    {
      // If image is currently displayed, hide it and return to carousel
      if(image_displayed)
      {
        // Hide image and show carousel again
        if(ui->screen_img_sdcard != NULL)
        {
          lv_obj_add_flag(ui->screen_img_sdcard, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(ui->screen_carousel_1, LV_OBJ_FLAG_HIDDEN);
        image_displayed = 0;
        // Reset to main interface page when returning from image
        ui_over = 2;
      }
      // If we're in the main interface (page 2) and image not displayed, load image
      else if(ui_over == 2)
      {
        load_and_display_sdcard_image(ui);
        image_displayed = 1;
      }
      else
      {
        // Normal carousel navigation when not displaying image
        switch (ui_over)
        {
          case 2:
            ui_over = 3;
            lv_obj_scroll_by(ui->screen_carousel_1, -SCREEN_WIDTH, 0, LV_ANIM_ON);
            break;
          case 3:
            ui_over = 4;
            lv_obj_scroll_by(ui->screen_carousel_1, -SCREEN_WIDTH, 0, LV_ANIM_ON);
            break;
          case 4:
            ui_over = 5;
            lv_obj_scroll_by(ui->screen_carousel_1, SCREEN_WIDTH, 0, LV_ANIM_ON);
            break;
          case 5:
            ui_over = 2;
            lv_obj_scroll_by(ui->screen_carousel_1, SCREEN_WIDTH, 0, LV_ANIM_ON);
            break;
          default:
            break;
        }
      }
    }
    else if(READ_BIT(even,1))  //双击
    {
      switch (bl_test)
      {
        case 255:
          bl_test = 0;
          setUpduty(LCD_PWM_MODE_0);
          break;
        case 0:
          bl_test = 255;
          setUpduty(LCD_PWM_MODE_255);
          break;
        default:
          break;
      }
    }
    else if(READ_BIT(even,5))  //长按
    {
      sdcard_test++;
      snprintf(sdcard_send_buf,50,"China is the greatest country : %ld",sdcard_test);
      sdcard_file_write("/sdcard/Test.txt",sdcard_send_buf);
      sdcard_file_read("/sdcard/Test.txt",sdcard_read_buf,NULL);
      if(!strcmp(sdcard_send_buf,sdcard_read_buf))
      {
        ESP_LOGI("sdcardTest", "sd card Test pass");
        lv_label_set_text(ui->screen_label_6, "sd Test Pass");
      }
      else
      {
        lv_label_set_text(ui->screen_label_6, "sd Test Fail");
      }
    }
    else
    {
      lv_label_set_text(ui->screen_label_6, "");
    }
  }
}
void example_sdcard_task(void *arg)
{
  lv_ui *Send_ui = (lv_ui *)arg;
  char send_lvgl[50] = {""};
  EventBits_t even = xEventGroupWaitBits(sdcard_even_,0x01,pdTRUE,pdTRUE,pdMS_TO_TICKS(15000)); //等待sdcard 成功
  if( READ_BIT(even,0) )
  {
    snprintf(send_lvgl,45,"sdcard : %.2fG",user_sdcard_bsp.sdcard_size);
    lv_label_set_text(Send_ui->screen_label_3,send_lvgl);
  }
  else
  {
    lv_label_set_text(Send_ui->screen_label_3,"null");
  }
  vTaskDelete(NULL);
}

void user_color_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  lv_obj_clear_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //不可移动
  lv_obj_clear_flag(ui->screen_img_1,LV_OBJ_FLAG_HIDDEN);  //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_2,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_3,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_add_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //可移动
  lv_obj_scroll_by(ui->screen_carousel_1,-320,0,LV_ANIM_ON);//向左滑动320
  vTaskDelete(NULL); //删除任务
}


void example_user_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  uint32_t stimes = 0;
  uint32_t rtc_time = 0;
  uint32_t qmi_time = 0;
  uint32_t adc_time = 0;
  char rtc_send_buf[50] = {0};
  char imu_send_buf[50] = {0};
  uint8_t imu_flag = 0;
  char adc_send_buf[30] = {0};
  float adc_value = 0;
  for(;;)
  {
    if(stimes - rtc_time > 4) //1s
    {
      rtc_time = stimes;
      RtcDateTime_t rtc_data = i2c_rtc_get();
      snprintf(rtc_send_buf,45,"rtc : \n%d/%d/%d\n%02d:%02d:%02d",rtc_data.year,rtc_data.month,rtc_data.day,rtc_data.hour,rtc_data.minute,rtc_data.second);
      lv_label_set_text(ui->screen_label_4, rtc_send_buf);
    }
    if(stimes - qmi_time > 4) //1s
    {
      qmi_time = stimes;
      ImuDate_t imu_data = i2c_imu_get();
      if(imu_flag == 0)
      {snprintf(imu_send_buf,50,"acc : \n%.2fg \n%.2fg \n%.2fg",imu_data.accx,imu_data.accy,imu_data.accz);imu_flag = 1;}
      else
      {snprintf(imu_send_buf,50,"gyro : \n%.2fdps \n%.2fdps \n%.2fdps",imu_data.gyrox,imu_data.gyroy,imu_data.gyroz);imu_flag = 0;}
      lv_label_set_text(ui->screen_label_5, imu_send_buf);
    }
    if(stimes - adc_time > 9) //2s
    {
      adc_time = stimes;
      adc_get_value(&adc_value,NULL);
      if(adc_value)
      {
        snprintf(adc_send_buf,30,"vbat : %.2fV",adc_value);
        lv_label_set_text(ui->screen_label_7, adc_send_buf);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    stimes++;
  }
}