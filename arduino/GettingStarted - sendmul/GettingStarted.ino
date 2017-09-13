
/*
* Getting Started example sketch for nRF24L01+ radios
* This is a very basic example of how to send data from one node to another
* Updated: Dec 2014 by TMRh20
*/

#include <SPI.h>
#include "RF24.h"

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(7,8);
/**********************************************************/
#define sendnum 3
byte addresses[][6] = {"1Node","DTIT0"};
//uint64_t writeaddr = (uint64_t)0xF0F0F0F0E1LL;
uint64_t writeaddrarr[3] = {(uint64_t)0xF0F0F0F001LL,(uint64_t)0xF0F0F0F071LL,(uint64_t)0xF0F0F0F0E1LL};
uint64_t writeaddr = (uint64_t)0xF0F0F0F0E1LL;
uint64_t readaddr = (uint64_t)0xF0F0F0F0D2LL;
uint64_t readaddr1 = (uint64_t)0xF0F0F0F002LL;
uint64_t readaddr2 = (uint64_t)0xF0F0F0F072LL;
uint64_t readaddr3 = (uint64_t)0xF0F0F0F0D2LL;
char sendmsg[33] = "now-i-am-sending-data-of-num-000";
char recvmsg[33] = {0};
char respmsg[33] = "ok--now-i-am-responsing-data-000";

uint16_t succ[sendnum] = {0};
uint16_t fail[sendnum] = {0};
uint16_t recv[sendnum] = {0};
uint8_t times = 0;
uint16_t sendtimes= 0;
#define PLAYLOAD_LEN 32

#define data_len 25
#define SEND_INTERVAL 1000

struct node
{
  uint16_t total;
  uint16_t succ;
  uint16_t fail;
  uint16_t recvres;
  uint32_t usetime;
};

node hisdata[sendnum][data_len];
uint8_t write_ptr = 0;
unsigned long starttime = millis();

int ShowHistory() {
  Serial.println("***************show history data*************");
  for(int8_t j=0;j<sendnum;j++)
  {
    Serial.println("-------------------------------------------------");
    for(int8_t i = 0;i<data_len;i++)
    {
      if(write_ptr == i)
        Serial.println("\r\n");
      Serial.print("total =");
      Serial.print(hisdata[j][i].total);
      Serial.print(",success =");
      Serial.print(hisdata[j][i].succ);
      Serial.print(",fail =");
      Serial.print(hisdata[j][i].fail);
      Serial.print(",recvres =");
      Serial.print(hisdata[j][i].recvres);
      Serial.print(",failper =");
      Serial.print((float)hisdata[j][i].fail*100/hisdata[j][i].total);
      Serial.print(",recvresper =");
      Serial.print((float)hisdata[j][i].recvres*100/hisdata[j][i].total);
      Serial.print(",usetime =");
      Serial.println(hisdata[j][i].usetime);
    }
  }

  Serial.println("***************show history data*************");
  return 0;
}
// Used to control whether this node is sending or receiving
bool role = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("RF24/examples/GettingStarted"));
  Serial.println(F("*** PRESS 'T' to begin transmitting to the other node"));
  //SPI.setClockDivider(SPI_CLOCK_DIV8);
  radio.begin();
  //radio.setAutoAck(1);
  // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setRetries(5, 15);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(90);
  // Open a writing and reading pipe on each radio, with opposite addresses
  if(role){
    radio.openWritingPipe(writeaddr);
    radio.openReadingPipe(1,readaddr1);
    //radio.openReadingPipe(2,readaddr2);
    //radio.openReadingPipe(3,readaddr3);
  }else{
    radio.openWritingPipe(readaddr);
    radio.openReadingPipe(1,writeaddr);
  }
  
  // Start the radio listening for data
  radio.startListening();
  bool chip = radio.isChipConnected();
  Serial.print(F("begin chip is "));
  Serial.println(chip);
  memset(hisdata,0,sizeof(node)*sendnum*data_len);
}

