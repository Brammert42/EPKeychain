#include <SPI.h>
#include "epd1in54_V2.h"
#include "imagedata.h"
#include "epdpaint.h"
#include <stdio.h>
#include <EEPROM.h>
Epd epd;
unsigned char image[1024];
Paint paint(image, 0, 0);

unsigned long time_start_ms;
unsigned long time_now_s;
int EEPROMvalue = 0;
#define COLORED     0
#define UNCOLORED   1
SPISettings spisettings(1000000, MSBFIRST, SPI_MODE0);
void setup()
{
  Serial.begin(115200);
  EEPROM.begin(256);
  pinMode(23, INPUT);
  pinMode(7, INPUT);
  digitalWrite(23, LOW);

  SPI.setRX(0);
  SPI.setSCK(2);
  SPI.setTX(3);
  SPI.begin(true);
//delay(100);

  epd.LDirInit();
  epd.HDirInit();

EEPROMvalue = EEPROM.read(0);
if(digitalRead(7)==0 && digitalRead(23)==1)
{EEPROMvalue = EEPROMvalue -1;}
else if (digitalRead(7)==1 && digitalRead(23)==0)
{EEPROMvalue = 0;}
else if (digitalRead(7)==1 && digitalRead(23)==1)
{EEPROMvalue = EEPROMvalue +1;}

if(EEPROMvalue>20) {EEPROMvalue=0;}
if(EEPROMvalue<0) {EEPROMvalue=20;}
EEPROM.write(0, EEPROMvalue);
EEPROM.commit();

Serial.println("EEPROMvalue");Serial.println(EEPROMvalue);
  switch (EEPROMvalue) 
  {
    case 0:
      epd.Display(gImage_LogoBakerton);
      break;
    case 1:
      epd.Display(gImage_doSomething);
      break;
    case 2:
      epd.Display(BTCLogo1);
      break;
    case 3:
      epd.Display(BTCExplosion);
      break;
    case 4:
      epd.Display(gImage_RickRoll);
      break;  
    case 5:
      epd.Display(BTCLogo1);
      break; 
    case 6:
      epd.Display(BTC1);
      break; 
    case 7:
      epd.Display(BTCLogo1);
      break; 
    case 8:
      epd.Display(btc2);
      break; 
    case 9:
      epd.Display(BTCLogo1);
      break; 
    case 10:
      epd.Display(btc4bull);
      break; 
    case 11:
      epd.Display(BTCLogo1);
      break; 
    case 12:
      epd.Display(btc3myassets);
      break;       
     case 13:
      epd.Display(BTCLogo1);
      break;       
    case 14:
      epd.Display(HoneyBadger1);
      break;            
    case 15:
      epd.Display(BTCLogo1);
      break;  
    case 16:
      epd.Display(btcmicrostrat);
      break;  
    case 17:
      epd.Display(BTCLogo1);
      break;  
    case 18:
      epd.Display(SatoshiMaybe);
      break; 
    case 19:
      epd.Display(HoneyBadger1);
      break; 
    case 20:
      epd.Display(HoneyBadger2);
      break;  
    case 21:
      epd.Display(BTCLogo1);
      break;    
    case 22:
      epd.Display(BTCLogo1);
      break; 
    case 23:
      epd.Display(BTCLogo1);
      break; 
    case 24:
      epd.Display(BTCLogo1);
      break; 
    case 25:
      epd.Display(BTCLogo1);
      break; 
    case 26:
      epd.Display(BTCLogo1);
      break;    
    case 27:
      epd.Display(BTCLogo1);
      break; 
    case 28:
      epd.Display(BTCLogo1);
      break; 
    case 29:
      epd.Display(BTCLogo1);
      break; 
    case 30:
      epd.Display(BTCLogo1);
      break;   
  }
  digitalWrite(23, HIGH);

}

void loop()
{

}

/* for lenore
case 0:
      epd.Display(gImage_ERNIE);
      break;
    case 1:
      epd.Display(gImage_sat);
      break;
    case 2:
      epd.Display(LenoreFam1);
      break;
    case 3:
      epd.Display(IMAGE_DATA);
      break;
    case 4:
      epd.Display(gImage_RickRoll);
      break;  
    case 5:
      epd.Display(gImage_aaron);
      break; 
    case 6:
      epd.Display(gImage_BRAMWENDY);
      break; 
    case 7:
      epd.Display(BTC1);
      break; 
    case 8:
      epd.Display(btc2);
      break; 
    case 9:
      epd.Display(gImage_BRAMWENDY);
      break; 
    case 10:
      epd.Display(gImage_BRAMWENDY);
      break; 
    case 11:
      epd.Display(LenoreFam1);
      break; 
    case 12:
      epd.Display(LenoreFam2);
      break;       
     case 13:
      epd.Display(LenoreFam3);
      break;       
    case 14:
      epd.Display(LenoreFam4);
      break;            
    case 15:
      epd.Display(LenoreFam5);
      break;  
    case 16:
      epd.Display(LenoreFam6);
      break;  
    case 17:
      epd.Display(LenoreFam7);
      break;  
    case 18:
      epd.Display(LenoreFam8);
      break; 
    case 19:
      epd.Display(HoneyBadger1);
      break; 
    case 20:
      epd.Display(HoneyBadger2);
      break;  
    case 21:
      epd.Display(BTCLogo1);
      break;      
*/
/*
 switch (EEPROMvalue) 
  {
    case 0:
      epd.Display(SatoshiMaybe);
      break;
    case 1:
      epd.Display(gImage_btcquote1);
      break;
    case 2:
      epd.Display(Nsharra);
      break;
    case 3:
      epd.Display(BTCExplosion);
      break;
    case 4:
      epd.Display(gImage_RickRoll);
      break;  
    case 5:
      epd.Display(Ace);
      break; 
    case 6:
      epd.Display(BTC1);
      break; 
    case 7:
      epd.Display(naia);
      break; 
    case 8:
      epd.Display(btc2);
      break; 
    case 9:
      epd.Display(ernieDamien);
      break; 
    case 10:
      epd.Display(btc4bull);
      break; 
    case 11:
      epd.Display(aceErnie);
      break; 
    case 12:
      epd.Display(btc3myassets);
      break;       
     case 13:
      epd.Display(gImage_BRAMWENDY);
      break;       
    case 14:
      epd.Display(HoneyBadger1);
      break;            
    case 15:
      epd.Display(LenoreErnie);
      break;  
    case 16:
      epd.Display(btcmicrostrat);
      break;  
    case 17:
      epd.Display(gImage_aaron);
      break;  
    case 18:
      epd.Display(SatoshiMaybe);
      break; 
    case 19:
      epd.Display(HoneyBadger1);
      break; 
    case 20:
      epd.Display(HoneyBadger2);
      break;  
    case 21:
      epd.Display(H2OQR);
      break;    
    case 22:
      epd.Display(BTCLogo1);
      break; 
    case 23:
      epd.Display(BTCLogo1);
      break; 
    case 24:
      epd.Display(BTCLogo1);
      break; 
    case 25:
      epd.Display(BTCLogo1);
      break; 
    case 26:
      epd.Display(BTCLogo1);
      break;    
    case 27:
      epd.Display(BTCLogo1);
      break; 
    case 28:
      epd.Display(BTCLogo1);
      break; 
    case 29:
      epd.Display(BTCLogo1);
      break; 
    case 30:
      epd.Display(BTCLogo1);
      break;   
  }
*/