#include "RTOS_tasks.h"

//***************************************************************//
//*---------------VARIABLES, HANDLERS, CLASSES-------------------//
//***************************************************************//

//*-----------------------GYROSCOPE MPU6050---------------------*//
Adafruit_MPU6050 mpu;


//*----------------------------Wi-Fi----------------------------*//
WiFiServer Server(23); // The port number, wich you need to enter in PUTTY (or another software)
WiFiClient Client; // one client should be able to telnet to this ESP32
const char *ssid = SSID;
const char *password = PASSWORD;


//*--------FreeRTOS QUEUES, SEMAPHORES, EVENTS (FLAGS)----------*//
// Queues
static QueueHandle_t reading_queue;

// Assign semaphore
SemaphoreHandle_t btnSemaphore;
volatile SemaphoreHandle_t mutex_wait;

// Events (FLAGS)
static EventGroupHandle_t scales_flags;
static EventBits_t flags;



//*---------------------------DISPLAY---------------------------*//

// DEFINE PINOUTS FOR DISPLAY: scl = 14 // si = 13 // cs = 15 // rs = 12 // rse = 27
U8G2_ST7565_ERC12864_1_4W_SW_SPI u8g2 ( U8G2_R0, /* scl=*/  14 , /* si=*/  13 , /* cs=*/  15 , /* rs=*/  12 , /* rse=*/  27 ) ;


//*-------------------------HX711 ADC---------------------------*//

// DEFINE PINs FOR TWO HX711
const int LOADCELL1_DOUT_PIN = 32;
const int LOADCELL1_SCK_PIN = 33;
const int LOADCELL2_DOUT_PIN = 25;
const int LOADCELL2_SCK_PIN = 26;

// CLASSES
HX711 scale1; // HX711
HX711 scale2; //  HX711 (2)
RTC_PCF8563 rtc; // REAL TIME CLOCK PCF8563

// DEFINE VARIABLES FOR HX711
double reading1;
double reading2;
double reading1print;
double reading2print;
double final_weight;
double current_weight_disp;
double coefficient;
double mas[CALC_ARRAY_SIZE];
double average = 0;

//*-------------------REAL TIME CLOCK---------------------------*//
// DEFINE VARIABLES FOR REAL TIME CLOCK
char date_time [50] = "";


//*-------------------BARCODE SCANNER---------------------------*//
// DEFINE VARIABLES FOR BARCODE SCANNER
char barcode_data[BARCODE_DATA_SIZE] = "";

//*---------------UART INPUT GYROSCOPE DATA---------------------*//
// GYRO DATA
char gyro_data[GYRO_DATA_SIZE] = "";

//*-----------------------BUTTONS-------------------------------*//
// DEFINE BUTTON FLAGS
int left_up_button; // FREE BUTTON
int left_down_button; // CALIBRATE BUTTON
int right_up_button; // SAVE BUTTON
int right_down_button; // WEIGHTING BUTTON


//*-----------------------UNUSED, TO DELETE---------------------*//
//extern const int CS = 5;
//int flag_weighting;
//volatile int flag;
//bool service_mode = false;
// SERVICE VARIABLES
//int task_counter = 0;
//int error_flag = 0x0; //Error bits: 0 bit - SD card error; 1 bit - reading1 is not in the area of the right values
//double current_weight;
//*-------------------------------------------------------------*//


//***************************************************************//
//*--------------------FreeRTOS FUNCTIONS------------------------//
//***************************************************************//

//*-------------------------------------------------------------*//
//*----------------------INTERRUPT HANDLER----------------------*//
//*-------------------------------------------------------------*//

void IRAM_ATTR ISR_btn() // IRAM_ATTR means, that we use RAM (wich more faster and recommended for interrupts).ISR - interrupt service routine
// Macro to release a semaphore from interruption. The semaphore must have previously been created with a call to 
//xSemaphoreCreateBinary() or xSemaphoreCreateCounting().
{
  xSemaphoreGiveFromISR( btnSemaphore, NULL ); 
}

