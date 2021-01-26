// *
// Example program to illustrate how to display time on an RGB display based on NMEA data received via GPS.
// Currently coded for PDT - you'll need to adjust for your timezone.
// 
// The source contained here goes with companion article for ARRL's QEX "A DIY NMEA Based GPS Time Display".
// Author - John C. Westmoreland
// Date -   April 27, 2020
// *

#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <ArduinoGraphics.h>
#include <Arduino_MKRRGB.h>
#include <ArduinoIoTCloud.h>
#include "SECRET.H"

// #include <WiFi.h>

//#define TEST_MODE 1       // If using the Hercules tool for debug
#define NO_TEST_MODE 1      // Running in normal mode using a GPS receiver (i.e. Themis' GPS via WiFi)

#define UTC_OFFSET  7       // this is PDT, so -8 from UTC 8 in Fall, 7 in Spring - currently 'manual'

// uncomment following if using the RGB Matrix Display From Arduino
// #define USING_ARDUINO_MATRIX 1

// uncomment following if using the Lumex LDM-6432 RGB Display
#define USING_LUMEX_DISPLAY 1
#define USING_LATEST_FIRMWARE 1
// Note:  only one RGB display can be enabled at a time.

#define SERIAL_COUNTER_TIME_OUT 5000    // so serial port doesn't hang the board
unsigned char utc_hr_buff[10];   // todo:  small buffer for UTC hour conversion
#if NO_TEST_MODE
const char* ssid     = SECRET_WIFI_NAME;
const char* password = SECRET_PASSWORD;

const char* host = "192.168.4.1";		// this is address of server
IPAddress timeServer(192, 168, 4, 1);		// this is address of server
#endif
// to test
#if TEST_MODE
const char* ssid     = SECRET_WIFI_NAME_TEST_MODE;
const char* password = SECRET_PASSWORD_TEST_MODE;
const char* host = "192.168.2.249";		// this is address of PC w/UDP server
IPAddress timeServer(192, 168, 2, 249);		// this is address of PC w/UDP server
#endif
unsigned int localPort = 123;      // local port to listen for UDP packets
const int httpPort = 123;
 
#if 0
String cloudSerialBuffer = ""; // the string used to compose network messages from the received characters
#endif

bool newData = false;

// char ldmCmd[] = "AT81=(0,0,TEST9876)";
// char ldmCmd1[] = "AT80=(0,0,A)";
char ldmCmdFMR[] = "AT20=()";
// char ldmCmdHdr[] = "AT81=(0,3,Time:)";
#ifndef USING_LATEST_FIRMWARE
char ldmCmdHdr[] = "ATe1=(0,3,96,Time:)";
#else
char ldmCmdHdr[] = "AT81=(0,3,Time:)";
#endif
char ldmBrightness[] = "ATf2=(10)";            // brightness
char ldmCmdMsg1[] = "ATe1=(2,2,3,Have A)";
// char ldmCmdMsg5[] = "ATe1=(2,0,3,GPS Actual)";
// char ldmCmdMsg5[] = "ATe1=(2,0,3,(GPS)Maker)";
#ifndef USING_LATEST_FIRMWARE
char ldmCmdMsg5[] = "ATe1=(2,0,3, WWVB Alt.)";
#else
// char ldmCmdMsg5[] = "AT81=(2,0, WWVB Alt.)";
char ldmCmdMsg5[] = "AT81=(2,0,GPS Atomic)";
#endif
char ldmCmdMsg3[] = "ATe1=(2,2,3,Happy )";
char ldmCmdMsg2[] = "ATe1=(3,1,3,Nice Day!)";
// char ldmCmdMsg6[] = "ATe1=(3,0,3,Real Time.)";
// char ldmCmdMsg6[] = "ATe1=(3,0,3,Faire 2019)";
#ifndef USING_LATEST_FIRMWARE
// char ldmCmdMsg6[] = "ATe1=(3,0,3, Time Sln.)";
char ldmCmdMsg6[] = "ATe1=(3,0,3,   Time)";
#else
// char ldmCmdMsg6[] = "AT81=(3,0, Time Sln.)";
char ldmCmdMsg6[] = "AT81=(3,0,   Time)";
#endif

char ldmCmdMsg4[] = "ATe1=(3,1,3,New Year!)";
// char ldmCmd[] = "AT81=(1,0,  HH:MM:SS)";                    // right now - just this format - char line x char column - 7 tall 5 wide
char ldmCmd1[] = "AT80=(0,0,A)";
#ifndef USING_LATEST_FIRMWARE
char ldmCmd[] = "ATe1=(1,0,111, HH:MM:SS)";
#else
char ldmCmd[] = "AT81=(1,0, HH:MM:SS)";
#endif

