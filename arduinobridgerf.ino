/**
 * @author Rogier van den Berg / http://www.rogiervandenberg.nl
 *
 * This project is to send/receive RF-signals (KaKu, Elro, etc).
 * Its purpose is to function as a proxy/bridge, so you can toggle lights or receive commands in your domotica solution.
 *
 * It is designed to work with combined with https://github.com/bslagter/hue-control
 * You need an Arduino with Ethternet for this to work.
 * 
 * I am using the interpretation of RF-signals from the RemoteSwitch Libraries by Randy Simons,
 * as found at https://bitbucket.org/fuzzillogic/433mhzforarduino/wiki/Home
 * 
 * For processing the raw RF-signals for the New style KaKu I am using the code from Tymmo at http://gathering.tweakers.net/forum/list_message/37257667#37257667
 *
 ==How to use==
 *  Send an UDP Package to port 8888 to execute (send) a command over RF.
 *  Listen for UDP Packages on port 8889 to receive RF-commands
 * 
 * A command is built with 5 comma separated values:
 *  - Direction of the message: O or I (outgoing, incoming);
 *  - Library that applies: 1, 2 or 3 (NewRemoteSwitch, RemoteSwitch or RemoteSensor)
 *  - Code: xxxx (any value)
 *  - Period: xxx (microseconds)
 *  - Additional details: 0000 (4 digits, containing: Type (0=Off, 1=On, 2=Dim), 2 digits for Unit (which light), Dim value (always 0 for now)
 * 
 * E.g: O,1,2559398,254,1200 (outgoing, NewRemoteswitch, code, period, type 1 / unit 2 / dim 0 / group 0) --> Turn light 2 of remote 2559398 ON.
 */

#include <RemoteReceiver.h>
#include <RemoteTransmitter.h>
#include <InterruptChain.h>
#include <SPI.h>
#include <Ethernet.h>
#include <String.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //Mac Address (IP will be dynamic by DHCP)
IPAddress broadcast(255, 255, 255, 255); //Where to broadcast incoming signals to
unsigned int localPort = 8888;      // local port to listen on
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

//Connections of the RF transmitter and LED to the Arduino board (receiver is 2)
const int led =  4; 
const int transmitterPin = 8;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

signed short stateCounter = -1; 
boolean sending = false; 

void setup()
{
  pinMode(led, OUTPUT); //For connecting a LED
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  
  // start the Ethernet connection:
  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }

  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  Serial.println();
  
  //Start accepting UDP
  Udp.begin(localPort);
  
  // Interrupt -1 to indicate you will call the interrupt handler with InterruptChain
  RemoteReceiver::init(-1, 2, handleOldCode);
  
  attachInterrupt(0, interruptHandler, CHANGE);  
  
  // On interrupt, call the interrupt handlers of remote and sensor receivers
  InterruptChain::addInterruptCallback(0, RemoteReceiver::interruptHandler);
  InterruptChain::addInterruptCallback(0, interruptHandler);
}

void loop() {
    
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();

  if(packetSize) {
    
    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.print("Binnen gekregen: ");
    Serial.println(packetBuffer);
    
    handleCommand(packetBuffer);
  }
}

//If we receive a command, parse it to do something with it
void handleCommand(char* receivedCommand) {
  Serial.print("Verwerken: ");
  Serial.println(receivedCommand);
  
  char *p = receivedCommand;
  char *str;
  int count = 0;
  String commandValues[5];
  
  unsigned long receivedCode = 0;
  unsigned int period = 0;
  
  while ((str = strtok_r(p, ",", &p)) != NULL) {
    
    if (count == 2)
    {
      receivedCode = atol(str); //Convert to long integer
    }
    if (count == 3)
    {
      period = atoi(str); //Convert to integer
    }
    
    commandValues[count++] = str;
    Serial.println(str);
  }
  if (commandValues[0] == "O") {
      Serial.println("Transmitting over RF:");
      Serial.println(receivedCode);
      Serial.println(period);
      transmitCode(receivedCode, period); 
  }
}

