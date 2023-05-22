#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include "LowPower.h"
#include <avr/sleep.h>
#include <SPI.h>
#include <LoRa.h>

/* get a real time clock object */
Rtc_Pcf8563 rtc;

/* a flag for the interrupt */
volatile int alarm_flag = 0;
volatile int pin_mask;
volatile int cnt = 1;
/* the interrupt service routine */
void blink()
{
  alarm_flag = 1;
}

ISR(PCINT2_vect) {
  pin_mask = PIND & 0x04;
  if (cnt % 2) {
    switch (pin_mask) {
      case 0x00 : alarm_flag = 1; break;
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

  /* setup int on pin 3 of arduino */
  /* clear out all the registers */
  rtc.initClock();
  /* set a time to start with.
     day, weekday, month, century, year */
  rtc.setDate(14, 6, 3, 0, 10);
  /* hr, min, sec */
  rtc.setTime(1, 15, 50);
  /* set an alarm for 20 secs later...
     alarm pin goes low when match occurs
     this triggers the interrupt routine
     min, hr, day, weekday
     99 = no alarm value to be set
  */
  rtc.setAlarm(16, 99, 99, 99);
  attachInterrupt(1, blink, FALLING);
  PCICR = 0x04;
  PCMSK2 = 0x10;
  SREG = 0x80;
  //attachInterrupt(0, blink, FALLING);
  alarm_flag = 0;

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop()
{
  /* each sec update the display */
  
  if (alarm_flag == 1) {
    clr_alarm();
  }
  enterSleep();
}

void enterSleep() {
  noInterrupts();                       // Disable interrupts
  attachInterrupt(1, blink, FALLING);
  //attachInterrupt(0, blink, FALLING);
  Serial.println("Going to sleep!");    // Print message to serial monitor
  Serial.flush();                       // Ensure all characters are sent to the serial monitor
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
  LoRa.beginPacket();
  LoRa.print("haha");
  LoRa.endPacket();
  rtc.initClock();
  /* set a time to start with.
     day, weekday, month, century, year */
  rtc.setDate(14, 6, 3, 0, 10);
  /* hr, min, sec */
  rtc.setTime(1, 15, 50);
  /* set an alarm for 20 secs later...
     alarm pin goes low when match occurs
     this triggers the interrupt routine
     min, hr, day, weekday
     99 = no alarm value to be set
  */
  rtc.setAlarm(16, 99, 99, 99);
  detachInterrupt(1);
  //detachInterrupt(0);
  Serial.print("blink!\r\n");
  //rtc.clearAlarm();
  //delay(1000);
  alarm_flag = 0;
  attachInterrupt(1, blink, FALLING);
  //attachInterrupt(0, blink, FALLING);
  LoRa.beginPacket();
  LoRa.print("haha");
  LoRa.endPacket();
  digitalWrite(13,HIGH);
  delay(500);
  digitalWrite(13,LOW);
}