#ifdef USING_ARDUINO_MATRIX
char matrixString[] = "abc";					// can only display text clearly (now) with 4x6 font - only 3 chars at a time
#endif

static void Write_Matrix_String(char *string);
static void Write_AT_Command(char *string);
int keyIndex = 0;
int value = 0;
// WiFiClient client;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// program setup
void setup()
{
  unsigned char i;
  unsigned int counter_main = 0;
  setDebugMessageLevel(3); // used to set a level of granularity in information output [0...4]
  Serial.begin(115200);

  delay(10);

#ifdef USING_ARDUINO_MATRIX
  MATRIX.begin();
  // set the brightness, supported values are 0 - 255
  MATRIX.brightness(10);
#endif

  Serial.println();
  Serial.println();

  // in case serial port isn't connected so we don't hang the program:
  do {
    counter_main++;

  } while ( !Serial && ( counter_main < SERIAL_COUNTER_TIME_OUT) );
  
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println();
  Serial.println();

  // lumex display is connected to Serial1

  //  Serial1.flush();
  Serial.println();
  Serial.println();

  Serial1.begin(115200);
  
#if 1
  while (!Serial1) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#endif

#ifdef USING_LATEST_FIRMWARE
  Write_AT_Command("ATef=(96)");
#endif

  Write_AT_Command("ATd0=()");
  delay(100);
  Write_AT_Command("at20=()");
  delay(1000);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  delay(1000);
  WiFi.begin(ssid, password);
  delay(10000);
#if 1
  while (WiFi.status() != WL_CONNECTED) {
    delay(10000);
    Serial.print(".");                        // send to console if taking too long to connect to WiFi...
    WiFi.begin(ssid, password);
  }
#endif
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);
  Serial.println("connected to server");

  Write_AT_Command(ldmBrightness);
  delay(2);
  Write_AT_Command("AT83=(0,0,EFGH9876)");          // just an optional test string output
  delay(1000);

  keyIndex = 0x30;
  // clear Lumex display
  Write_AT_Command("ATd0=()");
  delay(100);
  Write_AT_Command("AT27=(0)");
  delay(100);
  Write_AT_Command("AT29=(16,32,5,5,1)");
  delay(1000);
  Write_AT_Command(ldmCmdFMR);
  delay (2000);
  // Use WiFiClient class to create TCP connections
  //    WiFiClient client;

  Write_AT_Command("ATd0=()");
  delay(100);
#ifdef USING_LATEST_FIRMWARE
  Write_AT_Command("ATef=(96)");
#endif
  Write_AT_Command(ldmCmdHdr);
  delay(100);
#ifdef USING_LATEST_FIRMWARE
  Write_AT_Command("ATef=(111)");
#endif
  Write_AT_Command(ldmCmd);
  delay(100);
#ifdef USING_LATEST_FIRMWARE
  Write_AT_Command("ATef=(3)");
#endif
  Write_AT_Command(ldmCmdMsg5);
  delay(100);

  Write_AT_Command(ldmCmdMsg6);

#ifdef USING_LATEST_FIRMWARE
  Write_AT_Command("ATef=(111)");
#endif

#ifdef USING_ARDUINO_MATRIX
  MATRIX.beginDraw();
  MATRIX.noFill();
  MATRIX.noStroke();
  MATRIX.clear();
  MATRIX.stroke(0, 0, 255);
  MATRIX.textFont(Font_4x6);
  MATRIX.endDraw();
#endif

  delay(1000);

  // init utc hr buffer

  for ( i = 0; i < 9; i ++ ) {
    utc_hr_buff[i] = 0x00;
  }

} // end setup

