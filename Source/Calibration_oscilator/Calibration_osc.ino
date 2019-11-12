/*
The following program uses Timer/Counter0 Comparator A to generate a 10KHz 50% load on pin PB4 for an ATtiny25/45/85. 
It generates 10KHz whether the ATtiny is running directly off the internal oscillator at 8Mhz 
or is running at 1MHz because the CKDIV8 fuse is set.
http://becomingmaker.com/tuning-attiny-oscillator/
*/
// Timer/Counter0 Compare Match A interrupt handler 
ISR (TIMER0_COMPA_vect) {
   PORTB ^= 1 << PINB4;        // Invert pin PB4
}

void setup() {
    OSCCAL -= 0;                // User calibration
    pinMode(4,OUTPUT);          // Set PB4 to output
    TCNT0 = 0;                  // Count up from 0
    TCCR0A = 2 << WGM00;        // CTC mode
    if (CLKPR == 3)             // If clock set to 1MHz
        TCCR0B = (1<<CS00);     // Set prescaler to /1 (1uS at 1Mhz)
    else                        // Otherwise clock set to 8MHz
        TCCR0B = (2<<CS00);     // Set prescaler to /8 (1uS at 8Mhz)
    GTCCR |= 1 << PSR0;         // Reset prescaler
    OCR0A = 49;                 // 49 + 1 = 50 microseconds (10KHz)
    TIFR = 1 << OCF0A;          // Clear output compare interrupt flag
    TIMSK |= 1 << OCIE0A;       // Enable output compare interrupt
}

void loop() {
  // put your main code here, to run repeatedly:

}
