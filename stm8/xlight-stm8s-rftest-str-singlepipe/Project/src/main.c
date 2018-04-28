#include "_global.h"
#include "rf24l01.h"
#include "Uart2Dev.h"
#include "stdio.h"

/*
License: MIT

Auther: Baoshi Sun
Email: bs.sun@datatellit.com, bs.sun@uwaterloo.ca
Github: https://github.com/sunbaoshi1975
Please visit xlight.ca for product details

RF24L01 connector pinout:
GND    VCC
CE     CSN
SCK    MOSI
MISO   IRQ

Connections:
  PC3 -> CE
  PC4 -> CSN
  PC7 -> MISO
  PC6 -> MOSI
  PC5 -> SCK
  PC2 -> IRQ

*/

// Xlight Application Identification
#define XLA_VERSION               0x03
#define XLA_ORGANIZATION          "xlight.ca"               // Default value. Read from EEPROM

// Choose Product Name & Type
#define XLA_PRODUCT_NAME          "RFTester"
#define XLA_PRODUCT_Type          devtypUnknown

// Window Watchdog
// Uncomment this line if in debug mode
#define DEBUG_NO_WWDG
#define WWDG_COUNTER                    0x7f
#define WWDG_WINDOW                     0x77

// RF channel for the sensor net, 0-127
#define RF24_CHANNEL	   		90

// System Startup Status
#define SYS_INIT                        0
#define SYS_RESET                       1
#define SYS_WAIT_NODEID                 2
#define SYS_WAIT_PRESENTED              3
#define SYS_RUNNING                     5

////////////////config///////////////////////
uint8_t receiver = 1;
//const UC RF24_BASE_RADIO_ID[ADDRESS_WIDTH] = {0x00,0xF0,0xF0,0xF0,0xF0};
const UC RF24_BASE_RADIO_ID[ADDRESS_WIDTH] = {0xBA,0xF0,0xF0,0xF0,0xF0};

uint8_t sendData[33] = {0};
unsigned char sendMsg[33] = "dev-i-am-sending-data-of-num-000";
unsigned char recvMsg[33] = {0};
unsigned char respMsg[33] = "ok--dev-i-am-responsing-data-000";
uint8_t times = 0;
uint16_t succ = 0;
uint16_t fail = 0;
uint16_t recv = 0;
uint16_t sendtimes = 0;
int32_t lastRecvNum = -1;

#define DATA_LEN 25
#define SEND_INTERVAL 1000

typedef struct tagnode
{
  uint16_t total;
  uint16_t succ;
  uint16_t fail;
  uint16_t recvres;
}node;

node hisdata[DATA_LEN];
uint8_t write_ptr = 0;

///////////////config////////////////////////


// Public variables
Config_t gConfig;
bool gIsChanged = FALSE;

// Moudle variables
uint8_t mStatus = SYS_INIT;
uint8_t mutex = 0;

uint8_t sLog[50];
uint8_t sConsole[100];
bool bMsgReady = 0;

// Initialize Window Watchdog
void wwdg_init() {
#ifndef DEBUG_NO_WWDG  
  WWDG_Init(WWDG_COUNTER, WWDG_WINDOW);
#endif  
}

// Feed the Window Watchdog
void feed_wwdg(void) {
#ifndef DEBUG_NO_WWDG    
  uint8_t cntValue = WWDG_GetCounter() & WWDG_COUNTER;
  if( cntValue < WWDG_WINDOW ) {
    WWDG_SetCounter(WWDG_COUNTER);
  }
#endif  
}

void Flash_ReadBuf(uint32_t Address, uint8_t *Buffer, uint16_t Length) {
  assert_param(IS_FLASH_ADDRESS_OK(Address));
  assert_param(IS_FLASH_ADDRESS_OK(Address+Length));
  
  for( uint16_t i = 0; i < Length; i++ ) {
    Buffer[i] = FLASH_ReadByte(Address+i);
  }
}

void Flash_WriteBuf(uint32_t Address, uint8_t *Buffer, uint16_t Length) {
  assert_param(IS_FLASH_ADDRESS_OK(Address));
  assert_param(IS_FLASH_ADDRESS_OK(Address+Length));
  
  // Init Flash Read & Write
  FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
  FLASH_Unlock(FLASH_MEMTYPE_DATA);
  while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET);
  
  uint8_t WriteBuf[FLASH_BLOCK_SIZE];
  uint16_t nBlockNum = (Length - 1) / FLASH_BLOCK_SIZE + 1;
  for( uint16_t block = 0; block < nBlockNum; block++ ) {
    memset(WriteBuf, 0x00, FLASH_BLOCK_SIZE);
    for( uint16_t i = 0; i < FLASH_BLOCK_SIZE; i++ ) {
      WriteBuf[i] = Buffer[block * FLASH_BLOCK_SIZE + i];
    }
    FLASH_ProgramBlock(block, FLASH_MEMTYPE_DATA, FLASH_PROGRAMMODE_STANDARD, WriteBuf);
    FLASH_WaitForLastOperation(FLASH_MEMTYPE_DATA);
  }
  
  FLASH_Lock(FLASH_MEMTYPE_DATA);
}