//*-------------------------------------------------------------*//
//*-------------------BUTTONS FREERTOS TASK---------------------*//
//*-------------------------------------------------------------*//

void task_button(void *pvParameters) // create button RTOS task
{
  bool isISR = true;
  bool state_btn1 = true, state_btn2 = true, state_btn3 = true, state_btn4 = true; // assign four buttons
  
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);

  btnSemaphore = xSemaphoreCreateBinary(); // create simple binary semaphore

  xSemaphoreTake(btnSemaphore, 100); // taking semaphore to avoid false trigger of the button

  attachInterrupt(BUTTON_DOWN, ISR_btn, FALLING); // launch four interrupt handlers (button short-circuit to GND)
  attachInterrupt(BUTTON_UP, ISR_btn, FALLING);
  attachInterrupt(BUTTON_RIGHT, ISR_btn, FALLING);
  attachInterrupt(BUTTON_LEFT, ISR_btn, FALLING);

  while (true)
  {
    if (isISR) // interrupt handlers 
    {
      xSemaphoreTake(btnSemaphore, portMAX_DELAY);
      detachInterrupt(BUTTON_DOWN); //Turns off the given interrupt.
      detachInterrupt(BUTTON_UP);
      detachInterrupt(BUTTON_RIGHT);
      detachInterrupt(BUTTON_LEFT);
      isISR = false;
    }
    else
    {
      bool st1 = digitalRead(BUTTON_DOWN);
      bool st2 = digitalRead(BUTTON_UP);
      bool st3 = digitalRead(BUTTON_RIGHT);
      bool st4 = digitalRead(BUTTON_LEFT);

      if  (st1 != state_btn1)
      {
        state_btn1 = st1;
        if (st1 == LOW) 
          right_down_button = 1;
        else 
          right_down_button = 0;
      }

      if  (st2 != state_btn2)
      {
        state_btn2 = st2;
        if (st2 == LOW) 
          right_up_button = 1;
        else
          right_up_button = 0;
      }

      if  (st3 != state_btn3)
      {
        state_btn3 = st3;
        if (st3 == LOW) 
          left_up_button = 1;
        else 
          left_up_button = 0;
      }

      if  (st4 != state_btn4)
      {
        state_btn4 = st4;
        if (st4 == LOW) 
          left_down_button = 1;
        else 
          left_down_button = 0;
      }
      if (st1 == HIGH && st2 == HIGH && st3 == HIGH && st4 == HIGH)
      {
        attachInterrupt(BUTTON_DOWN, ISR_btn, FALLING);
        attachInterrupt(BUTTON_UP, ISR_btn, FALLING);
        attachInterrupt(BUTTON_RIGHT, ISR_btn, FALLING);
        attachInterrupt(BUTTON_LEFT, ISR_btn, FALLING);
        isISR = true;
      }
      vTaskDelay(100 / portTICK_PERIOD_MS); // this function calls the task manager, which set this task to WAIT mode for 100 system ticks.
    }
  }
}


//*-------------------------------------------------------------*//
//*-------------------DISPLAY FREERTOS TASK---------------------*//
//*-------------------------------------------------------------*//

