#include "RTOS_tasks.h"

const int CS = 5;
// DEFINE PINs FOR TWO HX711
const int LOADCELL1_DOUT_PIN = 32;
const int LOADCELL1_SCK_PIN = 33;
const int LOADCELL2_DOUT_PIN = 25;
const int LOADCELL2_SCK_PIN = 26;
// HX711 INITIALIZATION
HX711 scale1;
HX711 scale2;
// DEFINE VARIABLES FOR HX711
double current_weight;
char date_time [50] = "";
char logfilename [25] = "";
char barcode_data[BARCODE_DATA_SIZE] = "";
double reading1;
double reading2;
double reading1print;
double reading2print;
double final_weight;
double mas[CALC_ARRAY_SIZE];
double average = 0;
int flag_weighting;
int flag;
int task_counter = 0;
int error_flag = 0; //Error flags: 1 - SD card error; 2 - Barcode scanner error; 3 - RTC error;
double coefficient;
// DEFINE BUTTON FLAGS
int left_up_button; // FREE BUTTON
int left_down_button; // CALIBRATE BUTTON
int right_up_button; // FREE BUTTON
int right_down_button; // WEIGHTING BUTTON
// RTC INITIALIZATION
RTC_PCF8563 rtc;

// DEFINE PINOUTS FOR DISPLAY: scl = 14 // si = 13 // cs = 15 // rs = 12 // rse = 27
U8G2_ST7565_ERC12864_1_4W_SW_SPI u8g2 ( U8G2_R0, /* scl=*/  14 , /* si=*/  13 , /* cs=*/  15 , /* rs=*/  12 , /* rse=*/  27 ) ;
// DEFINE COMMON FUNCTIONS
void start_page();
void second_page();
void average_calc();
void median_calc();
void append_data_to_log();
void write_log_header(fs::FS &fs, const char * path);

File LogFile;
File myFile;

 // Assign semaphore
SemaphoreHandle_t btnSemaphore;
volatile SemaphoreHandle_t mutex_wait;

// create function - interrupt handler
void IRAM_ATTR ISR_btn() // IRAM_ATTR means, that we use RAM (wich more faster and recommended for interrupts). ISR - interrupt service routine
{
  xSemaphoreGiveFromISR( btnSemaphore, NULL ); // Macro to release a semaphore from interruption. The semaphore must have previously been created with a call to xSemaphoreCreateBinary() or xSemaphoreCreateCounting().
}

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
      detachInterrupt(BUTTON_DOWN);
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
        if (st1 == LOW) {right_down_button = 1;}
        else {right_down_button = 0;}
      }

      if  (st2 != state_btn2)
      {
        state_btn2 = st2;
        if (st2 == LOW) {right_up_button = 1;}
        else{right_up_button = 0;}
      }

      if  (st3 != state_btn3)
      {
        state_btn3 = st3;
        if (st3 == LOW) {left_up_button = 1;}
        else {left_up_button = 0;}
      }

      if  (st4 != state_btn4)
      {
        state_btn4 = st4;
        if (st4 == LOW) {left_down_button = 1;}
        else {left_down_button = 0;}
      }
      if (st1 == HIGH && st2 == HIGH && st3 == HIGH && st4 == HIGH)
      {
        attachInterrupt(BUTTON_DOWN, ISR_btn, FALLING);
        attachInterrupt(BUTTON_UP, ISR_btn, FALLING);
        attachInterrupt(BUTTON_RIGHT, ISR_btn, FALLING);
        attachInterrupt(BUTTON_LEFT, ISR_btn, FALLING);
        isISR = true;
      }
      vTaskDelay(100); // this function calls the task manager, which set this task to WAIT mode for 100 system ticks.
    }

  }
}

