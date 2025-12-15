idf_component_register(
  SRCS "user_app.cpp"
  PRIV_REQUIRES sdcard_bsp button_bsp ble_scan_bsp esp_wifi_bsp i2c_equipment lcd_bl_pwm_bsp esp_timer ui_bsp adc_bsp i2c_bsp
  INCLUDE_DIRS "./")