void show_display(void *pvParameters) // create display menu task
{
  scales_flags = xEventGroupCreate();
  flags = xEventGroupClearBits(scales_flags, WEIGHTING | SD_CARD_ERROR | READING1_OOR_ERROR | BARDODE_DATA_FLAG | FINAL_WEIGHT | \
  WIFI_FLAG | SERVICE_MODE | WIFI_DATA | RTC_ERROR | EMPTY_SCALE);
  Serial.begin(115200);
  u8g2. begin ( ) ;
  u8g2. setContrast  (10) ;
  u8g2. enableUTF8Print ( ) ;
  if(!SD.begin())
  {
    Serial.println("Card Mount Failed");
    //set_bit(0);
    flags  = xEventGroupSetBits(scales_flags, SD_CARD_ERROR );
  }
  write_log_header(SD, "/log.csv");
  int i = 0;
  while (true)
  {
    flags = xEventGroupGetBits(scales_flags);
    if (i < 1)
    {
      start_page();
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      i++;
    }
    if (i < 2)
    {
      if ((flags & SERVICE_MODE) != SERVICE_MODE)
      //if (service_mode == false)
      {
#ifndef DEBUG
        second_page();
        vTaskDelay(5000);
#endif
      }      
      coefficient = reading1/reading2; 
      Serial.print("Reading 1 value - ");
      Serial.println(reading1);
      Serial.print("Reading 2 value - ");
      Serial.println(reading2);
      Serial.print("Coefficient value - ");
      Serial.println(coefficient, 4);
      if (coefficient > 0.85 && coefficient < 0.75)
        flags = xEventGroupSetBits(scales_flags, CALIBRATION_FLAG );
      reading1print = reading1;
      reading2print = reading2;
      i++;
    }
    u8g2. firstPage ( ) ;
    do  
    {
      if ((flags & CALIBRATION_FLAG) == CALIBRATION_FLAG)
      {

      }
      if ((flags & RTC_ERROR) != RTC_ERROR)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10); // Width 12, Height 12
        u8g2.drawGlyph(66, 10, 0xE016);
      }
      if ((flags & WIFI_DATA) == WIFI_DATA)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10); // Width 12, Height 12
        u8g2.drawGlyph(44, 10, 0xE12F);
      }
      if ((flags & WIFI_FLAG) == WIFI_FLAG)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10); // Width 12, Height 12
        u8g2.drawGlyph(33, 10, 0xE219);
      }
      //if (is_bit_set(0) == false)
      if ((flags & SD_CARD_ERROR) != SD_CARD_ERROR)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10); // Width 12, Height 12
        u8g2.drawGlyph(55, 10, 0xE1D6);
      }
      //if (is_bit_set(0))
      if ((flags & SD_CARD_ERROR) == SD_CARD_ERROR)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10);
        u8g2.drawGlyph(55, 10, 0xE1C4);
        //u8g2.setCursor(55, 10);
        //u8g2.print("NO SD!");
      }
      if ((flags & SERVICE_MODE) != SERVICE_MODE)
      //if (service_mode == false)
      {
//#ifndef SERVICE_MODE
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 20);
        u8g2.print("coeff = ");

        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(40, 20);
        u8g2.print(coefficient, 4);
//#endif
      }
      if ((flags & SERVICE_MODE) == SERVICE_MODE)
      //if (service_mode == true)
      {
//#ifdef SERVICE_MODE 
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 20);
        u8g2.print("SERVICE MODE");
//#endif      
      }
      //if (flag == 0)
      if ((flags & BARDODE_DATA_FLAG) != BARDODE_DATA_FLAG)
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 30);
        u8g2.print(reading1print);
        
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(80, 30);
        u8g2.print(reading2print); 
      }
      //if (flag == 1)
      if ((flags & BARDODE_DATA_FLAG) == BARDODE_DATA_FLAG)
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 30);
        u8g2.print(barcode_data);
      }

      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(90, 20);
      u8g2.print(current_weight_disp, 2);

      if (reading2 < 1000.00)
      {
        flags  = xEventGroupSetBits(scales_flags, EMPTY_SCALE );
      }

      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.setCursor(30, 45);

      if ((flags & EMPTY_SCALE) == EMPTY_SCALE)
      {
        u8g2.print("PUT&PRESS");
      }

      if (((flags & SERVICE_MODE) != SERVICE_MODE) && (flags & EMPTY_SCALE) != EMPTY_SCALE)
      //if (service_mode == false)      
      {
        if (final_weight == false)
        {
//#ifndef SERVICE_MODE        
          u8g2.print("WAIT...");
        }
        else
        {
          u8g2.print(final_weight, 2);
        }
      }
