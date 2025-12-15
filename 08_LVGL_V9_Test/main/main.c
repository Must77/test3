#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_timer.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "user_config.h"
#include "lcd_bl_pwm_bsp.h"
static SemaphoreHandle_t lvgl_mux = NULL;
static SemaphoreHandle_t flush_done_semaphore = NULL;

static void example_backlight_loop_task(void *arg);

uint8_t *lvgl_dest = NULL;

#define EXAMPLE_LCD_BIT_PER_PIXEL 16


#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * BYTES_PER_PIXEL)

static const st7701_lcd_init_cmd_t lcd_init_cmds[] = 
{
  //   cmd   data        data_size  delay_ms 1
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x13},5,0},
  {0xEF,(uint8_t []){0x08},1,0},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x10},5,0},
  {0xC0,(uint8_t []){0xE5,0x02},2,0},
  {0xC1,(uint8_t []){0x15,0x0A},2,0},
  {0xC2,(uint8_t []){0x07,0x02},2,0},
  {0xCC,(uint8_t []){0x10},1,0},
  {0xB0,(uint8_t []){0x00,0x08,0x51,0x0D,0xCE,0x06,0x00,0x08,0x08,0x24,0x05,0xD0,0x0F,0x6F,0x36,0x1F},16,0},
  {0xB1,(uint8_t []){0x00,0x10,0x4F,0x0C,0x11,0x05,0x00,0x07,0x07,0x18,0x02,0xD3,0x11,0x6E,0x34,0x1F},16,0},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x11},5,0},
  {0xB0,(uint8_t []){0x4D},1,0},
  {0xB1,(uint8_t []){0x37},1,0},
  {0xB2,(uint8_t []){0x87},1,0},
  {0xB3,(uint8_t []){0x80},1,0},
  {0xB5,(uint8_t []){0x4A},1,0},
  {0xB7,(uint8_t []){0x85},1,0},
  {0xB8,(uint8_t []){0x21},1,0},
  {0xB9,(uint8_t []){0x00,0x13},2,0},
  {0xC0,(uint8_t []){0x09},1,0},
  {0xC1,(uint8_t []){0x78},1,0},
  {0xC2,(uint8_t []){0x78},1,0},
  {0xD0,(uint8_t []){0x88},1,0},
  {0xE0,(uint8_t []){0x80,0x00,0x02},3,100},
  {0xE1,(uint8_t []){0x0F,0xA0,0x00,0x00,0x10,0xA0,0x00,0x00,0x00,0x60,0x60},11,0},
  {0xE2,(uint8_t []){0x30,0x30,0x60,0x60,0x45,0xA0,0x00,0x00,0x46,0xA0,0x00,0x00,0x00},13,0},
  {0xE3,(uint8_t []){0x00,0x00,0x33,0x33},4,0},
  {0xE4,(uint8_t []){0x44,0x44},2,0},
  {0xE5,(uint8_t []){0x0F,0x4A,0xA0,0xA0,0x11,0x4A,0xA0,0xA0,0x13,0x4A,0xA0,0xA0,0x15,0x4A,0xA0,0xA0},16,0},
  {0xE6,(uint8_t []){0x00,0x00,0x33,0x33},4,0},
  {0xE7,(uint8_t []){0x44,0x44},2,0},
  {0xE8,(uint8_t []){0x10,0x4A,0xA0,0xA0,0x12,0x4A,0xA0,0xA0,0x14,0x4A,0xA0,0xA0,0x16,0x4A,0xA0,0xA0},16,0},
  {0xEB,(uint8_t []){0x02,0x00,0x4E,0x4E,0xEE,0x44,0x00},7,0},
  {0xED,(uint8_t []){0xFF,0xFF,0x04,0x56,0x72,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x27,0x65,0x40,0xFF,0xFF},16,0},
  {0xEF,(uint8_t []){0x08,0x08,0x08,0x40,0x3F,0x64},6,0},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x13},5,0},
  {0xE8,(uint8_t []){0x00,0x0E},2,0},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x00},5,0},
  {0x11,(uint8_t []){0x00},0,120},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x13},5,0},
  {0xE8,(uint8_t []){0x00,0x0C},2,10},
  {0xE8,(uint8_t []){0x00,0x00},2,0},
  {0xFF,(uint8_t []){0x77,0x01,0x00,0x00,0x00},5,0},
  {0x3A,(uint8_t []){0x55},1,0},
  {0x36,(uint8_t []){0x00},1,0},
  {0x35,(uint8_t []){0x00},1,0},
  {0x29,(uint8_t []){0x00},0,20},

};
static void example_increase_lvgl_tick(void *arg)
{
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
#ifdef EXAMPLE_Rotate_90
  lv_display_rotation_t rotation = lv_display_get_rotation(disp);
  lv_area_t rotated_area;
  if(rotation != LV_DISPLAY_ROTATION_0)
  {
    lv_color_format_t cf = lv_display_get_color_format(disp);
    /*Calculate the position of the rotated area*/
    rotated_area = *area;
    lv_display_rotate_area(disp, &rotated_area);
    /*Calculate the source stride (bytes in a line) from the width of the area*/
    uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
    /*Calculate the stride of the destination (rotated) area too*/
    uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
    /*Have a buffer to store the rotated area and perform the rotation*/
    
    int32_t src_w = lv_area_get_width(area);
    int32_t src_h = lv_area_get_height(area);
    lv_draw_sw_rotate(color_p, lvgl_dest, src_w, src_h, src_stride, dest_stride, rotation, cf);
    /*Use the rotated area and rotated buffer from now on*/
    area = &rotated_area;
  }
  esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2+1, area->y2+1, lvgl_dest);