void show_display(void *pvParameters) // create display menu task
{
  Serial.begin(115200);
  u8g2. begin ( ) ;
  u8g2. setContrast  (10) ;
  u8g2. enableUTF8Print ( ) ;
  if(!SD.begin())
  {
    Serial.println("Card Mount Failed");
    error_flag = 1;
  }
  write_log_header(SD, "/log.csv");
  int i = 0;
  while (true)
  {
    if (i < 1)
    {
      start_page();
      vTaskDelay(5000);
      i++;
    }
    if (i < 2)
    {
      //second_page();
      vTaskDelay(5000);
      coefficient = reading1/reading2; 
      Serial.print("Reading 1 value - ");
      Serial.println(reading1);
      Serial.print("Reading 2 value - ");
      Serial.println(reading2);
      Serial.print("Coefficient value - ");
      Serial.println(coefficient, 4);
      reading1print = reading1;
      reading2print = reading2;
      i++;
    }
    u8g2. firstPage ( ) ;
    do  
    {
      if (error_flag == 0)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10);
        u8g2.drawGlyph(50, 10, 0xE1D6);
      }
      
      if (error_flag == 1)
      {
        u8g2.setFont(u8g2_font_siji_t_6x10);
        u8g2.drawGlyph(50, 10, 0xE0B3);
      }
      
      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(5, 20);
      u8g2.print("coeff = ");

      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(40, 20);
      u8g2.print(coefficient, 4);

      if (flag == 0)
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 30);
        u8g2.print(reading1print);
        
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(80, 30);
        u8g2.print(reading2print); 
      }
      if (flag == 1)
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(5, 30);
        u8g2.print(barcode_data);
      }

      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(90, 20);
      u8g2.print(current_weight, 2);

      u8g2.setFont(u8g2_font_7x13B_tf);
      u8g2.setCursor(30, 45);
      
      if (final_weight == false)
      {
        u8g2.print("WAIT...");
      }
      else
      {
        u8g2.print(final_weight, 2);
      }

      u8g2.setFont(u8g2_font_fivepx_tr);
      u8g2.setCursor(5, 55);
      u8g2.print(date_time);

      if (left_up_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(0, 7, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "FREE BUTTON" );
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(0, 7);
        u8g2.print("FREE BUTTON");
      }


      if (left_down_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(0, 64, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "CALIBRATE" );
          coefficient = reading1/reading2;
          reading1print = reading1;
          reading2print = reading2;
          Serial.print("Reading 1 value - ");
          Serial.println(reading1);
          Serial.print("Reading 2 value - ");
          Serial.println(reading2);
          Serial.print("Coefficient value - ");
          Serial.println(coefficient, 4);
          flag = 0;
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
          u8g2.drawButtonUTF8(105, 7, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "SAVE" );
          flag = 0;
          if (task_counter == 0)
          {
            //write_to_sd();
            append_data_to_log();
            vTaskDelay(30);
            memset(barcode_data, '\0', sizeof(barcode_data));
            task_counter ++;
          }
          
        }
        xSemaphoreGive(mutex_wait);
      }
      else
      {
        u8g2.setFont(u8g2_font_fivepx_tr);
        u8g2.setCursor(105, 7);
        u8g2.print("SAVE");
        task_counter = 0;
      }
      
      
      if (right_down_button == 1)
      {
        if (xSemaphoreTake(mutex_wait, portMAX_DELAY) == pdTRUE) 
        {
          u8g2.drawButtonUTF8(85, 64, U8G2_BTN_INV|U8G2_BTN_BW2, 0,  2,  2, "WEIGHTING" );
          flag_weighting = 1;
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
    vTaskDelay(15); // Without this delay, Watchdog Timer always gets triggered
  }

}

void getweight1(void *pvParameters)
{
  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  scale1.set_scale(1);
  scale1.tare();

  while (1)
  {
    reading1 = scale1.get_units(3);
    vTaskDelay(25);
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
    vTaskDelay(25);
  }
}

void start_page()
{
  u8g2. firstPage ( ) ;
  do
  {
    u8g2.setFont(u8g2_font_t0_16b_tf);
    u8g2.setCursor(10, 15);
    u8g2.print("Digital Marine");
    u8g2.setFont(u8g2_font_t0_16b_tf);
    u8g2.setCursor(15, 30);
    u8g2.print("Balances 100g");
    u8g2.setFont(u8g2_font_fivepx_tr);
    u8g2.setCursor(5, 45);
    u8g2.print("Version 1.1 beta. 10.01.2023");
    u8g2.setFont(u8g2_font_fivepx_tr);
    u8g2.setCursor(50, 53);
    u8g2.print("");
    u8g2.setFont(u8g2_font_fivepx_tr);
    u8g2.setCursor(25, 61);
    u8g2.print("t.me/coreblogger");
  }
  while ( u8g2.nextPage() );
}

void second_page()
{
  while (reading1 < 35000 && reading2 < 35000)
  {
    u8g2. firstPage ( ) ;
    do
    {
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 15);
      u8g2.print("Please, place");
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(10, 30);
      u8g2.print("20g weight on");
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 45);
      u8g2.print("the both cells");
    }
    while ( u8g2.nextPage() );
  }
  
}


void get_final_weight(void *pvParameters)
{
  
  while (1)
  {    
    if (flag_weighting == 1)
    {
      for (int i = 0; i < CALC_ARRAY_SIZE; i++) {mas[i] = 0;}
      average = 0;
      final_weight = 0;
      flag_weighting = 0;
    }
    while (flag_weighting == 0) // Here is the method, which we use to calculate
    {
      //average_calc();
      median_calc();
    }
  }
}

