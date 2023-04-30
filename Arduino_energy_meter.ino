//
// Program using Arduino Uno, ENC28J60 and LDR to read energy usage from energy meter 
// 
//
#include <EtherCard.h>

//**********************
//**********************
//change values below
#define LDRpin 8  // pin where we connect LDR and resistor
const char website[] PROGMEM = "192.168.1.204"; // put your server address here
const int server_port = 8084; // web port of domoticz server
int IDX = 15; //IDX of domoticz device
int pulse_per_kwh = 1000; // pulses per kWh from energy meter - needs to be read from energy meter 
int refresh_rate = 6000; // time to refresh device status [ms]. Also determine minimum peak energy step
// eg. refresh_rate=6000 and pulse_per_kwh=1000 => 1 blink of led == 1*3600/6000*1000=600W step (domoticz will show
// 0W, 600W, 1200W etc on actual usage)
// eg. refresh_rate=1000 and pulse_per_kwh=1000 => 1 blink of led == 1*3600/1000*1000=3600W step (domoticz will show
// 0W, 3600W, 7200W.. etc on actual usage)
//so don't refresh too often 
//also don't refresh too rarely as you'll not see short peak's


static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };// ethernet interface mac address, must be unique on the LAN

//**********************
//**********************

//define variables
int LDRValue = 0; // result of reading the analog pin
long counting = 0; //counting LED pulses
long energy_used = 0; //for easy read of code
int last_state = 0; // last state of LED
int current_state = 0; // current LED state
char energy_value[100]; //total energy reading
long prev_value =0; // previous state of LED
int peak = 0; // Power reading
int peak_power = 0; //for easy read of code
int peak_used = 0; //for easy read of code
char peak_to_send[100]; // int data converted to char req by browseUrl
char to_send[100]; // int data converted to char req by browseUrl

byte Ethernet::buffer[700];
static uint32_t timer;


// function called under get request
static void my_callback (byte status, word off, word len) {
  Ethernet::buffer[off+300] = 0;
}


void setup () {
  Serial.begin(57600); //for debugging
  Serial.println(F("\n[webClient]"));
  // Change 'SS' to your Slave Select pin, if you arn't using the default pin
  if (ether.begin(sizeof Ethernet::buffer, mymac, SS) == 0)
    Serial.println(F("Failed to access Ethernet controller"));
  if (!ether.dhcpSetup())
    Serial.println(F("DHCP failed"));
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);
  ether.printIp("DNS: ", ether.dnsip);

#if 1
  // use DNS to resolve the website's IP address
  if (!ether.dnsLookup(website))
    Serial.println("DNS failed");
#endif
  ether.parseIp(ether.hisip, website);
  ether.hisport = server_port;
  ether.printIp("SRV: ", ether.hisip);
}


void loop () {
  ether.packetLoop(ether.packetReceive());
  LDRValue = digitalRead(LDRpin);// read the value from the LDR
  
  //if LED changed state increase counter
  if (LDRValue != last_state) { 
    if (LDRValue ==1){
      last_state = LDRValue;
      counting++;
    }
  else {
    last_state = 0;
  }
   }
  
  energy_used = counting * 1000 / pulse_per_kwh; // convert pulses from meter to Wh 
  //(sending  to domoticz 1 for 0.001kWh, 1000 for 1kWh)
  ultoa( energy_used, energy_value, 10 ); //int->char
    peak = counting - prev_value; //Instant consumption
    peak_used = peak * 1000 / pulse_per_kwh; //convert pulses from meter to watts
    peak_power = peak_used*60*60/refresh_rate*1000; //convert peak to kW
  ultoa( peak_power, peak_to_send, 10 ); //pack int to char
  if (millis() > timer) {
    timer = millis() + refresh_rate; //timer 2000 == peak_power /2

  //create char to_send in format IDX&nvalue=0&svalue=POWER;ENERGY
  // full link required by domoticz: website:server_port/json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=POWER;ENERGY
  sprintf(to_send,"%s%s", IDX , "&nvalue=0&svalue=" );
  sprintf(to_send,"%s%s", to_send, peak_to_send );
  sprintf(to_send,"%s%s",to_send,";");
  sprintf(to_send,"%s%s",to_send, energy_value);
  
  Serial.println(to_send);
  ether.browseUrl(PSTR("/json.htm?type=command&param=udevice&idx="), to_send , website, my_callback); //send meter read
  Serial.print("Meters sent");
  prev_value=counting;
  }
}