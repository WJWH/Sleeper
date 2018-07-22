
/*
 * Sleeper is a very small sketch to switch the power for a Raspberry Pi on and off.
 * Since a Raspberry Pi does not have a ultra low power sleep mode it can be necessary to physically
 * switch the power on and off.
 */

#include <avr/sleep.h> // Needed for sleep_mode
#include <avr/wdt.h>   // Needed for watchdog
 
const byte outputPin = 0;
const byte inputPin  = 1;
const int sleepperiods = 4*450; // About four hours, because 450*8sec = 3600 seconds = 1 hour. Alter this to modify the time between Pi wakeups.
const int bootupTime = 30000;   // Power on to indicator HIGH takes about 27-28 seconds
const int haltTime = 15000;     // Indicator LOW to activity LED switching off takes between 10.7 and 12.5 seconds

volatile int watchdog_counter;

// The setup function runs once when you press reset or power the board
void setup()
{
  cancel_watchdog();
  // Initialize digital pin 0 as an output and pin 1 as an input.
  pinMode(outputPin, OUTPUT);
  pinMode( inputPin, INPUT);
  // If the whole thing is just starting up after total power off, turn on the Pi
  digitalWrite(outputPin, HIGH); // Turn on the PSU
  delay(bootupTime);// Give the Pi some time to boot up

  // Get power consumption down as much as possible. See page 36 of the ATTINY85 datasheet.
  watchdog_counter = 0;
  set_sleep_mode (SLEEP_MODE_PWR_DOWN); // Deepest sleep mode available
  ADCSRA = 0;   // Turn off internal ADC
  ACSR |= 0x80; // Turn off AD comparator
  // Further possibilities for extreme power saving: we can disable the brown out detector and the interne band gap reference.
  // This can make the execution more unreliable in low power situations however, so we won't go that far in this sketch.
}

// The loop function runs over and over again forever
void loop()
{
    if (digitalRead(inputPin) != HIGH)
    {
      delay(haltTime);               // The pi takes quite some time to power down :|
      digitalWrite(outputPin,  LOW); // turn the PSU off
      deepsleep();                   // using the SLEEP_MODE_PWR_DOWN mode en watchdogs
      digitalWrite(outputPin, HIGH); // turn the PSU back on
      delay(bootupTime);             // Give the pi some time to boot up
    }
    delay(500); //sample about twice per second
}

// Sets the watchdog for interrupt mode with an interval given by the argument
// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms, 6=1000msec,7=2000ms, 8=4ms, 9=8s
void setup_watchdog(int timerPrescaler) {
  cli(); // Disable all interrupts
  if (timerPrescaler > 9 ) timerPrescaler = 9 // Correct incoming amount if necessary

  byte bb = timerPrescaler & 7;
  if (timerPrescaler > 7) bb |= (1<<5); // Set the special 5th bit if necessary

  // This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF);           // Clear the watch dog reset (because WDE is always set if WDRF is set, and if WDE and WDIE are set it'll interrupt once and then reset)
  WDTCR |= (1<<WDCE) | (1<<WDE); // Set WD_change enable, set WD enable
  WDTCR = bb;                    // Set new watchdog timeout value
  WDTCR |= _BV(WDIE);            // Set the interrupt enable, this will keep unit from resetting after each int (_BV(bit) is a macro voor 1<<bit, this will set the WDIE bit in WDTCR)
  sei();                         // Enable interrupts again
}

// Interrupt handler that handles the watchdog timer timeout triggering.
// The interrupt flag in WDTCR will be automatically cleared when accessing the interrupt vector.
// The global interrupt enable will be cleared when entering this routine begint and set again when the routine ends.
ISR(WDT_vect) {
  watchdog_counter++;
}

// Make our own, because disable_watchdog seems to just clear the WDE bit en leaves WDIE intact, so the WDT will stay running (page 45 datasheet)
void cancel_watchdog()
{
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable and WD enable, we now have a 4 clock cycle window to change WDTCR.
  WDTCR = 0;// Disable everything
}

void deepsleep()
{
  // Enable watchdog watchdog
  setup_watchdog(9); // Sets the watchdog to interrupt mode, interrupt will be issued approximately every 8 seconds
  // Enable sleep
  sleep_enable();
  // Sleep a few times;
    watchdog_counter = 0;// Reset the watchdog counter
    while(watchdog_counter < sleepperiods)// The ISR will increment watchdog_counter every 8 seconds
    {
      sleep_cpu();
      // After sleeping the program picks up here, so while watchdog_counter is smaller than sleepperiods this loop is super tight
    }
   
  // Disable sleep
  sleep_disable();
  // Disable watchdog
  cancel_watchdog();
}