void get_time(void *pvParameters)
{
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  rtc.start();
  while (1)
  {
    DateTime now = rtc.now();
    sprintf(date_time, "%02d/%02d/%04d        %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
    vTaskDelay(300);
  }
}
/*
void bubble_sort()
{
  for (int i = 0; i < CALC_ARRAY_SIZE; i++)
  {
    mas[i] = (reading2*20*coefficient)/reading1;
    if (flag_weighting == 1)
      break;
    vTaskDelay(25);
  }
  for (int i = 0; i < (CALC_ARRAY_SIZE - 1); i++)
  {
    for (int j = (CALC_ARRAY_SIZE - 1); j > i; j--)
    {
      if (mas[j - 1] > mas[j])
      {
        double temp = mas[j - 1]; // меняем их местами
        mas[j - 1] = mas[j];
        mas[j] = temp;
      }
    }
  }
  final_weight = mas[CALC_ARRAY_SIZE/2];
  Serial.println(final_weight);
  
}
*/

void median_calc()
{
  for (int i = 0; i < CALC_ARRAY_SIZE; i++)
  {
    mas[i] = (reading2*20*coefficient)/reading1;
    if (flag_weighting == 1)
      break;
    vTaskDelay(25);
  }
  qsort(mas, CALC_ARRAY_SIZE, sizeof(double), cmpfunc);
  final_weight = mas[CALC_ARRAY_SIZE/2];
  Serial.println(final_weight);

}


void average_calc()
{
  for (int i = 0; i < CALC_ARRAY_SIZE; i++)
  {
    mas[i] = reading2*(20/reading1);
    if (flag_weighting == 1)
      break;
    vTaskDelay(25);
  }
  for (int i = 0; i < CALC_ARRAY_SIZE; i++)
  {
    average = average + (mas[i]/CALC_ARRAY_SIZE);
    if (flag_weighting == 1)
      break;
  }
  final_weight = average;
  average = 0;
  Serial.println(final_weight);  
}

void show_current_weight(void *pvParameters)
{
  while (1)
  {
    current_weight = (reading2*20*coefficient)/reading1;
    vTaskDelay(500);
  }
}


void barcode_scanner(void *pvParameters)
{
  Serial2.begin(57600, SERIAL_8N1, RXD2, TXD2);
  int data_symbol;
  while (1)
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
      flag = 1;
    }
    else
      vTaskDelay(1000);
    
  }
}

void readFile(fs::FS &fs, const char * path)
{
  Serial.printf("Reading file: %s\n", path);
  Serial.println();
  myFile = fs.open(path);
  if(!myFile)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  Serial.println();
  while(myFile.available())
  {
    Serial.write(myFile.read());
  }
  myFile.close();
}

void write_log_header(fs::FS &fs, const char * path)
{
  myFile = fs.open(path, FILE_APPEND);
  if(!myFile)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  myFile.print("date/time");
  myFile.print(",");
  myFile.print("Weight");
  myFile.print(",");
  myFile.print("Barcode_data");
  myFile.print(",");
  myFile.print("current_weight");
  myFile.print(",");
  myFile.print("coefficient");
  myFile.print(",");
  myFile.print("reading1");
  myFile.print(",");
  myFile.print("reading2");
  myFile.println();
  myFile.close();
}

void appendFile(fs::FS &fs, const char * path)
{
  Serial.printf("Appending to file: %s\n", path);
  Serial.println();
  myFile = fs.open(path, FILE_APPEND);
  if(!myFile)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  myFile.print(date_time);
  myFile.print(",");
  myFile.print(final_weight);
  myFile.print(",");
  myFile.print(barcode_data);
  myFile.print(",");
  myFile.print(current_weight);
  myFile.print(",");
  myFile.print(coefficient);
  myFile.print(",");
  myFile.print(reading1);
  myFile.print(",");
  myFile.print(reading2);
  myFile.println();
  myFile.close();
}

void append_data_to_log()
{
  
  if(!SD.begin())
  {
    Serial.println("Card Mount Failed");
    u8g2. firstPage ( ) ;
    do
    {
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 15);
      u8g2.print("SD card ERROR");
      vTaskDelay(100);
    }
    while ( u8g2.nextPage() );
    return;
  }
  else
  {
    u8g2. firstPage ( ) ;
    do
    {
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 15);
      u8g2.print("DATA SAVED :)");
      vTaskDelay(100);
    }
    while ( u8g2.nextPage() );
  }
  /*
  uint8_t cardType = SD.cardType();
  
  if(cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC)
  {
    Serial.println("MMC");
  } 
  else if(cardType == CARD_SD)
  {
    Serial.println("SDSC");
  } 
  else if(cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  } 
  else 
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.println();
  */
  //writeFile(SD, "/log.csv", "Hello ");
  appendFile(SD, "/log.csv");
  readFile(SD, "/log.csv");
}


//---------------------------------------------------------------------------//


void setup ( void )  
{ 
  mutex_wait = xSemaphoreCreateMutex();
  //esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  //esp_task_wdt_add(NULL); //add current thread to WDT watch
  //esp_task_wdt_reset();
  xTaskCreate(task_button, "buttons", 4096, NULL, 2, NULL);
  xTaskCreate(show_display, "show_display", 8192, NULL, 2, NULL);
  xTaskCreate(getweight1, "getweight1", 2048, NULL, 2, NULL);
  xTaskCreate(getweight2, "getweight2", 2048, NULL, 2, NULL);
  xTaskCreate(get_final_weight, "get_final_weight", 2048, NULL, 2, NULL);
  xTaskCreate(get_time, "get_time", 2048, NULL, 2, NULL);
  xTaskCreate(show_current_weight, "current_weight", 1024, NULL, 2, NULL);
  xTaskCreate(barcode_scanner, "barcode_scanner", 2048, NULL, 2, NULL);
}
 
void loop ( void )  
{

}
