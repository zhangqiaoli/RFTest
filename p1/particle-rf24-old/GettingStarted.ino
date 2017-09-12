/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* Updated: Dec 2014 by TMRh20
*/

//
// Hardware configuration
// from https://github.com/mshoemaker/SparkCore-RF24

/*
  PINOUTS
  http://docs.spark.io/#/firmware/communication-spi
  http://maniacbug.wordpress.com/2011/11/02/getting-started-rf24/

  SPARK CORE    SHIELD SHIELD    NRF24L01+
  GND           GND              1 (GND)
  3V3 (3.3V)    3.3V             2 (3V3)
  *A0 (CSN)      9  (D6)          3 (CE)
  A2 (SS)       10 (SS)          4 (CSN)
  A3 (SCK)      13 (SCK)         5 (SCK)
  A5 (MOSI)     11 (MOSI)        6 (MOSI)
  A4 (MISO)     12 (MISO)        7 (MISO)
  *A1(IRQ)      PC3(IRQ)          8(IRQ)

  NOTE: Also place a 10-100uF cap across the power inputs of
        the NRF24L01+.  I/O o fthe NRF24 is 5V tolerant, but
        do NOT connect more than 3.3V to pin 2(3V3)!!!
 */

//
// Last updated by SBS 2016-06-23
/// 1. Add log and cloud message
/// 2. Fix some bugs
/// 3. Error handler
/// 4. Add idle mode (role = 2) as default
//
#include "application.h"
#include "particle-rf24.h"

/****************** User Config ***************************/
/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins D6 & A2 */
RF24 radio(A0,A2);

//RF24 radio(A0,A2);
/**********************************************************/

//byte pipes[][6] = {"1Node","2Node"};
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F001LL, 0xF0F0F0F002LL };

// The various roles supported by this sketch
typedef enum { role_idle = 0, role_ping_out, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "Idle", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role = role_pong_back; // Start as a Receiver

String mLastMessage;


/*unsigned long times = 0;
unsigned long succ = 0;
unsigned long received = 0;*/
#define data_len 50
#define SEND_INTERVAL 1000

struct node
{
  uint16_t total;
  uint16_t succ;
  uint16_t fail;
  uint16_t recvres;
  uint32_t usetime;
};

node hisdata[data_len];
uint8_t write_ptr = 0;
unsigned long starttime = millis();

int ChangeRole(String data) {
  data.toUpperCase();
  if ( data == "T" && role != role_ping_out )
  {
    mLastMessage = "Transmit Role";
    Serial.println("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK");

    // Become the primary transmitter (ping out)
    role = role_ping_out;
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
    radio.stopListening();
  }
  else if ( data == "R" && role != role_pong_back )
  {
    mLastMessage = "Receive Role";
    Serial.println("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK");

    // Become the primary receiver (pong back)
    role = role_pong_back;
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
    radio.startListening();
  }
  else if ( data == "Q" && role != role_idle )
  {
    role = role_idle;
    mLastMessage = role_friendly_name[role];
    Serial.println("**STOP WORKING - PRESS 'T' or 'R' TO RESTORE");

    radio.stopListening();
  }
  else if( data == "D" )
  {
    if(role == role_ping_out)
    {
      Serial.println("***************show history data*************");
      for(int8_t i = 0;i<data_len;i++)
      {
        if(write_ptr == i)
          Serial.println("\r\n");
        Serial.printlnf("total = %d,success = %d,fail = %d,recvres= %d,failper=%.2f%%,recvresper=%.2f%%,usetime=%d",hisdata[i].total,hisdata[i].succ,hisdata[i].fail,hisdata[i].recvres,(float)hisdata[i].fail*100/hisdata[i].total,(float)hisdata[i].recvres*100/hisdata[i].total,hisdata[i].usetime);
      }
      Serial.println("***************show history data*************");
    }
  }
  return 0;
}

void setup() {
  //WiFi.off();
  Serial.begin(9600);
  Serial.println(F("Particle-RF24/Firmware/Examples/GettingStarted"));

  //Particle.variable("LastMessage", &mLastMessage, STRING);
  //Particle.function("ChangeRole", ChangeRole);

  radio.begin();
  radio.setChannel(90);    // CH=0
  radio.setRetries(5, 15);
  //radio.setAutoAck(1);
  radio.setDataRate(RF24_250KBPS);
  //radio.setRetries(3, 3);
  //radio.setAutoAck(0);
  //radio.setDataRate(RF24_2MBPS);

  // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  //radio.setPALevel(RF24_PA_LOW);
  radio.setPALevel(RF24_PA_MAX);

  // Open a writing and reading pipe on each radio, with opposite addresses
  if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else if ( role == role_pong_back )
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }
  mLastMessage = role_friendly_name[role];

  // Start the radio listening for data
  radio.startListening();

  //Print debug info
  //radio.printDetails();
  memset(hisdata,0,sizeof(node)*50);
  while( Time.now() < 2000 )
    Particle.process();

  //Serial.println(F("Application started."));
  //Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));
  debug("Particle-RF24 Application started.");
  if( !radio.isValid() ) {
    debug("Radio is not valid!");
  } else {
    radio.printDetails();
    debug("**PRESS 'T' to begin transmitting to the other node");
  }
}
char sendmsg[33] = "now-i-am-sending-data-of-num-000";
char recvmsg[33] = {0};
char respmsg[33] = "ok--now-i-am-responsing-data-000";
uint8_t times = 0;
uint16_t succ = 0;
uint16_t fail = 0;
uint16_t recv = 0;
uint16_t sendtimes = 0;
#define PLAYLOAD_LEN 32

