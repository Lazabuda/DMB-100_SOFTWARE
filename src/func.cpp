#include "func.h"

//SD class
File myFile;

//-----------------------------------------SD CARD FUNCTIONS----------------------------------------------//

//-------------------------READ FROM SD CARD--------------------

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

//-------------------------WRITE HEADER TO LOG FILE--------------------

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

//-------------------------APPEND DATA TO LOG FILE--------------------

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

//-------------------------COMMAND TO APPEND DATA AFTER PRESSING BUTTON--------------------

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
      vTaskDelay(100 / portTICK_PERIOD_MS);
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
      u8g2.print("DATA HAS");
      u8g2.setCursor(15, 40);
      u8g2.print("BEEN SAVED :)");
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    while ( u8g2.nextPage() );
  }

  
  //writeFile(SD, "/log.csv", "Hello ");
  appendFile(SD, "/log.csv");
  readFile(SD, "/log.csv");
}

//-------------------------SHOW SD TYPE AND SD SIZE--------------------

void test_SD_type_size()
{  
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
}



//--------------------------------------------------------------------------------------------------------------------//

//---------------------------The first page on the display, information--------------------------------------- 

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


//---------------------------The second page on the display, calibration condition-------------------------------------- 


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
      u8g2.print(COMPENSATION_WEIGHT);
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 30);
      u8g2.print("    weight on");
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 45);
      u8g2.print("the both cells");
      vTaskDelay(200);
    }
    while ( u8g2.nextPage() );
  }
  
}



int cmpfunc (const void * a, const void * b) {
   return ( *(double*)a - *(double*)b );
}

void is_error()
{ 
  u8g2. firstPage ( ) ;
  do
  {
    if (is_bit_set(1))
    {
      u8g2.setFont(u8g2_font_t0_16b_tf);
      u8g2.setCursor(15, 15);
      u8g2.print("LOAD CELL1");
      u8g2.setCursor(15, 40);
      u8g2.print("ERROR!");
      
    }
  }
  while ( u8g2.nextPage() );
  
  
  /*
  if (is_bit_set(2))
  {
    u8g2.setFont(u8g2_font_siji_t_6x10);
    u8g2.setCursor(50, 20);
    u8g2.print("RTC ERR");
  }
  */
}

int set_bit(int num_bit)
{
  error_flag |= (1 << num_bit); // set
  //error_flag &= ~(1<<num_bit); // clear
  return error_flag;
}

int is_bit_set(int num_bit)
{
  if (((error_flag << num_bit) & 0x01) == 1)
  {
    return true;
  }
  if (((error_flag << num_bit) & 0x01) == 0)
  {
    return false;
  }
  return true;
}