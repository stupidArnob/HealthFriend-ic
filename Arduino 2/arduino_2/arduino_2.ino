// ECG
// MAX 30205

#include <Wire.h>
#include "Protocentral_MAX30205.h"
#include<SPI.h>
#include <TimerOne.h>
#include "MAX30003.h"
#include <Ethernet.h>

#define MAX30003_CS_PIN   7
#define CLK_PIN          6

volatile char SPI_RX_Buff[5] ;
volatile char *SPI_RX_Buff_Ptr;
int i = 0;
unsigned long uintECGraw = 0;
signed long intECGraw = 0;
uint8_t DataPacketHeader[20];
uint8_t data_len = 8;
signed long ecgdata;
unsigned long data;


char SPI_temp_32b[4];
char SPI_temp_Burst[100];


// Sensor 2
MAX30205 tempSensor;


// GSR
const int GSR = A0;
int sensorValue = 0;
int gsr_average = 0;

//~~~~~~~~~~~~~~~~~~~ Ethernet Data
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // RESERVED MAC ADDRESS
EthernetClient client;
// IPAddress server(192, 168, 1, 1);
char server[] = "api.stupidarnob.com"; 
////////////////////////////////////////


void setup() {

  Serial.begin(115200);
  Wire.begin();
  EtherINIT();
  tempSensor.begin();   // set continuos mode, active mode

  pinMode(MAX30003_CS_PIN, OUTPUT);
  digitalWrite(MAX30003_CS_PIN, HIGH); //disable device

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  //please enable the below lines if you are using older version of Protocentral_max30003 board without 32k f-clock generator
  /*SPI.setClockDivider(SPI_CLOCK_DIV4);
    pinMode(CLK_PIN,OUTPUT);

    //Start CLK timer
    Timer1.initialize(16);              // set a timer of length 100000 microseconds (or 0.1 sec - or 10Hz => the led will blink 5 times, 5 cycles of on-and-off, per second)
    Timer1.attachInterrupt( timerIsr ); // attach the service routine here
  */
  MAX30003_begin();   // initialize MAX30003
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  pinMode(5, INPUT);
  
}

void loop() {
  if(digitalRead(2)){
    Serial.println("MAX_30205_func High");
    MAX_30205_func();  
  }
  if(digitalRead(3)){
    Serial.println("MAX30003_func High");
    MAX30003_func();
  }
  if(digitalRead(4)){
    Serial.println("GSR");
    MAX30003_func();
  }
  if(digitalRead(5)){
    Serial.println("GSR");
    MAX30003_func();
  }
}

/// Ehternet
void EtherINIT()
{
  //Ethernet Data Get ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Ethernet.begin(mac);
  Serial.println("Initializing Ethernet.");
  Serial.print("IP Address        : ");
  Serial.println(Ethernet.localIP());
  if (client.connect(server, 80))
  {
    Serial.println("Connected");
    client.println();
    client.println();
    client.stop();
  }
  else
  {
    // you didn't get a connection to the server:
    Serial.println("\nConnection failed\n");
  }

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
}

// Sending Data To Server
void send_date(String s, int data){
  if (client.connect(server, 80))
    {
      client.print("GET /health-friend/api.php?");
      client.print("s=");
      client.print(s);
      client.print("&data=");
      client.print(data);

      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(server);
      client.println("Connection: close");
      client.println();
      client.println();
      client.stop();
    } else {
      Serial.println("Data Sending Problem");
    }
}

// GSR
void GSR_func() {
  long sum = 0;
  for (int i = 0; i < 10; i++)    //Average the 10 measurements to remove the glitch
  {
    sensorValue = analogRead(GSR);
    sum += sensorValue;
    delay(5);
  }
  gsr_average = sum / 10;
  Serial.print("gsr_average");
  Serial.println(gsr_average);
  send_date("gsr", gsr_average);
}



// EMG Sensor
void EMG_func() {
  int sensorValue = analogRead(A1);
  Serial.print("EMG_func");
  Serial.println(sensorValue);
  send_date("emg", sensorValue);
  delay(1);
}


// MAX 30205
void MAX_30205_func() {
  float temp = tempSensor.getTemperature(); // read temperature for every 100ms
  Serial.println(temp , 2);
  send_date("temp", temp);
  delay(1000);
}


/// ECG
void timerIsr()
{
  digitalWrite( CLK_PIN, digitalRead(CLK_PIN ) ^ 1 ); // toggle Digital6 attached to FCLK  of MAX30003
}