#else
  esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2+1, area->y2+1, color_p);
#endif
}
static bool example_lvgl_lock(int timeout_ms)
{
  assert(lvgl_mux && "bsp_display_start must be called first");

  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}
static void example_lvgl_unlock(void)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  xSemaphoreGive(lvgl_mux);
}
static void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (example_lvgl_lock(-1))
    {
      task_delay_ms = lv_timer_handler();
      // Release the mutex
      example_lvgl_unlock();
    }
    if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    }
    else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}
static bool example_on_bounce_frame_finish_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) //刷新完成触发LVGL刷新新一帧
{
  BaseType_t high_task_awoken = pdFALSE;
  xSemaphoreGiveFromISR(flush_done_semaphore, &high_task_awoken);
  return high_task_awoken == pdTRUE;
}
void lvgl_flush_wait_cb(lv_display_t * disp) //等待RGB发送数据完成,使用lvgl_flush_wait_cb 不需要再使用lv_disp_flush_ready(disp);
{
  xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
}
void app_main(void)
{
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
  flush_done_semaphore = xSemaphoreCreateBinary();
  assert(flush_done_semaphore);
  spi_line_config_t line_config = 
  {
    .cs_io_type = IO_TYPE_GPIO,             // Set to `IO_TYPE_GPIO` if using GPIO, same to below
    .cs_gpio_num = EXAMPLE_LCD_IO_SPI_CS,
    .scl_io_type = IO_TYPE_GPIO,
    .scl_gpio_num = EXAMPLE_LCD_IO_SPI_SCK,
    .sda_io_type = IO_TYPE_GPIO,
    .sda_gpio_num = EXAMPLE_LCD_IO_SPI_SDO,
    .io_expander = NULL,                        // Set to NULL if not using IO expander
  };
  esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
  esp_lcd_panel_io_handle_t io_handle = NULL;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

  esp_lcd_rgb_panel_config_t rgb_config = 
  {
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .psram_trans_align = 64,
    .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,
    .num_fbs = 2,
    .data_width = 16,
    .bits_per_pixel = 16,
    .de_gpio_num = EXAMPLE_LCD_IO_RGB_DE,
    .pclk_gpio_num = EXAMPLE_LCD_IO_RGB_PCLK,
    .vsync_gpio_num = EXAMPLE_LCD_IO_RGB_VSYNC,
    .hsync_gpio_num = EXAMPLE_LCD_IO_RGB_HSYNC,
    .flags.fb_in_psram = true,
    .disp_gpio_num = -1,
    .data_gpio_nums = 
    { //BGR
      EXAMPLE_LCD_IO_RGB_B0,
      EXAMPLE_LCD_IO_RGB_B1,
      EXAMPLE_LCD_IO_RGB_B2,
      EXAMPLE_LCD_IO_RGB_B3,
      EXAMPLE_LCD_IO_RGB_B4,
      EXAMPLE_LCD_IO_RGB_G0,
      EXAMPLE_LCD_IO_RGB_G1,
      EXAMPLE_LCD_IO_RGB_G2,
      EXAMPLE_LCD_IO_RGB_G3,
      EXAMPLE_LCD_IO_RGB_G4,
      EXAMPLE_LCD_IO_RGB_G5,
      EXAMPLE_LCD_IO_RGB_R0,
      EXAMPLE_LCD_IO_RGB_R1,
      EXAMPLE_LCD_IO_RGB_R2,
      EXAMPLE_LCD_IO_RGB_R3,
      EXAMPLE_LCD_IO_RGB_R4,
    },
    .timings = 
    {
      .pclk_hz = 18 * 1000 * 1000,
      .h_res = EXAMPLE_LCD_H_RES,
      .v_res = EXAMPLE_LCD_V_RES,
      .hsync_back_porch = 30,
      .hsync_front_porch = 30,  //30
      .hsync_pulse_width = 6,
      .vsync_back_porch = 20,   //10-100 40
      .vsync_front_porch = 20,  //10-100 70
      .vsync_pulse_width = 40,
    },
  };
  st7701_vendor_config_t vendor_config = 
  {
    .rgb_config = &rgb_config,
    .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
    .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
    .flags = 
    {
      .mirror_by_cmd = 1,       // Only work when `enable_io_multiplex` is set to 0
      .enable_io_multiplex = 0, /**
                                 * Set to 1 if panel IO is no longer needed after LCD initialization.
                                 * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                 * Please set it to 1 to release the pins.
                                */
    },
  };
  const esp_lcd_panel_dev_config_t panel_config = 
  {
    .reset_gpio_num = EXAMPLE_LCD_IO_RGB_RESET,     // Set to -1 if not use
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
    .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,    // Implemented by LCD command `3Ah` (16/18/24)
    .vendor_config = &vendor_config,
  };         
  esp_lcd_panel_handle_t panel_handle = NULL;
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle));   /**
                                                                                         * Only create RGB when `enable_io_multiplex` is set to 0,
                                                                                         * or initialize st7701 meanwhile
                                                                                       */
  esp_lcd_rgb_panel_event_callbacks_t cbs = {
    .on_bounce_frame_finish = example_on_bounce_frame_finish_event,
  };
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));                                   // Only reset RGB when `enable_io_multiplex` is set to 1, or reset st7701 meanwhile
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));                                    // Only initialize RGB when `enable_io_multiplex` is set to 1, or initialize st7701 meanwhile

  lv_init();
  lv_display_t * disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES); /* 以水平和垂直分辨率（像素）进行基本初始化 */
  lv_display_set_flush_cb(disp, example_lvgl_flush_cb);                          /* 设置刷新回调函数以绘制到显示屏 */
  lv_display_set_flush_wait_cb(disp,lvgl_flush_wait_cb);
  uint8_t *buf_1 = NULL;
  uint8_t *buf_2 = NULL;
  buf_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  buf_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  lv_display_set_buffers(disp, buf_1, buf_2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_user_data(disp, panel_handle);
#ifdef EXAMPLE_Rotate_90
  lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM); //旋转buf
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
#endif
  const esp_timer_create_args_t lvgl_tick_timer_args = 
  {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

  xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL,1); 
  xTaskCreatePinnedToCore(example_backlight_loop_task, "example_backlight_loop_task", 4 * 1024, NULL, 2, NULL,1); 
  if (example_lvgl_lock(-1))
  {
    lv_demo_music();
    //lv_demo_widgets();      /* A widgets example */
    //lv_demo_music();        /* A modern, smartphone-like music player demo. */
    // lv_demo_stress();      /* A stress test for LVGL. */
    //lv_demo_benchmark();    /* A demo to measure the performance of LVGL or to compare different settings. */
    // Release the mutex
    example_lvgl_unlock();
  }
}

static void example_backlight_loop_task(void *arg)
{
  for(;;)
  {
#ifdef  Backlight_Testing
    setUpduty(LCD_PWM_MODE_255);
    vTaskDelay(pdMS_TO_TICKS(2000));
    setUpduty(LCD_PWM_MODE_150);
    vTaskDelay(pdMS_TO_TICKS(2000));
    setUpduty(LCD_PWM_MODE_75);
    vTaskDelay(pdMS_TO_TICKS(2000));
    setUpduty(LCD_PWM_MODE_0);
    vTaskDelay(pdMS_TO_TICKS(2000));
#else
    vTaskDelay(pdMS_TO_TICKS(2000));
#endif
  }
}
