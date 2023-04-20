#pragma once

#define SERIAL_NUMBER 0001

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

Adafruit_MPU6050 mpu;

WiFiServer Server(23);
WiFiClient Client; // one client should be able to telnet to this ESP32
const char *ssid = "AVL9-19";
const char *password = "baku2020";


//#define WDT_TIMEOUT 2
//#include <esp_task_wdt.h>

void task_button(void *pvParameters);
void show_display(void *pvParameters);
void getweight1(void *pvParameters);
void getweight2(void *pvParameters);
void get_final_weight(void *pvParameters);
void get_time(void *pvParameters);
void show_current_weight(void *pvParameters);
void barcode_scanner(void *pvParameters);
void gyroscope_data(void *pvParameters);
void telnet_server(void *pvParameters);


 