//#endif
//#ifdef SERVICE_MODE
      if ((flags & SERVICE_MODE) == SERVICE_MODE)
      //if (service_mode == true)
      {
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.setCursor(15, 45);
        u8g2.print("SERVICE MODE");
      }
//#endif
      

      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(5, 55);
      u8g2.print(date_time);

      if (left_up_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(0, 5, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  0,  0, "SERVICE" );
          u8g2.drawButtonUTF8(5, 11, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  0,  0, "MODE" );
          while(1)
          {
            if ((flags & SERVICE_MODE) != SERVICE_MODE)
            //if (service_mode == false)
            {
              flags  = xEventGroupSetBits(scales_flags, SERVICE_MODE );
              //service_mode = true;
              Serial.println("service_mode = true");
              //memset(barcode_data, '\0', sizeof(barcode_data));
              vTaskDelay(200);
              break;
            }
            if ((flags & SERVICE_MODE) == SERVICE_MODE)
            //if (service_mode == true)
            {
              flags = xEventGroupClearBits(scales_flags, SERVICE_MODE );
              //service_mode = false;
              Serial.println("service_mode = false");
              vTaskDelay(200);
              break;
            }
          }
          
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(0, 5);
        u8g2.print("SERVICE");
        u8g2.setCursor(5, 11);
        u8g2.print("MODE");
      }


      if (left_down_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(0, 64, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "CALIBRATE" );
          coefficient = reading1/reading2;
          reading1print = reading1;
          reading2print = reading2;
#ifdef SERIAL_FOR_DEBUG
          Serial.print("Reading 1 value - ");
          Serial.println(reading1);
          Serial.print("Reading 2 value - ");
          Serial.println(reading2);
          Serial.print("Coefficient value - ");
          Serial.println(coefficient, 4);
#endif
          flags = xEventGroupClearBits(scales_flags, BARDODE_DATA_FLAG);
          //flag = 0;
          vTaskDelay(20);
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(0, 64);
        u8g2.print("CALIBRATE");
      }

      if (right_up_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(105, 10, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  0,  0, "SAVE" );
          flags = xEventGroupClearBits(scales_flags, BARDODE_DATA_FLAG);
          //flag = 0;
          //if (task_counter == 0)
          //{
          //write_to_sd();
          if ((flags & SD_CARD_ERROR) != SD_CARD_ERROR)
          //if (!(is_bit_set(0)))
          {
            append_data_to_log();
            vTaskDelay(30 / portTICK_PERIOD_MS);
            memset(barcode_data, '\0', sizeof(barcode_data));
            //task_counter ++;
          }
          else
          {
            u8g2. firstPage ( ) ;
            do
            {
              u8g2.setFont(u8g2_font_t0_16b_tf);
              u8g2.setCursor(15, 15);
              u8g2.print("SD card ERROR");
              vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            while ( u8g2.nextPage() );
          }
            
        
          
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(105, 10);
        u8g2.print("SAVE");
        //task_counter = 0;
      }
      
      
      if (right_down_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(85, 64, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "WEIGHTING" );
          flags  = xEventGroupSetBits(scales_flags, FINAL_WEIGHT );
          flags  = xEventGroupClearBits(scales_flags, EMPTY_SCALE );
          //flag_weighting = 1;
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(85, 64);
        u8g2.print("WEIGHTING");
      }
      
    }
    while ( u8g2.nextPage() );
    vTaskDelay(100 / portTICK_PERIOD_MS); // Without this delay, Watchdog Timer always gets triggered
  }

}

//*-------------------------------------------------------------*//
//*------------------WEIGHTING PROCESS TASKs--------------------*//
//*-------------------------------------------------------------*//