uint8_t idleProcess() {
  return mutex;
}

bool WaitMutex(uint32_t _timeout) {
  while(_timeout--) {
    if( idleProcess() > 0 ) return TRUE;
    feed_wwdg();
  }
  return FALSE;
}

// Save config to Flash
void SaveConfig()
{
  if( gIsChanged ) {
    Flash_WriteBuf(FLASH_DATA_START_PHYSICAL_ADDRESS, (uint8_t *)&gConfig, sizeof(gConfig));
    gIsChanged = FALSE;
  }
}

// Initialize Node Address and look forward to being assigned with a valid NodeID by the SmartController
void InitNodeAddress() {
  memcpy(gConfig.NetworkID, RF24_BASE_RADIO_ID, ADDRESS_WIDTH);
}

void UpdateNodeAddress(void) {
  memcpy(rx_addr, gConfig.NetworkID, ADDRESS_WIDTH);
  memcpy(tx_addr, gConfig.NetworkID, ADDRESS_WIDTH);
  if( gConfig.nodeID == 'r' ) {
    rx_addr[0] = 0x71;
    tx_addr[0] = 0x72;
  } else {
    rx_addr[0] = 0x72;
    tx_addr[0] = 0x71;
  }
  RF24L01_setup(RF24_CHANNEL, 0);
  if( gConfig.nodeID == 'r' ) {
    RF24L01_set_mode_RX();
  } else {
    RF24L01_set_mode_TX();
  }
}

// Load config from Flash
void LoadConfig()
{
    // Load the most recent settings from FLASH
    Flash_ReadBuf(FLASH_DATA_START_PHYSICAL_ADDRESS, (uint8_t *)&gConfig, sizeof(gConfig));
    if( (gConfig.nodeID != 'r' && gConfig.nodeID != 't') || gConfig.rfPowerLevel > 3 ) {
      memset(&gConfig, 0x00, sizeof(gConfig));
      gConfig.version = XLA_VERSION;
      gConfig.nodeID = 'r';
      gConfig.rfPowerLevel = 3;
      InitNodeAddress();

      gIsChanged = TRUE;
      SaveConfig();
    }
}

uint32_t htonl(uint32_t n) 
{ 
return ((n & 0xff) << 24) | 
((n & 0xff00) << 8) | 
((n & 0xff0000) >> 8) | 
((n & 0xff000000) >> 24); 
} 

int atoi(char *str)
{
        int value = 0;
        while(*str>='0' && *str<='9')
        {
                value *= 10;
                value += *str - '0';
                str++;
        }
        return value;
}

// Send message and switch back to receive mode
bool SendMyMessage() {
  mutex = 0;
  RF24L01_set_mode_TX();
  RF24L01_write_payload((uint8_t *)sendData, PLOAD_WIDTH);
  
  WaitMutex(0x1FFFF);
  if (mutex != 1) {
    //The transmission failed, Notes: mutex == 2 doesn't mean failed
    //It happens when rx address defers from tx address
    //asm("nop"); //Place a breakpoint here to see memory
    fail++;
    memset(sLog, 0x00, sizeof(sLog));
    sprintf(sLog, "%s...failed\n\r", sendData);
    Uart2SendString(sLog);
  } else {
    succ++;
    memset(sLog, 0x00, sizeof(sLog));
    sprintf(sLog, "%s...OK\n\r", sendData);
    Uart2SendString(sLog);
  }
  
  // Switch back to receive mode
  RF24L01_set_mode_RX();
  
  return(mutex > 0);
}

int main( void ) {
  
  //After reset, the device restarts by default with the HSI clock divided by 8.
  //CLK_DeInit();
  /* High speed internal clock prescaler: 1 */
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);  // now: HSI=16M prescale = 1; sysclk = 16M
  //CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV2);  // now: HSI=16M prescale = 2; sysclk = 8M
  //CLK_HSECmd(ENABLE);
  // Init PWM Timers
  //initTim2PWMFunction();

  // Init serial ports
  memset(sLog, 0x00, sizeof(sLog));
  uart2_config(9600);
  
  RF24L01_init();
  while(!NRF24L01_Check())feed_wwdg();
  // IRQ
  NRF2401_EnableIRQ();
  
  LoadConfig();
  InitNodeAddress();
  gConfig.nodeID = 'r';
  UpdateNodeAddress();
  
  mStatus = SYS_RUNNING;
  
  memset(sConsole, 0x00, sizeof(sConsole));
  sprintf(sConsole, "**PRESS 'T' to begin transmitting to the other node\n\r");
  Uart2SendString(sConsole);

  RF24L01_show_registers();
  
  uint32_t send_tick = 0;
  memset(hisdata,0x00,sizeof(node)*DATA_LEN);
  while (mStatus == SYS_RUNNING) {
    if( gConfig.nodeID == 'r' ) {
      if(bMsgReady)
      {
        bMsgReady = 0;
        SendMyMessage();
      }
    } else if( gConfig.nodeID == 't' ) {
      if( !send_tick ) {
        send_tick = 0x0001FFFF;
        times++;
        sendtimes++;
        times %= 10;
        sendMsg[31] = times + '0';
        memcpy(sendData,sendMsg,PLOAD_WIDTH);
        SendMyMessage();
        if(sendtimes == SEND_INTERVAL)
        {
          hisdata[write_ptr].total = sendtimes;
          hisdata[write_ptr].succ = succ;
          hisdata[write_ptr].fail = fail;
          hisdata[write_ptr].recvres = recv;
          write_ptr = write_ptr%DATA_LEN+1;
          sendtimes = 0;
          recv = 0;
          succ = 0;
          fail = 0;
        }
      } else if( send_tick > 0 ) {
        send_tick--;
      }
    }
  }
}