void loop() {
  if(radio.isChipConnected())
  {
    /****************** Ping Out Role ***************************/  
    if (role == 1)  {          
      //Serial.println(sendmsg);
      //unsigned long time = micros();                             // Take the time, and send it.  This will block until complete
      times++;
      sendtimes++;
      times = times%10;
      sendmsg[31] = times + '0';                                 // First, stop listening so we can talk.
      for(int i= 0;i<sendnum;i++)
      {
        radio.stopListening();
        radio.openWritingPipe(writeaddrarr[i]);    
        bool sentOK = radio.write( sendmsg, PLAYLOAD_LEN );
        Serial.print(F("pipe "));
        Serial.print(i+1); 
        Serial.print(sendmsg);
        if( sentOK ) {
          succ[i]++;
          Serial.println(" ok");
        } else {
          fail[i]++;
          Serial.println(F(" failed"));
        }

        radio.startListening();                                    // Now, continue listening
        
        unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
        boolean timeout = false;                                   // Set up a variable to indicate if a response was received or not
        while ( !radio.available() ){      // While nothing is received
          if (micros() - started_waiting_at > 500000 ){            // If waited longer than 200ms, indicate timeout and exit while loop
              //Serial.println("timeout");
              timeout = true;
              break;
          }      
        }
            
        if ( timeout ){                                             // Describe the results
            Serial.println(F("Failed, response timed out."));
        }else{
          uint8_t pipenum = 0;
          while( radio.available(&pipenum)){
            Serial.print(F("pipe "));
            Serial.print(pipenum);
             recv[pipenum-1]++;
            //uint32_t got_time; 
            // Grab the response, compare, and send to debugging spew
            radio.read( recvmsg, PLAYLOAD_LEN );
            //unsigned long end_time = micros();
            Serial.print(" recv res:");
            Serial.println(recvmsg);
          }
        }
        delay(500);
      }         

        if(sendtimes == SEND_INTERVAL)
        {
          unsigned long now = millis();
          for(int i= 0;i<sendnum;i++)
          {
            hisdata[i][write_ptr].usetime = now - starttime;
            hisdata[i][write_ptr].total = sendtimes;
            hisdata[i][write_ptr].succ = succ[i];
            hisdata[i][write_ptr].fail = fail[i];
            hisdata[i][write_ptr].recvres = recv[i];

            recv[i] = 0;
            succ[i] = 0;
            fail[i] = 0;
          }
          sendtimes = 0;
          write_ptr = write_ptr%data_len+1;
          starttime = now;
        }
        /*Serial.print("success sent ");
        Serial.print((float)succ*100/times);
        Serial.print(", success be received ");
        Serial.println((float)recv*100/times);*/
    
        // Try again 1s later
        //delay(500);
      }
    
    
    
    /****************** Pong Back Role ***************************/
    
      if ( role == 0 )
      {
        uint8_t pipenum = 0;
        if( radio.available(&pipenum)){
        //if( radio.available()){
          memset(recvmsg,0,sizeof(recvmsg));                                                              // Variable for the received timestamp
          radio.read(recvmsg, PLAYLOAD_LEN );             // Get the payload
          Serial.print(F("pipe "));
          Serial.print(pipenum);
          Serial.print(F(" recv data: "));
          Serial.println(recvmsg);
          radio.stopListening();                                        // First, stop listening so we can talk   
          char got_time = recvmsg[31];
          respmsg[31]=got_time;
          //radio.write( respmsg, sizeof(respmsg) );              // Send the final one back.  
         //Serial.println(respmsg);           
         if(pipenum == 1)
         {
           radio.openWritingPipe(readaddr1);          
         }
         else if(pipenum == 2 )
         {
           radio.openWritingPipe(readaddr2);
         }else if(pipenum == 3 )
         {
           radio.openWritingPipe(readaddr3);
         }
         Serial.print(F("pipe "));
         Serial.print(pipenum);
         if (!radio.write( respmsg, sizeof(respmsg))){
           Serial.println(F(" response failed"));
         }
         else
         {
           Serial.println(" response ok");
         }   
          radio.startListening();                                       // Now, resume listening so we catch the next packets.        
       }
     }
  }
  else
  {
    Serial.println(F("Please check rf hardware!")); 
    delay(2000);
  }

/****************** Change Roles via Serial Commands ***************************/

  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' && role == 0 ){      
      Serial.println(F("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK"));
      role = 1;                  // Become the primary transmitter (ping out)
      //radio.openWritingPipe(writeaddr);
      radio.openReadingPipe(1,readaddr1);
      radio.openReadingPipe(2,readaddr2);
      radio.openReadingPipe(3,readaddr3);
      radio.stopListening();
    
   }else
    if ( c == 'R' && role == 1 ){
      Serial.println(F("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK"));      
       role = 0;                // Become the primary receiver (pong back)
       radio.openWritingPipe(readaddr);
       radio.openReadingPipe(1,writeaddr);
       radio.startListening();
    }
    else if ( c == 'D' && role == 1 ){
      ShowHistory();
    }
  }

} // Loop

