#include "HX711.h"
#include <Arduino.h> 
#include <U8g2lib.h>
//#include <esp_task_wdt.h>
//SCLK = 18, MISO = 19, MOSI = 23, SS = 5 
#include <SPI.h>
#include "RTClib.h"

//#define WDT_TIMEOUT 2

#define BUTTON_UP 34
#define BUTTON_DOWN 35
#define BUTTON_RIGHT 2
#define BUTTON_LEFT 4
#define CALC_ARRAY_SIZE 900

void start_screen(void *pvParameters);
void task_button(void *pvParameters);
void show_display(void *pvParameters);
void getweight1(void *pvParameters);
void getweight2(void *pvParameters);
void get_final_weight(void *pvParameters);
void get_time(void *pvParameters);
void set_calibration_factor(void *pvParameters);
void autocalibration(void *pvParameters);
 