//Handles incoming RF data, part of this code is made
// by Tymmo: http://gathering.tweakers.net/forum/list_message/37257667#37257667
void interruptHandler() 
{ 
  if(sending) 
    return; 
   
  static byte receivedBit; 
  static unsigned long receivedCommand;   
  static unsigned long lastChange = 0; 
  unsigned long currentTime=micros(); 
  unsigned int duration=currentTime-lastChange; 
  lastChange=currentTime; 
   
  if(duration > 2200 && duration < 2800)  // Startbit 
  { 
    stateCounter = 0; 
    receivedBit = 0; 
    receivedCommand = 0; 
  } 
  else if(!(stateCounter & 0x80)) // stateCounter>=0 && stateCounter < 128 
  { 
    if(duration > 1500) 
    { 
      stateCounter = -1; 
      return;   
    } 
      
    receivedBit <<= 1; 
     
    if(duration > 1000) 
      receivedBit |= 0x01; 

    if((stateCounter%4) == 3) 
    { 
      receivedCommand <<= 1; 
         
      if(receivedBit == 4)        // -___-_ '1' 
        receivedCommand |= 0x01; 
      else if(receivedBit == 1)   // -_-___ '0' 
        ;  // No need to handle 
      else if(receivedBit == 0)   // -_-_  Dimming 
        ;  // Unhandled for now 
      else 
      { 
        stateCounter = -1; 
        return;   
      } 
         
      receivedBit = 0; 
    }   

    stateCounter++; 
  } 
  else if(stateCounter == 128) 
  { 
    Serial.print(receivedCommand); 
    Serial.print("  Address:"); 
    Serial.print(receivedCommand >> 6); 
    Serial.print("  Group:"); 
    Serial.print((receivedCommand >> 5)&0x01); 
    Serial.print("  Cmd:"); 
    Serial.print((receivedCommand >> 4)&0x01); 
    Serial.print("  Unit:"); 
    Serial.println((receivedCommand)&0x0F); 
               
               
     //Broadcasting received RF-signal
    Udp.beginPacket(broadcast, 8889);
    Udp.write("I,1,");

    char charBuf[50];
    String codeString(receivedCommand >> 6);
    codeString.toCharArray(charBuf, 50);  
    Udp.write(charBuf);
    Udp.write(",");

    String periodString("000");
    periodString.toCharArray(charBuf, 50);  
    Udp.write(charBuf);
    Udp.write(",");
  
    //Type
    String typeString((receivedCommand >> 4)&0x01);
    typeString.toCharArray(charBuf, 50);  
    Udp.write(charBuf);
  
    //Unit
    String unitString((receivedCommand)&0x0F);
    if (unitString.length() == 1) {
      unitString = "0" + unitString;
    }
    unitString.toCharArray(charBuf, 50);  
    Udp.write(charBuf);

    //Dim value
    String dimString("0");
    dimString.toCharArray(charBuf, 50);  
    Udp.write(charBuf);
  
    Udp.endPacket();            
               
    stateCounter = -1; 
  } 
} 

//Sending RF signals, overloaded function to build raw signal
void sendSwitch(unsigned long address, boolean group, boolean state, short unit, short repeats) 
{ 
  unsigned long transmitCommand; 
   
  transmitCommand = address << 6; 
  transmitCommand |= group << 5; 
  transmitCommand |= state<<4; 
  if(!group) 
    transmitCommand |= unit & 0x0f; 
   
  sendSwitch(transmitCommand, repeats); 
} 

//Sending Raw RF signals for new KaKu
void sendSwitch(unsigned long transmitCommand, short repeatCount) 
{ 
  sending = true; 
   
  Serial.print("Sending: "); 
  Serial.println(transmitCommand); 
   
  for(short repeats = 1; repeats <= repeatCount; repeats++) 
  { 
    digitalWrite(11, HIGH); 
    delayMicroseconds(330); 
    digitalWrite(11, LOW); 
    delayMicroseconds(2600); 
   
    for(short i = 31; i>=0; i--) 
    { 
      digitalWrite(11, HIGH); 
      delayMicroseconds(330);   
   
      if(bitRead(transmitCommand, i)) 
      { 
        digitalWrite(11, LOW); 
        delayMicroseconds(1200);   
        digitalWrite(11, HIGH); 
        delayMicroseconds(330); 
        digitalWrite(11, LOW); 
        delayMicroseconds(200); 
      } 
      else 
      { 
        digitalWrite(11, LOW); 
        delayMicroseconds(200);   
        digitalWrite(11, HIGH); 
        delayMicroseconds(330); 
        digitalWrite(11, LOW); 
        delayMicroseconds(1200); 
      } 
    } 
   
    digitalWrite(11, HIGH); 
    delayMicroseconds(330); 
    digitalWrite(11, LOW); 
    delayMicroseconds(10200);  
  } 
  
  sending = false;  
}


// shows the received code sent from an old-style remote switch
void handleOldCode(unsigned long receivedCode, unsigned int period) {
  // Print the received code.
  Serial.print("Code: ");
  Serial.print(receivedCode);
  Serial.print(", period: ");
  Serial.print(period);
  Serial.println("us.");
  
  //Broadcasting received RF-signal
  Udp.beginPacket(broadcast, 8889);
  Udp.write("I,2,");

  char charBuf[50];
  String codeString(receivedCode);
  codeString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",");

  String periodString(period);
  periodString.toCharArray(charBuf, 50);  
  Udp.write(charBuf);
  Udp.write(",0000");
  Udp.endPacket(); 
}




void transmitCode(unsigned long receivedCode, unsigned int period) {  
  
  Serial.print("We gaan sturen: ");
  Serial.print(receivedCode);
  Serial.print(" / ");
  Serial.println(period);

  RemoteReceiver::disable();
  interrupts();
  RemoteTransmitter::sendCode(8, receivedCode, period, 3);
  RemoteReceiver::enable();
}


