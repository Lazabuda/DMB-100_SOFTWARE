#pragma once

#define SERIAL_NUMBER 0001

//***************************************************************//
//*---------------------------DEFINES----------------------------//
//***************************************************************//

//*-------------------------------------------------------------*//
//*-----------------!!!!!SET THE DEFINES!!!!!!!-----------------*//
//*-------------------------------------------------------------*//
//   Additional service processes defines

//#define WITHOUT_TARE
//#define SERIAL_FOR_DEBUG // Uncomment for see SERIAL DEBUG MESSAGES

#define BARCODE_DATA_SIZE 500                       // <---------- Barcode symbols quantity. Change if you need more
#define GYRO_DATA_SIZE 500                          // <---------- Data symbols from gyroscope quantity. Change if you need more
#define CALC_ARRAY_SIZE 500                         // <---------- Array size for median calculation of weight. MAX = 900
#define COMPENSATION_WEIGHT 7.77                    // <---------- The weight of compensation sample. 20 g by default.

//*--------------------------------------------*//
//*---ENTER YOUR WIFI SSID AND PASSWORD HERE---*//
//*--------------------------------------------*//
const static char ssid_name[] = "AVL9-19";          // <---------- WiFi SSID
const static char password_name[] = "baku2020";     // <---------- WiFi PASSWORD
//*--------------------------------------------*//



//*-------------------------------------------------------------*//
//*------------------EVENTS (FLAGS) DEFINES---------------------*//
//*-------------------------------------------------------------*//
#define WEIGHTING BIT0 // Allow Weighting flag
#define SD_CARD_ERROR BIT1 // SD Card error flag
#define READING1_OOR_ERROR BIT2 // Reading 1 out of range error flag
#define BARDODE_DATA_FLAG BIT3 // Is data in barcode scanner buffer
#define FINAL_WEIGHT BIT4 // Final weight enable flag
#define SERVICE_MODE BIT5 // Service mode flag
#define WIFI_FLAG BIT6 // If WiFi has been initialized, FLAG = 1
#define WIFI_DATA BIT7 // If data is transmitting, FLAG = 1
#define RTC_ERROR BIT8 // Real Time Clock error flag
#define QUEUE_ERROR BIT9 // Data from load cells ERROR FLAG
#define EMPTY_SCALE BIT10 // Flag, which is "1" if scale for weighting samples is emty. "0" - if sample on the scale.
#define CALIBRATION_FLAG BIT11 // If this flag = "1", it means, that reset happened and you need to recalibrate scales

//*-------------------------------------------------------------*//
//*---------------------PINs SET DEFINES------------------------*//
//*-------------------------------------------------------------*//
// BUTTUNS DEFINES
#define BUTTON_UP 34
#define BUTTON_DOWN 35
#define BUTTON_RIGHT 2
#define BUTTON_LEFT 4
// UART2 DEFINES
#define RXD2 16
#define TXD2 17

//*-------------------------------------------------------------*//
//*-------------------------INCLUDES----------------------------*//
//*-------------------------------------------------------------*//

#include "HX711.h"
#include <Arduino.h> 
#include <U8g2lib.h>
//SCLK = 18, MISO = 19, MOSI = 23, SS = 5 
#include <SPI.h>
#include "RTClib.h"
#include <stdlib.h>
#include "SD.h"
#include <EEPROM.h>
#include "FS.h"
#include "WiFi.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>


//***************************************************************//
//*---------------------FreeRTOS tasks---------------------------//
//***************************************************************//


void task_button(void *pvParameters);
void show_display(void *pvParameters);
//void getweight1(void *pvParameters);
//void getweight2(void *pvParameters);
void getweight(void *pvParameters);
void get_final_weight(void *pvParameters);
void get_time(void *pvParameters);
void show_current_weight(void *pvParameters);
void barcode_scanner(void *pvParameters);
void gyroscope_data(void *pvParameters);
void telnet_server(void *pvParameters);


//***************************************************************//
//*----------------Non-FreeRTOS functions------------------------//
//***************************************************************//

int cmpfunc (const void * a, const void * b);
void start_page();
void second_page();
void median_calc();
void append_data_to_log();
void write_log_header(fs::FS &fs, const char * path);