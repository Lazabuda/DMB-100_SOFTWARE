#pragma once

//---------------------INCLUDES---------------------------//
#include "HX711.h"
#include <Arduino.h> 
#include <U8g2lib.h>
#include <SPI.h>
#include "RTClib.h"
#include <stdlib.h>
#include "SD.h"
#include <EEPROM.h>
#include "FS.h"
//#include "RTOS_tasks.h"

//---------------------DEFINES---------------------------//
#define BARCODE_DATA_SIZE 50
#define CALC_ARRAY_SIZE 700 //MAX = 900

#define BUTTON_UP 34
#define BUTTON_DOWN 35
#define BUTTON_RIGHT 2
#define BUTTON_LEFT 4


#define RXD2 16
#define TXD2 17

#define COMPENSATION_WEIGHT 7.77 // The weight of compensation sample. 20 g by default.

const int CS = 5;

extern char date_time[50];
extern double reading1;
extern double reading2;
extern double reading1print;
extern double reading2print;
extern double final_weight;
extern double current_weight;
extern double coefficient;
extern char barcode_data[BARCODE_DATA_SIZE];
extern int error_flag;

extern U8G2_ST7565_ERC12864_1_4W_SW_SPI u8g2;


//---------------------DEFINE COMMON FUNCTIONS---------------------------//
int cmpfunc (const void * a, const void * b);
void start_page();
void second_page();
void average_calc();
void median_calc();
void append_data_to_log();
void write_log_header(fs::FS &fs, const char * path);
int set_bit(int num_bit);
int is_bit_set(int num_bit);
void is_error();