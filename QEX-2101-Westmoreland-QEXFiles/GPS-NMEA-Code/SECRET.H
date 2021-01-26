// *
// Example program to illustrate how to display time on an RGB display based on NMEA data received via GPS.
// Currently coded for PDT - you'll need to adjust for your timezone.
// 
// The source contained here goes with companion article for ARRL's QEX "A DIY NMEA Based GPS Time Display".
// Author - John C. Westmoreland
// Date -   April 27, 2020
// *

// this header is to put your AP information:

// the following are for normal operation from the AP the GPS receiver is attached to
#define SECRET_WIFI_NAME (const char*)   "THEMIS_WS2"
#define SECRET_PASSWORD  (const char*)   "1234567890AB"

// the following is for test purposes only
#define SECRET_WIFI_NAME_TEST_MODE (const char*)   "AJ6BC-ARGUS"
#define SECRET_PASSWORD_TEST_MODE  (const char*)   "1234567890AB"