void getweight(void *pvParameters)
{
  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  scale1.set_scale(1);
#ifndef WITHOUT_TARE
  scale1.tare();
#endif
#ifdef WITHOUT_TARE
  scale1.set_offset(51100);
#endif
  scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  scale2.set_scale(1);
#ifndef WITHOUT_TARE
  scale2.tare();
#endif
#ifdef WITHOUT_TARE
  scale2.set_offset(211800);
#endif
  while (1)
  {
    if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE)
    {
      double current_weight;
      reading1 = scale1.get_units(3);
      reading2 = scale2.get_units(3);
      current_weight = (reading2*COMPENSATION_WEIGHT*coefficient)/reading1;
#ifdef SERIAL_FOR_DEBUG      
      Serial.print("Calculated value                 ");
      Serial.println(current_weight);
#endif
      if (xQueueSend(reading_queue, &current_weight, portMAX_DELAY) == pdPASS) 
      {
#ifdef SERIAL_FOR_DEBUG
        Serial.print("Value was sent to queue         ");
        Serial.println(current_weight);
#endif
        flags = xEventGroupSetBits(scales_flags, QUEUE_ERROR);
      }
    }
    xSemaphoreGive(mutex_wait);
    vTaskDelay(25 / portTICK_PERIOD_MS);
    
  }
}

    

void get_final_weight(void *pvParameters)
{
  
//#ifdef SERVICE_MODE
    //vTaskDelete(NULL);
//#endif  
  while (1)
  { 
    if ((flags & SERVICE_MODE) != SERVICE_MODE)
    //if (service_mode == false)
    {
      //if (flag_weighting == 1)
      if ((flags & FINAL_WEIGHT) == FINAL_WEIGHT)
      {
        for (int i = 0; i < CALC_ARRAY_SIZE; i++) {mas[i] = 0;}
        average = 0;
        final_weight = 0;
        //flag_weighting = 0;
        flags = xEventGroupClearBits(scales_flags, FINAL_WEIGHT);
      }
      //while (flag_weighting == 0) // Here is the method, which we use to calculate
      while ((flags & FINAL_WEIGHT) != FINAL_WEIGHT)
      {
        median_calc();
      }
    }
    else
      vTaskDelay(5000);
  }
}