void MAX30003_func()
{
  MAX30003_Reg_Read(ECG_FIFO);

  unsigned long data0 = (unsigned long) (SPI_temp_32b[0]);
  data0 = data0 << 24;
  unsigned long data1 = (unsigned long) (SPI_temp_32b[1]);
  data1 = data1 << 16;
  unsigned long data2 = (unsigned long) (SPI_temp_32b[2]);
  data2 = data2 >> 6;
  data2 = data2 & 0x03;

  data = (unsigned long) (data0 | data1 | data2);
  ecgdata = (signed long) (data);

  MAX30003_Reg_Read(RTOR);
  unsigned long RTOR_msb = (unsigned long) (SPI_temp_32b[0]);
  // RTOR_msb = RTOR_msb <<8;
  unsigned char RTOR_lsb = (unsigned char) (SPI_temp_32b[1]);

  unsigned long rtor = (RTOR_msb << 8 | RTOR_lsb);
  rtor = ((rtor >> 2) & 0x3fff) ;

  float hr =  60 / ((float)rtor * 0.008);
  unsigned int HR = (unsigned int)hr;  // type cast to int

  unsigned int RR = (unsigned int)rtor * 8 ; //8ms

  //     Serial.print(RTOR_msb);
  //     Serial.print(",");
  //     Serial.print(RTOR_lsb);
  //     Serial.print(",");
  //     Serial.print(rtor);
  //     Serial.print(",");
  //     Serial.print(rr);
  //     Serial.print(",");
  //     Serial.println(hr);

  for (i = 0; i < 19; i++) // transmit the data
  {
    // Serial.write(DataPacketHeader[i]);
  }

  // Serial.println(ecgDataSend);
  send_date("ecg", ecgdata / 1000);
  Serial.println(ecgdata / 1000);

  delay(8);
}

void MAX30003_Reg_Write (unsigned char WRITE_ADDRESS, unsigned long data)
{

  // now combine the register address and the command into one byte:
  byte dataToSend = (WRITE_ADDRESS << 1) | WREG;

  // take the chip select low to select the device:
  digitalWrite(MAX30003_CS_PIN, LOW);

  delay(2);
  SPI.transfer(dataToSend);   //Send register location
  SPI.transfer(data >> 16);   //number of register to wr
  SPI.transfer(data >> 8);    //number of register to wr
  SPI.transfer(data);      //Send value to record into register
  delay(2);

  // take the chip select high to de-select:
  digitalWrite(MAX30003_CS_PIN, HIGH);
}

void max30003_sw_reset(void)
{
  MAX30003_Reg_Write(SW_RST, 0x000000);
  delay(100);
}

void max30003_synch(void)
{
  MAX30003_Reg_Write(SYNCH, 0x000000);
}

void MAX30003_Reg_Read(uint8_t Reg_address)
{
  uint8_t SPI_TX_Buff;

  digitalWrite(MAX30003_CS_PIN, LOW);

  SPI_TX_Buff = (Reg_address << 1 ) | RREG;
  SPI.transfer(SPI_TX_Buff); //Send register location

  for ( i = 0; i < 3; i++)
  {
    SPI_temp_32b[i] = SPI.transfer(0xff);
  }

  digitalWrite(MAX30003_CS_PIN, HIGH);
}

void MAX30003_Read_Data(int num_samples)
{
  uint8_t SPI_TX_Buff;

  digitalWrite(MAX30003_CS_PIN, LOW);

  SPI_TX_Buff = (ECG_FIFO_BURST << 1 ) | RREG;
  SPI.transfer(SPI_TX_Buff); //Send register location

  for ( i = 0; i < num_samples * 3; ++i)
  {
    SPI_temp_Burst[i] = SPI.transfer(0x00);
  }

  digitalWrite(MAX30003_CS_PIN, HIGH);
}

void MAX30003_begin()
{
  max30003_sw_reset();
  delay(100);
  MAX30003_Reg_Write(CNFG_GEN, 0x081007);
  delay(100);
  MAX30003_Reg_Write(CNFG_CAL, 0x720000);  // 0x700000
  delay(100);
  MAX30003_Reg_Write(CNFG_EMUX, 0x0B0000);
  delay(100);
  MAX30003_Reg_Write(CNFG_ECG, 0x805000);  // d23 - d22 : 10 for 250sps , 00:500 sps
  delay(100);

  MAX30003_Reg_Write(CNFG_RTOR1, 0x3fc600);
  max30003_synch();
  delay(100);
}
