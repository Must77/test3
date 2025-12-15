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
#include "user_config.h"
#include <sys/stat.h>
#include <vector>
#include "freertos/semphr.h"
lv_ui user_ui;

void user_color_task(void *arg);
void example_user_task(void *arg);
void example_sdcard_task(void *arg);
void example_button_task(void *arg);
void example_scan_wifi_ble_task(void *arg);
static void show_rgb_sequence(lv_ui *ui);
static void log_sd_test_file(void);
static void load_and_display_jpg(lv_ui *ui);
static const char *TAG_BTN = "boot_key";
static const char *TAG_SDCARD_LOG = "sdcard_meta";
static const char *TAG_JPG = "jpg_loader";
static const char *SD_ROOT_PATH = "/sdcard/";
static const char *TEST_FILE_NAME = "TEST.TXT";
static const char *DEFAULT_JPG_FILE_NAME = "1.jpg";
static constexpr size_t SD_PATH_BUF_LEN = 256;
static SemaphoreHandle_t rgb_mutex = NULL;
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
void example_button_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  uint8_t bl_test = 255;
  uint32_t sdcard_test = 0;
  char sdcard_send_buf[50] = {""};
  char sdcard_read_buf[50] = {""};
  char test_path[SD_PATH_BUF_LEN] = {0};
  snprintf(test_path, sizeof(test_path), "%s%s", SD_ROOT_PATH, TEST_FILE_NAME);
  uint8_t even_set_bit = 0;
  SET_BIT(even_set_bit,0);
  SET_BIT(even_set_bit,1);
  SET_BIT(even_set_bit,5);
  for(;;)
  {
    EventBits_t even = xEventGroupWaitBits(key_groups,even_set_bit,pdTRUE,pdFALSE,pdMS_TO_TICKS(2500));
    if(READ_BIT(even,0))    //单击
    {
      ESP_LOGI(TAG_BTN, "BOOT single click detected");
      show_rgb_sequence(ui);
      load_and_display_jpg(ui);
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
      sdcard_file_write(test_path,sdcard_send_buf);
      sdcard_file_read(test_path,sdcard_read_buf,NULL);
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
    log_sd_test_file();
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
  show_rgb_sequence(ui);
  lv_obj_add_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //可移动
  lv_obj_scroll_by(ui->screen_carousel_1,-320,0,LV_ANIM_ON);//向左滑动320
  vTaskDelete(NULL); //删除任务
}

static void show_rgb_sequence(lv_ui *ui)
{
  if(ui == NULL)
  {
    return;
  }
  if(rgb_mutex == NULL)
  {
    rgb_mutex = xSemaphoreCreateMutex();
    if(rgb_mutex == NULL)
    {
      ESP_LOGE(TAG_BTN, "RGB mutex init failed");
      return;
    }
  }
  if(xSemaphoreTake(rgb_mutex, 0) != pdTRUE)
  {
    ESP_LOGW(TAG_BTN, "RGB sequence already running, skip new request");
    return;
  }
  ESP_LOGI(TAG_BTN, "Start RGB sequence on screen");
  lv_obj_clear_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //不可移动
  lv_obj_clear_flag(ui->screen_img_1,LV_OBJ_FLAG_HIDDEN);  //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  ESP_LOGI(TAG_BTN, "Showing RED frame");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_2,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  ESP_LOGI(TAG_BTN, "Showing GREEN frame");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_3,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  ESP_LOGI(TAG_BTN, "Showing BLUE frame");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_add_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //可移动
  xSemaphoreGive(rgb_mutex);
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

static void log_sd_test_file(void)
{
  char test_path[SD_PATH_BUF_LEN] = {0};
  snprintf(test_path, sizeof(test_path), "%s%s", SD_ROOT_PATH, TEST_FILE_NAME);
  struct stat test_stat = {};
  if(stat(test_path, &test_stat) != 0)
  {
    ESP_LOGE(TAG_SDCARD_LOG, "STAT fail for %s", test_path);
    return;
  }
  ESP_LOGI(TAG_SDCARD_LOG, "File:%s size:%ld bytes mode:0%o", test_path, (long)test_stat.st_size, test_stat.st_mode);
  size_t alloc_len = test_stat.st_size + 1;
  std::vector<char> file_buf(alloc_len, 0);
  size_t read_len = 0;
  if(sdcard_file_read(test_path, file_buf.data(), &read_len) == ESP_OK)
  {
    size_t max_len = (file_buf.size() > 0) ? (file_buf.size() - 1) : 0;
    bool truncated = read_len > max_len;
    size_t capped_len = truncated ? max_len : read_len;
    file_buf[capped_len] = '\0';
    if(truncated)
    {
      ESP_LOGW(TAG_SDCARD_LOG, "Content truncated to %d bytes (raw len %d)", (int)capped_len, (int)read_len);
    }
    ESP_LOGI(TAG_SDCARD_LOG, "Content(%d bytes): %s", (int)capped_len, file_buf.data());
  }
  else
  {
    ESP_LOGE(TAG_SDCARD_LOG, "Read fail for %s", test_path);
  }
}

static void load_and_display_jpg(lv_ui *ui)
{
  if(ui == NULL)
  {
    return;
  }
  char fs_path[SD_PATH_BUF_LEN] = {0};
  char lv_path[SD_PATH_BUF_LEN] = {0};
  if(ui->screen_img_4 == NULL || ui->screen_carousel_1_element_3 == NULL)
  {
    ESP_LOGE(TAG_JPG, "Target image widgets not ready");
    return;
  }
  snprintf(fs_path, sizeof(fs_path), "%s%s", SD_ROOT_PATH, DEFAULT_JPG_FILE_NAME);
  snprintf(lv_path, sizeof(lv_path), "S:%s", DEFAULT_JPG_FILE_NAME);
  struct stat jpg_stat = {};
  if(stat(fs_path, &jpg_stat) != 0)
  {
    ESP_LOGE(TAG_JPG, "STAT fail for %s", fs_path);
    return;
  }
  ESP_LOGI(TAG_JPG, "JPEG %s size:%ld bytes mode:0%o", fs_path, (long)jpg_stat.st_size, jpg_stat.st_mode);
  lv_img_header_t header;
  lv_res_t res = lv_img_decoder_get_info(lv_path, &header);
  if(res == LV_RES_OK)
  {
    ESP_LOGI(TAG_JPG, "LVGL decoded %s -> w:%d h:%d cf:%d", lv_path, header.w, header.h, header.cf);
  }
  else
  {
    ESP_LOGE(TAG_JPG, "LVGL decoder failed for %s", lv_path);
    return;
  }
  lv_obj_clear_flag(ui->screen_img_4, LV_OBJ_FLAG_HIDDEN);
  lv_img_set_src(ui->screen_img_4, lv_path);
  lv_obj_set_pos(ui->screen_img_4, 0, 0);
  lv_obj_set_size(ui->screen_img_4, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
  lv_obj_scroll_to_view(ui->screen_carousel_1_element_3, LV_ANIM_ON);
  ESP_LOGI(TAG_JPG, "JPEG shown on carousel element 3 using %s", lv_path);
}
