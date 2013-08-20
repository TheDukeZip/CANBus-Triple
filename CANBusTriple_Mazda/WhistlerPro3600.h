//Integration with Whistler Pro-3600
//
//See http://phzero.net/?q=whistler for technical details on connecting Whistler to CBT

// Quick hack implementation of 9 bit serial packets - adapted from standard SoftwareSerial lib
// Maybe in the future update SoftwareSerial to properly support different parity options
#include "SoftwareSerial9.h"

//Software serial seems to be fast enough while processing all 3 CAN busses at same time
//Other option is to utilize hardware serial port on digital pins 0 and 1 which are wired to 
// CAN interrupts but not currently in use.  HW Serial can safely use 8N2 setting - 9th bit always 1.
#define RADAR_RX	10		//RX is yellow to radar detector interface box
#define RADAR_TX	6		//TX is green to radar detector interface box
#define RADAR_BAUD	9600

#define RADAR_STATUS_A  0x02
#define RADAR_STATUS_B  0xf1
#define RADAR_STATUS_E  0x03

SoftwareSerial9 radarSerial(RADAR_RX, RADAR_TX); // RX, TX
char RadarMsgBuf[_SS_MAX_RX_BUFF];
uint8_t RadarMsgBufTail = 0;

byte RadarWheelButton = 0;
boolean RadarButtonDown = false;
uint8_t RadarButtonTimer = 0;

byte RadarStatusC = 0xff;
byte RadarStatusD = 0x10;

char RadarOutput[13];

enum RADAR_MODE {
  RADAR_MODE_ALERTS_ONLY,
  RADAR_MODE_ALWAYS_ON
};

RADAR_MODE RadarMode = RADAR_MODE_ALERTS_ONLY;
boolean RadarDebug = false;


class WhistlerPro3600 {
  public:
    static void init();
    static void tick();
  private:
    static void RadarSendStatus();
    static void RadarWriteOutput();
    static void RadarSendPower();
    static void RadarSendQuiet();
    static void RadarSendDark();
    static void RadarSendCity();
    static void RadarSendNone();
    static void RadarGetWheelButton();
};

void WhistlerPro3600::init()
{
  radarSerial.begin(RADAR_BAUD);
}

void WhistlerPro3600::tick()
{
  bool bTemp = 0;

  RadarGetWheelButton();
  
  if( (millis() % 32) < 1 ) RadarSendStatus();

  if(!RadarButtonDown)
  {
    while(radarSerial.available())
    {
      RadarMsgBuf[RadarMsgBufTail] = radarSerial.read9(bTemp);
      RadarMsgBufTail++;
    }

    if(RadarMsgBufTail > 13)  //Have a full message from Whistler
    {
      RadarWriteOutput();
    }
    //Update the screen once per second in this mode so MazdaLED doesn't take over
    else if(RadarMode == RADAR_MODE_ALWAYS_ON && (millis() % 1000) < 1) RadarWriteOutput();
  }
}

void WhistlerPro3600::RadarSendStatus()
{
  //You don't need to send the status every 32ms like the unit does
  //only need to send on button down or button up events
  //the interface box sometimes misses a msg, send at least twice to be sure
  if(RadarButtonDown)
  {
    radarSerial.write9(RADAR_STATUS_A, 1);
    radarSerial.write9(RADAR_STATUS_B, 1);
    radarSerial.write9(RadarStatusC, 1);
    radarSerial.write9(RadarStatusD, 1);
    radarSerial.write9(RADAR_STATUS_E, 1);
    RadarButtonTimer++;
    
    if(RadarButtonTimer > 1)
    {
      RadarButtonDown = false;
      RadarButtonTimer = 0;
    }
  }
}

//TODO: Possibly do something in future so Radar alerts override 'stock' and 'status' in MazdaLED
void WhistlerPro3600::RadarWriteOutput()  
{
  char LocalOutput[13] = "            ";

  if(RadarMsgBuf[0] == 0x02)
  {
    for(int i = 2; i < 10; i++)
      LocalOutput[i] = RadarMsgBuf[i];
    if(strcmp(RadarOutput, LocalOutput) || RadarMode == RADAR_MODE_ALWAYS_ON)
    {
      sprintf( RadarOutput, LocalOutput);
      if(RadarDebug) Serial.println(RadarOutput);
      
      //TODO: Filter based on arrows in that char if GPS addon is present
      if((RadarMode == RADAR_MODE_ALERTS_ONLY && RadarOutput[2] != '-') || RadarMode == RADAR_MODE_ALWAYS_ON)                          
      {
        //Convert Whistler chars to MazdaSpeed3 chars
        for(int i = 2; i < 10; i++)                      
        {
          if(RadarOutput[i] == '}') RadarOutput[i] = 0xdf;       //Supposed to be degrees
          else if(RadarOutput[i] == '*') RadarOutput[i] = 0xef;  //Asterisk
          else if(RadarOutput[i] == '/') RadarOutput[i] = 0xf0;  //Supposed to be full block
        }
        MazdaLED::showStatusMessage(RadarOutput, 4000);  
      }
    }
  }
  RadarMsgBufTail = 0;
}

//TODO: Need to support simultaneous pushes for some settings features
void WhistlerPro3600::RadarSendPower()
{
  RadarStatusC = 0xfe;
  RadarStatusD = 0x11;
  RadarButtonDown = true;
}

void WhistlerPro3600::RadarSendDark()
{
  RadarStatusC = 0xfd;
  RadarStatusD = 0x12;
  RadarButtonDown = true;
}

void WhistlerPro3600::RadarSendQuiet()
{
  RadarStatusC = 0xf7;
  RadarStatusD = 0x18;
  RadarButtonDown = true;
}

void WhistlerPro3600::RadarSendCity()
{
  RadarStatusC = 0xfb;
  RadarStatusD = 0x14;
  RadarButtonDown = true;
}

void WhistlerPro3600::RadarSendNone()
{
  RadarStatusC = 0xff;
  RadarStatusD = 0x10;
  RadarButtonDown = true;
}

void WhistlerPro3600::RadarGetWheelButton()
{
  byte button = WheelButton::getButtonDown();
  
  if(RadarWheelButton != button)
  {
    RadarWheelButton = button;
    
    switch(RadarWheelButton)
    {
      case B10000000:    //Left - Power
        if(RadarMode == RADAR_MODE_ALWAYS_ON) RadarSendPower();
        break;
      case B01000000:    //Right - Dark
        if(RadarMode == RADAR_MODE_ALWAYS_ON) RadarSendDark();
        break;
      case B00100000:    //Up - City
        if(RadarMode == RADAR_MODE_ALWAYS_ON) RadarSendCity();
        break;
      case B00010000:    //Down - Quiet
        if(RadarMode == RADAR_MODE_ALWAYS_ON) RadarSendQuiet();
        break;
      case B00000010:    //Back - if in alert mode - Quiet
        if(RadarMode == RADAR_MODE_ALERTS_ONLY) RadarSendQuiet;
        break;
      case B00010010:    //Back and Down - switch mode
        if(RadarMode == RADAR_MODE_ALERTS_ONLY) RadarMode = RADAR_MODE_ALWAYS_ON;
        else RadarMode = RADAR_MODE_ALERTS_ONLY;
        break;
      case B00000000:    //None
        RadarSendNone();
        break;
    }
  }
}
