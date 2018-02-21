#include <msp430.h>
#include "stdint.h"
/**
 * main.c
 */

#define BUTTON_DEBOUNCE_MS  250 // How many ms to pass between button presses

#define WIPER_SLOW_PIN  BIT2    // Wiper slow motor winding P2.2
#define WIPER_FAST_PIN  BIT1    // Wiper fast motor winding P2.1
#define WIPER_ZERO_PIN  BIT0    // Zero crossing pin        P1.0
#define WIPER_HALF_PIN  BIT1    // Half point pin           P1.1

#define ON_OFF_PIN      BIT0    // On Off button pin        P2.0
#define STATUS_LED_PIN  BIT3    // Status LED in on off button P2.3
#define TOGGLE_1_PIN    BIT5    // Toggle sw pin 1          P1.5
#define TOGGLE_2_PIN    BIT4    // Toggle sw pin 2          P1.4

// 8-position switch for interval mode
#define POS_SW_1_PIN    0x00    // P
#define POS_SW_2_PIN    0x00    // P
#define POS_SW_3_PIN    0x00    // P
#define POS_SW_4_PIN    0x00    // P
#define POS_SW_5_PIN    0x00    // P
#define POS_SW_6_PIN    0x00    // P
#define POS_SW_7_PIN    0x00    // P
#define POS_SW_8_PIN    0x00    // P

uint8_t get_interval();
void get_wiper_mode();
void wiper_fast();
void wiper_slow();
void wiper_off();

// Wiper intervals in seconds
uint8_t INTERVAL_SECONDS[8] = {3, 6, 9, 12, 15, 18, 21, 24};

volatile uint16_t millis;
volatile uint8_t interval_seconds_left;

enum{
    INTERVAL,
    SLOW,
    FAST
} WIPER_MODE = SLOW;

enum{
    OFF,
    ON
} DEVICE_STATE = OFF;


int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    // Clock to 1 MHz
    DCOCTL = CALDCO_1MHZ;
    BCSCTL1 = CALBC1_1MHZ;
    BCSCTL2 |= DIVS_1;      // SMCLK to 500 kHz

    // Need to initialize vars on MSP430 manually
    millis = 0;

    P1DIR = 0x00;
    P2DIR = 0x00;

    P2OUT &= ~(STATUS_LED_PIN + WIPER_FAST_PIN + WIPER_SLOW_PIN);
    P2DIR |= STATUS_LED_PIN + WIPER_FAST_PIN + WIPER_SLOW_PIN;

    P1REN |= 0xFF;
    P2REN |= 0xFF - (STATUS_LED_PIN + WIPER_FAST_PIN + WIPER_SLOW_PIN);

    P1OUT |= 0xFF;
    P2OUT |= 0xFF - STATUS_LED_PIN;

    P2IE  |= ON_OFF_PIN;
    P2IFG &= ~ON_OFF_PIN;

    P1IE  |= WIPER_HALF_PIN + WIPER_ZERO_PIN;
    P1IFG &= ~(WIPER_HALF_PIN + WIPER_ZERO_PIN);

    // Set up timer for wiper interval mode
    TA0CCR0 = 62500;   // TCLK = SMCLK/8 = 62.5kHz. Interrupt every 1 second.
    TA0CCR1 = 125;     // Interrupt every 2ms
    TA0CCTL0 = CCIE;
    TA0CCTL1 = CCIE;
    TA0CTL = TASSEL_2 + ID_3 + MC_1;

    __nop();
    __eint();
    // Main Loop
    while(1){
        if (DEVICE_STATE == ON){
            get_wiper_mode();
            switch (WIPER_MODE){
                case INTERVAL:
                    if (interval_seconds_left == 0){
                        interval_seconds_left = get_interval();
                        wiper_slow();
                    }
                    break;
                case SLOW:
                    wiper_slow();
                    break;
                case FAST:
                    wiper_fast();
                    break;
                default:
                    DEVICE_STATE = SLOW;
            }
        }
    }
    return 0;
}


uint8_t get_interval(){
    // Read rotary switch and return the current interval in seconds
    uint8_t interval = INTERVAL_SECONDS[7];

    return interval;
}


void get_wiper_mode(){
    WIPER_MODE = SLOW;
}


void wiper_fast(){
    wiper_off();
    P2OUT |= WIPER_FAST_PIN;
}


void wiper_slow(){
    wiper_off();
    P2OUT |= WIPER_SLOW_PIN;
}


void wiper_off(){
    P2OUT &= ~(WIPER_FAST_PIN + WIPER_SLOW_PIN);
}


__attribute__((interrupt(PORT1_VECTOR)))
void port_1_handler(void)
{
    if (P1IFG && WIPER_ZERO_PIN){
        if (DEVICE_STATE == OFF){
            wiper_off();
        }
        else if (WIPER_MODE == INTERVAL){
            wiper_off();
        }

        P1IFG &= ~WIPER_ZERO_PIN;
    }

    if (P1IFG && WIPER_HALF_PIN){
        // Nothing here currently..
        P1IFG &= ~WIPER_HALF_PIN;
    }
}

__attribute__((interrupt(PORT2_VECTOR)))
void port_2_handler(void)
{
    static uint16_t last_press = 0;
    if (P2IFG && ON_OFF_PIN){
        P2IFG &= ~ON_OFF_PIN;

        if ((millis - last_press) > BUTTON_DEBOUNCE_MS){
            last_press = millis;

            if (DEVICE_STATE == OFF){
                DEVICE_STATE = ON;
                P2OUT |= STATUS_LED_PIN;
            }
            else{
                DEVICE_STATE = OFF;
                P2OUT &= ~STATUS_LED_PIN;
            }
        }
    }
}

__attribute__((interrupt(TIMER0_A0_VECTOR)))
void timer_handler_1(void)
{
    // CCR0 Interrupt (1 second interrupt)
    if (interval_seconds_left != 0){
        interval_seconds_left--;
    }
}

__attribute__((interrupt(TIMER0_A1_VECTOR)))
void timer_handler_2(void)
{
    // Everything else interrupt (2 ms interrupt)
    millis += 2;
}

// TODO: Check all 'if' statements