INTERRUPT_HANDLER(EXTI_PORTC_IRQHandler, 5) {
  if(RF24L01_is_data_available()) {
    //Packet was received
    RF24L01_clear_interrupts();
    memset(recvMsg,0,sizeof(recvMsg));
    RF24L01_read_payload(recvMsg, PLOAD_WIDTH);
    memset(sLog, 0x00, sizeof(sLog));
    uint16_t recvnum = atoi(&recvMsg[29]);
    sprintf(sLog, "Rece data: %d\n\r", recvnum);
    Uart2SendString(sLog);
    if( gConfig.nodeID == 'r' )
    {
      if(lastRecvNum != -1)
      {
        if(recvnum == 0 || recvnum < lastRecvNum)
        {
          write_ptr = write_ptr%DATA_LEN+1;
        }
      }
      lastRecvNum = recvnum;
      hisdata[write_ptr].recvres++;      
      sprintf(respMsg,"%dok-now-i-am-responsing-data-%03d",receiver,recvnum);
      memcpy(sendData,respMsg,PLOAD_WIDTH);
      bMsgReady = 1;
    }else if(gConfig.nodeID == 't')
    {
      recv++;
    }
    return;
  }
 
  uint8_t sent_info;
  if (sent_info = RF24L01_was_data_sent()) {
    //Packet was sent or max retries reached
    RF24L01_clear_interrupts();
    mutex = sent_info;
    return;
  }

   RF24L01_clear_interrupts();
}

INTERRUPT_HANDLER(UART2_RX_IRQHandler, 21)
{
  /* In order to detect unexpected events during development,
  it is recommended to set a breakpoint on the following instruction.
  */
  u8 data;
  if( UART2_GetITStatus(UART2_IT_RXNE) == SET ) {
    data = UART2_ReceiveData8();
    if( (data == 't' || data == 'T') && gConfig.nodeID == 'r' ) {
      gConfig.nodeID = 't';
      UpdateNodeAddress();
      memset(sConsole, 0x00, sizeof(sConsole));
      sprintf(sConsole, "*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n\r");
      Uart2SendString(sConsole);
    } else if( (data == 'r' || data == 'R') && gConfig.nodeID == 't' ) {
      gConfig.nodeID = 'r';
      UpdateNodeAddress();
      memset(sConsole, 0x00, sizeof(sConsole));
      sprintf(sConsole, "*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n\r");
      Uart2SendString(sConsole);
    } else if( data == 'd' || data == 'D' ) {
        memset(sConsole, 0x00, sizeof(sConsole));
        /*for(int8_t i = 0;i<DATA_LEN;i++)
        {
          sprintf(sConsole, "Succ sent total=%d,succ=%d,fail=%d,recvres=%d,failper: %.2f%%, recvresper: %.2f%%\n\r", hisdata[i].total, hisdata[i].succ, hisdata[i].fail, hisdata[i].recvres,(float)hisdata[i].fail*100/hisdata[i].total,(float)hisdata[i].recvres*100/hisdata[i].total);
          Uart2SendString(sConsole);
        }*/
        Uart2SendString("***************show history data*************");
        for(int8_t i = 0;i<DATA_LEN;i++)
        {
          if(write_ptr == i)
            Uart2SendString("\r\n");
          sprintf(sConsole,"recv = %d,recvper = %.2f%%\n\r",hisdata[i].recvres,(float)hisdata[i].recvres*100/SEND_INTERVAL);
          Uart2SendString(sConsole);
        }      
    } else if( data == 'p' || data == 'P' ) {
      RF24L01_show_registers();
    } else {
      memset(sConsole, 0x00, sizeof(sConsole));
      sprintf(sConsole, "*** Press 'R' or 'T' to change role, 'P' to print RF, 'D' to show data\n\r");
      Uart2SendString(sConsole);
    }
    UART2_ClearITPendingBit(UART2_IT_RXNE);
  }
}
