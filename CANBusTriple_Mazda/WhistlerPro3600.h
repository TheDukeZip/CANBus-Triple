// Quick hack implementation of 9 bit serial packets - adapted from standard SoftwareSerial lib
// Maybe in the future update SoftwareSerial to properly support different parity options
#include "SoftwareSerial9.h"

//Unknown if software serial will be fast enough while processing all 3 CAN busses at same time
//Other option is to utilize hardware serial port on digital pins 0 and 1 which are wired to 
// CAN interrupts but not currently in use.  HW Serial can safely use 8N2 setting - 9th bit always 1.
// NEEDS TESTING WHILE RUNNING IN CAR!
#define RADAR_RX	10		//RX is yellow to radar detector interface box
#define RADAR_TX	6		//TX is green to radar detector interface box
#define RADAR_BAUD	9600

#define RADAR_STATUS_A  0x02
#define RADAR_STATUS_B  0xf1
#define RADAR_STATUS_E  0x03

SoftwareSerial9 radarSerial(RADAR_RX, RADAR_TX); // RX, TX
char RadarMsgBuf[_SS_MAX_RX_BUFF];
uint8_t RadarMsgBufTail = 0;

bool RadarButtonDown = false;
bool RadarButtonUp = false;
int  RadarButtonTimer = 0;

byte RadarStatusC = 0xff;
byte RadarStatusD = 0x10;

char RadarOutput[9];

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
    static void RadarProcessInput();
};

void WhistlerPro3600::init()
{
  radarSerial.begin(RADAR_BAUD);
}

void WhistlerPro3600::tick()
{
  bool bTemp = 0;

  if( (millis() % 32) < 1 ) RadarSendStatus();

  if(!RadarButtonDown)
  {
    while(radarSerial.available())
    {
      RadarMsgBuf[RadarMsgBufTail] = radarSerial.read9(bTemp);
      RadarMsgBufTail++;
    }

    if(RadarMsgBufTail > 13)
    {
      RadarWriteOutput();
    }
  }
}

void WhistlerPro3600::RadarSendStatus()
{
  //You don't need to send the status every 32ms like the unit does
  //only need to send on button down or button up events
  //the interface box sometimes misses a msg, send at least twice to be sure
  if(RadarButtonUp)
  {
    radarSerial.write9(RADAR_STATUS_A, 1);
    radarSerial.write9(RADAR_STATUS_B, 1);
    radarSerial.write9(RadarStatusC, 1);
    radarSerial.write9(RadarStatusD, 1);
    radarSerial.write9(RADAR_STATUS_E, 1);
    RadarButtonTimer++;
    if(RadarButtonTimer > 1)
    {
      RadarButtonUp = false;
      RadarButtonTimer = 0;
    }
  }
  if(RadarButtonDown)
  {
    radarSerial.write9(RADAR_STATUS_A, 1);
    radarSerial.write9(RADAR_STATUS_B, 1);
    radarSerial.write9(RadarStatusC, 1);
    radarSerial.write9(RadarStatusD, 1);
    radarSerial.write9(RADAR_STATUS_E, 1);
    RadarButtonTimer++;
    RadarButtonDown = false;
    RadarButtonUp = true;
    RadarStatusC = 0xff;
    RadarStatusD = 0x10;
    
    if(RadarButtonTimer > 1)
    {
      RadarButtonDown = false;
      RadarButtonUp = true;
      RadarButtonTimer = 0;
      RadarStatusC = 0xff;
      RadarStatusD = 0x10;
    }
  }
}

void WhistlerPro3600::RadarWriteOutput()
{
  char LocalOutput[9] = "        ";

  if(RadarMsgBuf[0] == 0x02)
  {
    for(int i = 2; i < 10; i++)
      LocalOutput[i - 2] = RadarMsgBuf[i];
    if(strcmp(RadarOutput, LocalOutput))
    {
      sprintf( RadarOutput, LocalOutput);
      Serial.println(RadarOutput);          //TODO: To MazdaLED instead of USB Serial
    }
  }
  RadarMsgBufTail = 0; 
}


//TODO: These were previously triggered off of USB serial input
//  Need to trigger these off steering buttons
//  Need possibility for multiple simultaneous presses
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

void WhistlerPro3600::RadarProcessInput()
{
  char c = Serial.read();
    
  if(c == 'p') 
    RadarSendPower();
  else if(c == 'd')
    RadarSendDark();
  else if(c == 'q')
    RadarSendQuiet();
  else if(c == 'c')
    RadarSendCity();
  else if(c == ' ')
    sprintf( RadarOutput, "        "); //Use space to see current display
}
