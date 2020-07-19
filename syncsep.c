#include <xc.h>
#include <pic.h>
#include <stdint.h>

/*
 * SyncSep.c
 * Joshua Reich / josh@i2pi.com / @i2pi
 * July 2020.
 * 
 * This program is designed to run on the low-cost 8-pin, 8-bit, 8 MHz,
 * PIC12HV752 MCU. 
 * 
 * It takes in the Luma/Sync (green-cable) signal from a 1080p component video
 * signal, and outputs:
 * 
 *   - H Sync      -- HIGH for each incoming h-sync pulse 
 *   - V Sync      -- HIGH for the first v-sync pulse
 *   - Line Gate   -- HIGH for the duration of the visible analog video content
 * 
 * This is similar to the function of an LM1881, but works for 1080p component
 * video, instead of NTSC/PAL composite video. 
 * 
 * Pinout:
 * 
 *                 Vdd  1.    8  GND
 *                      2     7  COG1OUT1 / Line Gate Out
 *  C1IN1- / Signal IN  3     6  RA1 / V Sync Out
 *                      4     5  C1OUT / H Sync Out
 */


#pragma config FOSC0 = INT      // FOSC: Oscillator Selection bit (Internal oscillator mode.  I/O function on RA5/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (Watchdog Timer disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (Power-up Timer disabled)
#pragma config MCLRE = OFF      // MCLR/VPP Pin Function Select bit (MCLR pin is alternate function)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config BOREN = DIS      // Brown-out Reset Enable bits (BOR disabled)
#pragma config WRT = OFF        // Flash Program Memory Self Write Enable bit (Flash self-write protection off)
#pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function disabled.  CLKOUT pin acts as I/O)


/*
 * We use the DAC to generate a constant voltage that we compare against to
 * identify horizontal sync pulses. Using a 5V Vdd, each step in the DAC is
 * 156mV.
 */
#define DAC_SYNC_LEVEL 1

/*
 * We use the COG to generate a high level on pin 7 to indicate that we're in 
 * the portion of the signal that contains analog video. The COG is triggered by
 * the comparator, which is triggered by a horizontal sync pulse. The video
 * signal begins ~3uS after this pulse, so we delay the line start to compensate
 * for this.
 */
#define COG_LINE_START_DELAY 7

/*
 * The analog signal for each line takes about 26uS. We use the HLT timer to
 * trigger the auto-shutdown of the COG to drive pin 7 low. 
 */
#define HLT_LINE_DURATION 57

/*
 * Each frame includes a number of blank horizontal lines before and after the
 * actual video frame data. We only want pin 7 to be high during a line that
 * isn't blank. 
 */
#define MIN_LINE_COUNT 35
#define MAX_LINE_COUNT 1115

// For counting which line we're currently processing.
uint16_t line_count = 0;  

void main(void)
{
    // System clock of 8Mhz
    OSCCON=0b00110000; 
    
    // Enable output for RA1 / V Sync
    TRISA1 = 0;  
       
    // DAC: Enable, with Full Range, Disable Output, Referenced to Vdd
    DACCON0bits.DACEN = 1; // Enable DAC
    DACCON0bits.DACOE = 0; // No output
    DACCON0bits.DACPSS0 = 0; // Reference Vdd
    DACCON0bits.DACRNG = 1; // Full range
    DACCON1 = DAC_SYNC_LEVEL; 
    
    // Comparator 1
    TRISA2 = 0; // Allow comparator 1 to drive output
    TRISA4 = 1; // C1IN1- as Input.
    CM1CON0bits.C1ON = 1;   // Comparator 1 enabled
    CM1CON0bits.C1OUT = 1;  // Output non-inverted polarity
    CM1CON0bits.C1OE = 1;   // Output enabled (H Sync Out)
    CM1CON0bits.C1POL = 0;  // Not inverted
    CM1CON0bits.C1ZLF = 1;  // Enable zero latency filter
    CM1CON0bits.C1SP = 1;   // High speed mode
    CM1CON0bits.C1HYS = 1;  // Disable hysteresis
    CM1CON0bits.C1SYNC = 0; // Asynchronous from Timer1
    
    CM1CON1bits.C1INTN = 0; // No interrupt on Negative
    CM1CON1bits.C1INTP = 1; // Interrupt on Positive
    CM1CON1bits.C1PCH = 0b01; // Compare to DAC reference
    CM1CON1bits.C1NCH0 = 1;  // C1N1- 
    
    // Interrupts 
    INTCON = 0;
    INTCONbits.GIE = 1; // Enable global interrupts
    INTCONbits.PEIE = 1; // Enable peripheral interrupts
    PIE2bits.C1IE = 1;
    CM1CON1bits.C1INTP = 1;
    
    // HLT - for generating the timeout for the line gate pulse
    HLT1CON0bits.H1ON = 1; // Enable
    HLT1CON0bits.H1OUTPS = 0; // 1:1 post-scaler
    HLT1CON0bits.H1CKPS = 0; // 1:1 pre-scaler
    HLT1CON1bits.H1ERS = 1; // C1OUT Reset
    HLT1CON1bits.H1FEREN = 1; // Reset on falling edge 
    HLTPR1 = HLT_LINE_DURATION; // Counter value reset, triggering COG1 shutdown
    
    // COG
    TRISA0 = 0; 
    COG1CON0bits.G1EN = 1; // Enable COG1
    COG1CON0bits.G1OE1 = 1; // Enable COG1OUT1 output pin
    COG1CON0bits.G1CS1 = 0b10; // 8Mhz
    COG1CON1bits.G1FSIM = 0; // Falling source level sensitive
    COG1CON1bits.G1RSIM = 0; // Rising source level sensitive
    COG1CON1bits.G1RS = 0x000; // Rising Source = C1OUT
    COG1CON1bits.G1FS = 0x000; // Falling Source = C1OUT
    COG1DBbits.G1DBF = COG_LINE_START_DELAY; // Falling dead band 
    COG1ASDbits.G1ARSEN = 1; // Enable autorestart
    COG1ASDbits.G1ASDL1 = 0; // When shutdown, output 0
    COG1ASDbits.G1ASDSHLT = 1; // Shutdown when HLTMR = HLTPR
    
    while(1);
}

void __interrupt() isr(void) {
    if (PIR2bits.C1IF) {
        /* 
         * A sync pulse has happened. This isr has been called ~10us later,
         * and if the comparator output is also high right now, then we're
         * in the middle of a vertical sync pulse, so output that to RA1 and 
         * reset the line counter.
         */
        if (CMOUTbits.MCOUT1) {
            // Only output a pulse for the first in the train of 5
            if (line_count > 5) PORTAbits.RA1 = 1; 
            line_count = 0;
        } else {
            PORTAbits.RA1 = 0;
            line_count++;
        }
        
        PIR2bits.C1IF = 0;
        
         if ((line_count > MAX_LINE_COUNT) || (line_count < MIN_LINE_COUNT)) {
            /* 
             * Disable the COG responsible for outputting the Line Gate signal,
             * as we are outside of the viewable 1080 lines in the frame.
             */
            COG1CON0bits.G1EN = 0;
            COG1CON0bits.G1OE1 = 0;
            PORTAbits.RA0 = 0;
        } else {
            COG1CON0bits.G1EN = 1;
            COG1CON0bits.G1OE1 = 1;
        }
    }
}