void median_calc()
{
  for (int i = 0; i < CALC_ARRAY_SIZE; i++)
  {
    double temp_weight;
    if (xQueueReceive(reading_queue, &temp_weight, portMAX_DELAY) == pdPASS) 
    {
#ifdef SERIAL_FOR_DEBUG
      Serial.print(i);
      Serial.print("     Value received to queue:         ");
      Serial.println(temp_weight);
#endif
      flags = xEventGroupSetBits(scales_flags, QUEUE_ERROR);
    }
    //mas[i] = current_weight;
    //Serial.print("Received value                  -                  ");
    //Serial.println(current_weight);
    mas[i] = temp_weight;
    //mas[i] = (reading2*COMPENSATION_WEIGHT*coefficient)/reading1;
    //Serial.print("Calculated value                  -                  ");
    //Serial.println(mas[i]);
    //mas[i] = temp_weight;
    //if (flag_weighting == 1)
    if ((flags & FINAL_WEIGHT) == FINAL_WEIGHT)
      break;
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
  qsort(mas, CALC_ARRAY_SIZE, sizeof(double), cmpfunc);
  final_weight = mas[CALC_ARRAY_SIZE/2];
  Serial.println(final_weight);

}

void show_current_weight(void *pvParameters)
{
  while (1)
  {
    if ((flags & SERVICE_MODE) != SERVICE_MODE)
    //if (service_mode == false)
    {
    //if (reading1 < 35000 && reading1 > 40000)
    //{
    //  set_bit(1);
    //}
      current_weight_disp = (reading2*COMPENSATION_WEIGHT*coefficient)/reading1;
      vTaskDelay(500 / portTICK_PERIOD_MS);
      Serial.print("reading1 = ");
      Serial.println(reading1);
      Serial.print("reading2 = ");
      Serial.println(reading2);
    }
    else
      vTaskDelay(5000);
  }
}

//*-------------------------------------------------------------*//
//*--------------------REAL TIME CLOCK TASK---------------------*//
//*-------------------------------------------------------------*//

void get_time(void *pvParameters)
{
  rtc.begin();
  if (! rtc.begin()) 
  {
    //set_bit(2);
    flags  = xEventGroupSetBits(scales_flags, RTC_ERROR );
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  rtc.start();
  while (1)
  {
    DateTime now = rtc.now();
    if ((flags & SERVICE_MODE) != SERVICE_MODE)
    //if (service_mode == false)
    {
//#ifndef SERVICE_MODE
      if ((now.year() < 2030) && (now.hour() < 24))
        sprintf(date_time, "%02d/%02d/%04d        %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
//#endif
    }
//#ifdef SERVICE_MODE
    if ((flags & SERVICE_MODE) == SERVICE_MODE)
    //if (service_mode == true)
    {
      if ((now.year() < 2030) && (now.hour() < 24))
        sprintf(date_time, "%02d/%02d/%04d-%02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
//#endif
    }
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
}


//*-------------------------------------------------------------*//
//*-----------------------BARCODE TASK--------------------------*//
//*-------------------------------------------------------------*//

void barcode_scanner(void *pvParameters)
{
//#ifdef SERVICE_MODE
//  vTaskDelete(NULL);
//#endif
  Serial2.begin(57600, SERIAL_8N1, RXD2, TXD2);
  int data_symbol;
  while (1)
  {
    if ((flags & SERVICE_MODE) != SERVICE_MODE)
    //if (service_mode == false)
    {
      if (Serial2.available()) 
      {
        data_symbol = 0;
        while (Serial2.available() && (data_symbol < BARCODE_DATA_SIZE))
          {
            barcode_data[data_symbol] = (char(Serial2.read()));
            data_symbol++;
          }
        Serial.println(barcode_data);
        if (data_symbol > 5)
        {
          flags  = xEventGroupSetBits(scales_flags, BARDODE_DATA_FLAG );
        }
      }
      else
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    
  }
}

//*-------------------------------------------------------------*//
//*--------------GET DATA FROM GYROSCOPE TASK-------------------*//
//*-------------------------------------------------------------*//

void gyroscope_data(void *pvParameters)
{
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  int data_symbol;
  while (1)
  {
    if ((flags & SERVICE_MODE) == SERVICE_MODE)
    //if (service_mode == true)
    {
      if (gyro_data[0] == 0)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE)
        {
          data_symbol = 0;
          while (Serial2.available() && (data_symbol < GYRO_DATA_SIZE))
          {

            gyro_data[data_symbol] = (char(Serial2.read()));
            if (gyro_data[data_symbol] == 'Q')
            {
              gyro_data[data_symbol] = '\r';
              break;
            }
            data_symbol++;
          }
          Serial.println(gyro_data);
        }
        xSemaphoreGive(mutex_wait);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        Serial2.println('B');
      }
      else
      {
        Serial.println("Waiting for the packet");
        vTaskDelay(300 / portTICK_PERIOD_MS);
      }
    }
    else
      vTaskDelay(5000);
      
  }
}

//*-------------------------------------------------------------*//
//*----------------ESP32 Wi-Fi TELNET Server--------------------*//
//*-------------------------------------------------------------*//

void telnet_server(void *pvParameters)
{
  int8_t i;
  //Serial.begin(115200);
  Serial.print("\nAttaching to WiFi '" + String(ssid) + String("' ..."));

  WiFi.begin(ssid, password);
  
  Serial.print(" attached to WiFi.\nConnecting to network ... ");
  for (i = 60; i != 0; i--) 
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      flags = xEventGroupSetBits(scales_flags, WIFI_FLAG );
      break;
    }
    vTaskDelay(50);
  }
  if (i == 0) 
  {
    Serial.println("Network connection failed!\nRestarting ESP32!");
    vTaskDelay(50);
    //ESP.restart();
  }
  
  Serial.print(" network connected.\nLocal IP address: ");
  Serial.println(WiFi.localIP());
  Server.begin();
  Server.setNoDelay(true);
  Serial.print("Ready! Use port 23 to connect.");
  while (1)
  {
    if ((flags & SERVICE_MODE) == SERVICE_MODE)
    //if (service_mode == true)
    {
      if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE)
      {
        
        if (WiFi.status() != WL_CONNECTED) // Check if WiFi is connected
        {
          flags = xEventGroupClearBits(scales_flags, WIFI_FLAG);
          Serial.println("WiFi not connected! Retrying ...");
          if (Client) Client.stop();
        }
        if (Server.hasClient()) //check if there are any new clients
        {
          Client = Server.available();
          if (!Client) Serial.println("available broken");  // If Client = false, print message
          Serial.print("New client: ");
          Serial.println(Client.remoteIP());
        }
        if (Client.connected())
        {
          flags = xEventGroupSetBits(scales_flags, WIFI_DATA );
          char scales_data[350];
          snprintf(scales_data, sizeof(scales_data), "%s %s %s %.2lf %s %.2lf %s", \
          "Date/Time:", date_time, \
          "Reading1:", reading1, \
          "Reading2:", reading2, " ");
          strcat(scales_data, gyro_data);
          Client.println(scales_data);
          Serial.println(scales_data);
          memset(gyro_data, 0, sizeof(gyro_data));
        }
        
      }
      xSemaphoreGive(mutex_wait);
      vTaskDelay(500);
      flags = xEventGroupClearBits(scales_flags, WIFI_DATA );
    }
    else
      vTaskDelay(5000);
  }

}


//*-------------------------------------------------------------*//
//*----------------------FREERTOS SETUP-------------------------*//
//*-------------------------------------------------------------*//


void setup ( void )  
{ 
  mutex_wait = xSemaphoreCreateMutex();
  reading_queue = xQueueCreate(1, sizeof(double));

  xTaskCreate(task_button, "buttons", 4096, NULL, 3, NULL);
  xTaskCreate(show_display, "show_display", 8192, NULL, 2, NULL);
  //xTaskCreate(getweight1, "getweight1", 2048, NULL, 2, NULL);
  //xTaskCreate(getweight2, "getweight2", 2048, NULL, 2, NULL);
  xTaskCreate(getweight, "getweight", 2048, NULL, 7, NULL);
  xTaskCreate(get_final_weight, "get_final_weight", 2048, NULL, 7, NULL);
  xTaskCreate(get_time, "get_time", 2048, NULL, 2, NULL);
  xTaskCreate(show_current_weight, "current_weight", 1024, NULL, 3, NULL);
  xTaskCreate(barcode_scanner, "barcode_scanner", 2048, NULL, 2, NULL);
//#ifdef SERVICE_MODE
  xTaskCreate(gyroscope_data, "gyroscope data", 4096, NULL, 2, NULL);
  xTaskCreatePinnedToCore(telnet_server, "telnet server", 8192, NULL, 3, NULL, 1);
//#endif
}
 
void loop ( void )  
{
  vTaskDelay(50);
}


//*-------------------------------------------------------------*//
//*----------------------UNUSED, TO DELETE----------------------*//
//*-------------------------------------------------------------*//



void getweight1(void *pvParameters)
{
  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  scale1.set_scale(1);
  scale1.tare();

  while (1)
  {
    reading1 = scale1.get_units(3);
    Serial.print("Sent reading1                  -                  ");
    Serial.println(reading1);
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void getweight2(void *pvParameters)
{
  scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  scale2.set_scale(1);
  scale2.tare();

  while (1)
  {
    reading2 = scale2.get_units(3);
    Serial.print("Sent reading2                  -                  ");
    Serial.println(reading2);
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