// this is the main even loop for the Arduino project
void loop()
{
//  float flat, flon;
//  int temp;
  unsigned char ones, tens = 0;

  char c, b, a = 0;
 static unsigned char counter = 0;
  static unsigned char arrayIndex = 0;
  static bool start_counter = false;
  static bool new_data = false;

  newData = false;
  counter = 0;

  // if there are incoming bytes available
  // from the server, read them and print them:

// simple, fast parser to decode incoming packet from UDP server to display
// todo:  check the checksum, currently, no checksum checking done.
if (Udp.parsePacket()) {
  while (Udp.available()) {
    //     while (client.available()) {
    //    c = client.read();
    c = Udp.read();             // character decode, we control what's being transmitted so no reason to have sophisticated parser here...

    if ( c == 'Z' )
    {
      b = c;
    }
    if ( c == 'D' )
    {
      a = c;
    }
 
    if ( c == 'A' && b == 'Z' && a == 'D' )
    {
      if ( start_counter != true )
      {
        start_counter = true;
      }
      if ( new_data != true )
      {
        new_data = true;
      }
    }
    if ( start_counter )
    {
      counter++;

      switch ( counter )
      {
        case 3:
#ifndef USING_LATEST_FIRMWARE

          arrayIndex = 14;
          ldmCmd[arrayIndex] = 0x20;
          arrayIndex = 15;			// 15 16 - HR
          ldmCmd[arrayIndex] = c;               // correctiom here

#else
          arrayIndex = 10;
          ldmCmd[arrayIndex] = 0x20;
          arrayIndex = 11;      // 15 16 - HR
          ldmCmd[arrayIndex] = c;

#endif

          utc_hr_buff[7] = c;
          //                  tens = c - 0x30;
          tens = c;
          arrayIndex = 0;

#ifdef USING_ARDUINO_MATRIX

          matrixString[arrayIndex] = c;
#endif
          break;

        case 4:

#ifndef USING_LATEST_FIRMWARE

          arrayIndex = 16;			// 15 16 - HR
          ldmCmd[arrayIndex] = c;               // correction here
          arrayIndex = 1;
#else
          arrayIndex = 12;      // 15 16 - HR
          ldmCmd[arrayIndex] = c;               // correction here
          arrayIndex = 1;

#endif
#ifdef USING_ARDUINO_MATRIX
          matrixString[arrayIndex] = c;
#endif
          // utc_hr_buff[8] = c;
          // utc_hr_buff[9] = 0x00;
          ones = c - 0x30;
          //                  ones = c - 0x37;

          switch ( tens )                     // needs to be fast and easily computed - maybe a LUT type of solution also...
          {
            case 0x30:

#ifndef USING_LATEST_FIRMWARE
              switch ( ldmCmd[16] )			// HR
#else
              switch ( ldmCmd[12] )      // HR
#endif
              {
                
// Spring - UTC - 7
// Fall - UTC - 8

                case 0x30:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x37;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x37;        // here:  0x36 or 0x37
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x37;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x31:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x38;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x38;
#endif

#ifdef USING_ARDUINO_MATRIX

                  matrixString[0] = 0x31;
                  matrixString[1] = 0x38;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x32:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x39;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x39;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x39;
                  matrixString[2] = 0x3a;
                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }

#endif

                  break;

                case 0x33:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x32;    // here
                  ldmCmd[16] = 0x30;
#else
                  ldmCmd[11] = 0x32;
                  ldmCmd[12] = 0x30;

#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x32;
                  matrixString[1] = 0x30;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x34:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x32;
                  ldmCmd[16] = 0x31;
#else
                  ldmCmd[11] = 0x32;
                  ldmCmd[12] = 0x31;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x32;
                  matrixString[1] = 0x31;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x35:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x32;
                  ldmCmd[16] = 0x32;
#else
                  ldmCmd[11] = 0x32;
                  ldmCmd[12] = 0x32;             // UTC - 8 - FALL
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x32;
                  matrixString[1] = 0x32;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x36:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x32;
                  ldmCmd[16] = 0x33;
#else
                  ldmCmd[11] = 0x32;
                  ldmCmd[12] = 0x33;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x32;
                  matrixString[1] = 0x33;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x37:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x30;
#else
                  ldmCmd[11] = 0x30;      // fall - 0x32
                  ldmCmd[12] = 0x30;      // fall - 0x33
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x30;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x38:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x31;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x31;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x31;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x39:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x32;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x32;

#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x32;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                  default:
                  break;
              }
              break;

            case 0x31:
#ifndef USING_LATEST_FIRMWARE
              switch ( ldmCmd[16] )
#else
              switch ( ldmCmd[12] )
#endif
              {

                case 0x30:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x33;

#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x33;
#endif

#ifdef USING_ARDUINO_MATRIX

                  matrixString[0] = 0x30;
                  matrixString[1] = 0x33;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x31:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x34;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x34;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x34;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x32:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x35;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x35;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x35;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x33:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x36;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x36;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x36;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif

                  break;

                case 0x34:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x37;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x37;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x37;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x35:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x38;

#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x38;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x38;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x36:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x30;
                  ldmCmd[16] = 0x39;
#else
                  ldmCmd[11] = 0x30;
                  ldmCmd[12] = 0x39;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x30;
                  matrixString[1] = 0x39;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x37:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x30;
#else
                  ldmCmd[11] = 0x31;                  // TRANSITION POINT
                  ldmCmd[12] = 0x30;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x30;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x38:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x31;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x31;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x31;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x39:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x32;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x32;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x32;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                default:
                  break;
              }
              break;

            case 0x32:
#ifndef USING_LATEST_FIRMWARE
              switch ( ldmCmd[16] )
#else
              switch ( ldmCmd[13] )      
#endif
              {

                case 0x30:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x33;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x33;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x33;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x31:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x34;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x34;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x34;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x32:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x35;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x35;
#endif

#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x35;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                case 0x33:
#ifndef USING_LATEST_FIRMWARE
                  ldmCmd[15] = 0x31;
                  ldmCmd[16] = 0x36;
#else
                  ldmCmd[11] = 0x31;
                  ldmCmd[12] = 0x36;
#endif


#ifdef USING_ARDUINO_MATRIX
                  matrixString[0] = 0x31;
                  matrixString[1] = 0x36;
                  matrixString[2] = 0x3a;

                  if (ldmCmd[18] == 0x30 && ldmCmd[19] == 0x30)
                  {
                    Write_Matrix_String(matrixString);
                    delay(250);
                  }
#endif
                  break;

                default:
                  break;

              }
              break;

            default:
              break;
          }

          break;

        case 5:

#ifndef USING_LATEST_FIRMWARE
          arrayIndex = 18;						// 18 19 - MIN
          ldmCmd[arrayIndex] = c;
#else
          arrayIndex = 14;						// 18 19 - MIN
          ldmCmd[arrayIndex] = c;
#endif

#ifdef USING_ARDUINO_MATRIX
          matrixString[0] = c;
#endif
          break;

        case 6:
#ifndef USING_LATEST_FIRMWARE
          arrayIndex = 19;						// 18 19 - MIN
          ldmCmd[arrayIndex] = c;
#else
          arrayIndex = 15;						// 18 19 - MIN
          ldmCmd[arrayIndex] = c;
#endif

#ifdef USING_ARDUINO_MATRIX
          matrixString[1] = c;
          matrixString[2] = 0x3a;

          if (ldmCmd[19] == 0x30)
          {
            Write_Matrix_String(matrixString);
            delay(250);
          }
#endif

          break;

        case 7:
#ifndef USING_LATEST_FIRMWARE
          arrayIndex = 21;						// 21 22 - SEC
          ldmCmd[arrayIndex] = c;
#else
          arrayIndex = 17;						// 21 22 - SEC
          ldmCmd[arrayIndex] = c;

#endif

#ifdef USING_ARDUINO_MATRIX
          matrixString[2] = c;
          MATRIX.stroke(0, 0, 255);               // blue
          Write_Matrix_String(matrixString);
          delay(250);
#endif
          break;

        case 8:
#ifndef USING_LATEST_FIRMWARE
          arrayIndex = 22;						// 21 22 - SEC
          ldmCmd[arrayIndex] = c;
#else
          arrayIndex = 18;						// 21 22 - SEC
          ldmCmd[arrayIndex] = c;
#endif

#ifdef USING_ARDUINO_MATRIX
          if (ldmCmd[21] == 0x30 && ldmCmd[22] == 0x30)
          {
            Write_Matrix_String(matrixString);
            delay(250);
          }
          matrixString[2] = c;
#endif
          start_counter = false;        // just doing seconds, not 1/10ths or 1/100rds... yet
          if ( new_data ) {

            Write_AT_Command(ldmCmd);
            //      MATRIX.stroke(0, 255, 0);               // green

#ifdef USING_ARDUINO_MATRIX
            Write_Matrix_String(matrixString);
#endif
            //                  delay(1);
            new_data = false;
            counter = 0;
            a = 0x30;
            b = 0x30;
          }


          break;

        default:
#ifndef USING_LATEST_FIRMWARE
          arrayIndex = 14;
#else
          arrayIndex = 10;
#endif
          break;
      }

    }

  }

}
}  // end main loop()

// command to write string to Lumex LDM-6432 display
static void Write_AT_Command(char *string)
{

#ifdef USING_LUMEX_DISPLAY
  Serial1.print(string);
  while (Serial1.read() != 'E') {}
  delay(2);
#endif

}

// command to write string to Arduino RGB Matrix
static void Write_Matrix_String(char *string)
{

#ifdef USING_ARDUINO_MATRIX

  MATRIX.beginDraw();
  //	MATRIX.clear();
  MATRIX.textFont(Font_4x6);
  //	MATRIX.stroke(0, 0, 255);

  MATRIX.text(string, 0, 1);
  MATRIX.endDraw();

  //  delay(2);
#endif
}

// *eof
