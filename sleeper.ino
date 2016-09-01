/*
 * Sleeper is een superkleine sketch om de power van een Raspberry Pi af en aan te schakelen.
 * Raspberry Pi heeft zelf geen low power sleep mode, vandaar.
 */

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/wdt.h>   //Needed for watchdog
 
const byte outputPin = 0;
const byte inputPin  = 1;

volatile int watchdog_counter;

// the setup function runs once when you press reset or power the board
void setup() 
{
  cancel_watchdog();
  // initialize digital pin 0 as an output and pin 1 as an input.
  pinMode(outputPin, OUTPUT);
  pinMode( inputPin, INPUT);
  //if the whole thing is just starting up after total power off, turn on the Pi
  digitalWrite(outputPin, HIGH);   // turn the PSU on
  delay(1000);//give the Pi some time to boot up and set its output pins to HIGH

  //Get power consumption down as much as possible. Zie ook blz 36 van de datasheet van de ATTINY85
  watchdog_counter = 0;
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);//deepest sleep mode
  ADCSRA = 0; //turn off internal ADC
  ACSR |= 0x80;//turn off AD comparator
  //verdere mogelijkheden: brown out detector en interne band gap reference?
  //sleep_enable();//ik wil dit toch altijd aan? aan de andere kant wordt aangeraden om dit alleen aan te zetten vlak voor je gaat slapen
}

// the loop function runs over and over again forever
void loop() 
{
    if (digitalRead(inputPin) != HIGH) 
    {
      digitalWrite(outputPin,  LOW);   // turn the PSU off
      deepsleep();              // using the SLEEP_MODE_PWR_DOWN mode and watchdogs
      digitalWrite(outputPin, HIGH);   // turn the PSU back on
    }
    delay(1000); //give the Pi some time to boot up and set its output pins to HIGH
}

//Zet de watchdog naar interrupt mode met een interval gegeven door het argument
// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms, 6=1000msec,7=2000ms, 8=4ms, 9=8s
void setup_watchdog(int timerPrescaler) {
  cli();//disable all interrupts
  if (timerPrescaler > 9 ) timerPrescaler = 9; //maximum value is 9

  byte bb = timerPrescaler & 7; 
  if (timerPrescaler > 7) bb |= (1<<5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF); //Clear the watch dog reset (because WDE is always set if WDRF is set, and if WDE and WDIE are set then interrupt once and then reset)
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int (_BV(bit) is een macro voor 1<<bit, dit zet WDIE bit in WDTCR)
  sei();//enable interrupts again
}

//interrupt handler that handles the watchdog timer timeout triggering
//de interrupt flag in WDTCR wordt automatisch gecleared als je de interrupt vector doet
//de global interrupt enable wordt gecleared als je deze routine begint en weer gezet als de routine eindigt
ISR(WDT_vect) {
  watchdog_counter++;
}

//eigen routine, want volgens mij cleart disable_watchdog namelijk alleen de WDE bit en niet ook de WDIE en dan blijft de WDT runnen (blz 45 datasheet)
void cancel_watchdog()
{
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable, nu heb je 4 clock cycles om wijzigingen aan WDTCR te doen.
  WDTCR = 0;//sets all the bits to zero
}

void deepsleep()
{
  //enable watchdog zodat je weer wakker kan worden na elke individuele slaap
  setup_watchdog(9); //Sets the watchdog to interrupt mode, interrupt (ongeveer) elke 8 seconden
  //enable sleep
  sleep_enable();
  //slaap een aantal keer;
    watchdog_counter = 0;//reset de watchdog counter voor de volgende keer
    while(watchdog_counter < 2)//de ISR hoogt watchdog_counter elke 8 seconden met eentje op
    {
      //sleep
      sleep_cpu();
      //na de sleep gaat het programma hier weer verder, als watchdog_counter nog niet hoog genoeg is
      //is deze loop dus super tight
    }
    
  //disable sleep
  sleep_disable();
  //disable watchdog 
  cancel_watchdog();
}

