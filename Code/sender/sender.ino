#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include "LowPower.h"
#include <avr/sleep.h>
#include <SPI.h>
#include <LoRa.h>

Rtc_Pcf8563 rtc;

volatile int alarm_flag = 0;
volatile int pin_mask;
volatile int cnt = 1;
volatile int stat;

String device_num;

void blink()
{
  alarm_flag = 1;
  //Status = 0;
}

ISR(PCINT2_vect) {
  pin_mask = PIND & 0x04;
  if (cnt % 2) {
    switch (pin_mask) {
      case 0x00 : alarm_flag = 1; stat = 1; break;
      case 0x04 : break;
    }
    cnt++;
  }
  else{
    cnt++;
  }
}

void setup()
{
  pinMode(13,OUTPUT);
  pinMode(3, INPUT_PULLUP);           // set pin to input
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  Serial.begin(9600);
  Serial.println("start");
  
  for(int i = 14;i<19;i++){
    pinMode(i,INPUT_PULLUP);
  }
  
  pinMode(5,INPUT_PULLUP);
  pinMode(6,INPUT_PULLUP);

  device_num += digitalRead(5);
  device_num += digitalRead(6);

  device_num += ',';
  
  for(int i = 14;i<19;i++){
    device_num += (digitalRead(i) + '0');
  }

  device_num += ',';
  device_num += 0;
  rtc.initClock();
  rtc.setDate(14, 6, 3, 0, 10);
  rtc.setTime(1, 15, 50);
  rtc.setAlarm(16, 99, 99, 99);
  attachInterrupt(1, blink, FALLING);
  PCICR = 0x04;
  PCMSK2 = 0x10;
  SREG = 0x80;
  alarm_flag = 0;

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");
  
}

void loop()
{
  if (alarm_flag == 1) {
    clr_alarm();
  }
  enterSleep();
}

void enterSleep() {
  noInterrupts();                      
  attachInterrupt(1, blink, FALLING);
  Serial.println("Going to sleep!");    
  Serial.flush();                       
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void clr_alarm()
{
  Serial.print(rtc.formatTime());
  Serial.print("  ");
  Serial.print(rtc.formatDate());
  Serial.print("  0x");
  Serial.print(rtc.getStatus2(), HEX);
  Serial.print("\r\n");
  //LoRa.beginPacket();
  //LoRa.print("haha");
  //LoRa.endPacket();
  rtc.initClock();
  rtc.setDate(14, 6, 3, 0, 10);
  rtc.setTime(1, 15, 50);
  rtc.setAlarm(16, 99, 99, 99);
  detachInterrupt(1);
  Serial.print("blink!\r\n");
  alarm_flag = 0;
  attachInterrupt(1, blink, FALLING);
  device_num[14] += stat + '0';
  stat = 0;
  Serial.println("stat");
  Serial.println("Lora");
  LoRa.beginPacket();
  Serial.println("a");
  LoRa.print(device_num);
  Serial.println("b");
  LoRa.endPacket();
  Serial.println("loranum");
  //Serial.println("LED?");
}