void loop()
{
  if( radio.isValid() )
  {
    bool sentOK, getOK;
    switch( role )
    {
  /****************** Ping Out Role ***************************/
    case role_ping_out:
    {
      // Switch to a Receiver before each transmission,
      // or this will only transmit once.
      //radio.startListening();

      // Re-open the pipes for Tx'ing
      //radio.openWritingPipe(pipes[0]);
      //radio.openReadingPipe(1,pipes[1]);

      radio.stopListening();
      times++;
      sendtimes++;
      times = times%10;
      sendmsg[31] = times + '0';                                 // First, stop listening so we can talk.

      Serial.print(sendmsg);
      //unsigned long time = micros();                             // Take the time, and send it.  This will block until complete
      sentOK = radio.write( sendmsg, PLAYLOAD_LEN );

      //radio.startListening();
      if( sentOK ) {
        succ++;
        //Serial.printlnf("ok...%.2f%%\n\r", (float)succ * 100 / times);
        Serial.println(" ok");
      } else {
        fail++;
        Serial.println(F(" failed"));
      }

      radio.startListening();                                    // Now, continue listening
      unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
      boolean timeout = false;                                   // Set up a variable to indicate if a response was received or not
      while ( !radio.available() ){      // While nothing is received
        if (micros() - started_waiting_at > 200000 ){            // If waited longer than 200ms, indicate timeout and exit while loop
            //Serial.println("timeout");
            timeout = true;
            break;
        }
      }

      if ( timeout ){                                             // Describe the results
          Serial.println(F("Failed, response timed out."));
      }else{
          recv++;
          radio.read( recvmsg, PLAYLOAD_LEN );
          //unsigned long end_time = micros();
          Serial.print("recv res:");
          Serial.println(recvmsg);
      }
      if(sendtimes == SEND_INTERVAL)
      {
        unsigned long now = millis();
        hisdata[write_ptr].usetime = now - starttime;
        hisdata[write_ptr].total = sendtimes;
        hisdata[write_ptr].succ = succ;
        hisdata[write_ptr].fail = fail;
        hisdata[write_ptr].recvres = recv;
        write_ptr = write_ptr%data_len+1;
        sendtimes = 0;
        recv = 0;
        succ = 0;
        fail = 0;
        starttime = now;
      }
      // Try again 1s later
      delay(500);
    }
    break;

    case role_pong_back:
    {
  /****************** Pong Back Role ***************************/
      unsigned long got_time;
      if( radio.available()){
        bool done = false;
        while (!done) {
                                           // While there is data ready
          done = radio.read(recvmsg, PLAYLOAD_LEN );
          if( done ) {            // Get the payload
            Serial.print(F("recv data: "));
            Serial.println(recvmsg);
          }
        }
        radio.stopListening();
        char got_time = recvmsg[31];
        respmsg[31]=got_time;
        if (!radio.write( respmsg, sizeof(respmsg))){
          Serial.println(F("response failed"));
        }
        else
        {
          Serial.println("response ok");
        }
         radio.startListening();
       }
    }
    break;

   default:
     break;
   }
 }

/****************** Change Roles via Serial Commands ***************************/
  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' || c == 'R' || c == 'Q' || c == 'D') {
      String strTmp = String(c);
      ChangeRole(strTmp);
    } else if ( c == 'C' ) {
      Serial.printlnf("**RF module is %s", (radio.isPVariant() ? "available" : "not available!"));
    } else if ( c == 'P' ) {
      radio.printDetails();
    } else if ( c == 'V' ) {
      Serial.printlnf("System version: %s", System.version().c_str());
    } else if ( c == 'F' ) {
      Serial.printlnf("System enter dfu");
      System.dfu();
    }else if ( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ) {
      Serial.println("------- Help -----------");
      Serial.println("Change role: 'T'-Transmitor, 'R'-Receiver, 'Q'-Idle");
      Serial.println("Show info: 'C'-Check RF module, 'P'-Print RF Detail, 'V'-Version");
      Serial.println("------------------------");
    }
  }

  if( role == role_idle ) {
    Serial.println("idle in loop...");
    delay(2000);
  }

  delay(10);
} // Loop

// Log message to cloud, message is a printf-formatted string
void debug(String message) {
    char msg [128];
    sprintf(msg, message.c_str());
    //Particle.publish("DEBUG", msg);
    mLastMessage = msg;

    Serial.println(msg);
}
