#include <pic.h>
#include <limits.h>
#include "delay.h"

/** Contains the main program routines
 * 	\file main.c
 * 	\author Martin Bo Kristensen Grønholdt
 */

/** \mainpage SPDIF input selector
 *	\section Introduction
 *	This is the code documentation of an SPDIF input selector for a TDA1545 NONOS DAC,
 *	using the a PIC16F628A
 *	\author Martin Bo Kristensen Grønholdt
 *	\version 1.0
 *		First working version
 *	\version 1.01
 *		Fixed a bug preventing the selector to get out of automatic mode unless an input was found
 *	\version 1.02
 *		Made the search for a signal faster skipping everything after the first delay, if there is no lock
 *		from the CS8412
 *	\version 1.03
 *		Changed the autosearch function from allways starting with the first input, to the next
 *		from the CS8412
 */

__CONFIG(INTIO & BORDIS & LVPDIS & WDTDIS);

//#define	DEBUG				1

#define INVERT_RELAYS		1

#define STATE_INIT		0
#define STATE_SET_RELAY		1
#define STATE_AUTO_SEARCH	2
#define	STATE_BUTTON_PRESS	3
#define NEXT_STATE		state++ 
#define	RESET_STATE		state = 0

#define RELAYS				PORTA
#define	BUTTON				RB0
#define I2S_DATA			RB4
#define CS8412_LOCK			!RB5
#define I2S_RELAY			RB3

//Variable to hold the current state, of the state machine
unsigned char state = STATE_INIT;
//Keep record of the active relay
//Setting the active relay to 5 results in no active relays
unsigned char active_relay = 5;
//Save the active relay in eeprom also
eeprom unsigned char non_volatile_active_relay = 0;
//Set this variable to start signal autosearch
unsigned char signal_autosearch = 0;
//Number of pulses currently recieved
unsigned int signal_pulses = 0;
//Time keeping for delay routines
unsigned long milliseconds;

void init(void);
void set_relay(unsigned char nr);
void enable_autosearch(void);
void disable_autosearch(void);
void enable_pulse_count(void);
void next_input(void);
void delayMs(unsigned int ms);
void delayMs_noninteruptable(unsigned int ms);
void delayS(unsigned int s);

void main(void)
{
    unsigned int temp;

    while (1)
    {
        switch (state)
        {
            case STATE_INIT:
                init();
                NEXT_STATE;
                break;
            case STATE_SET_RELAY:
                set_relay(active_relay);
                if (state != STATE_BUTTON_PRESS)
                    NEXT_STATE;
                break;
            case STATE_AUTO_SEARCH:
                if (signal_autosearch)
                {
                    //Make sure the relays have settled before counting pulses
                    delayMs(100);

                    if (CS8412_LOCK)
                    {
                        enable_pulse_count();

                        delayMs(10);

                        if (signal_pulses > 10)
                        {
                            disable_autosearch();

                            //Turn on I2S input to the DAC
                            I2S_RELAY = 1;
                        }
                        else
                            next_input();
                    }
                    else
                        next_input();

                    //Make sure to get out, if the button has been pressed
                    if (state != STATE_BUTTON_PRESS)
                        state = STATE_SET_RELAY;
                }
                else
                {
                    if (state != STATE_BUTTON_PRESS)
                        NEXT_STATE;
                }
                break;
            case STATE_BUTTON_PRESS:
                if (BUTTON)
                {
                    //Turn off I2S input to the DAC
                    I2S_RELAY = 0;
                    temp = 0;

                    while (BUTTON)
                    {
                        //Wait 5ms for debounce, and timekeeping
                        //to make the button multifunctional
                        //A short activation makes the input channel skip to the next
                        //A long activation turn on the autosearch feature
                        delayMs_noninteruptable(5);

                        //Keep track of how long the button is pressed
                        if (temp < UINT_MAX)
                            temp++;
                    }

                    if (temp < 100)
                    {
                        //Short press, skip to next input

                        next_input();

                        disable_autosearch();

                        //Turn on I2S input to the DAC
                        I2S_RELAY = 1;

                        NEXT_STATE;
                    }
                    else
                        enable_autosearch();
                }
                else
                    NEXT_STATE;
                break;
            default:
                state = STATE_SET_RELAY;
                break;
        }
    }
}

void init(void)
{
    //Comparators off
    CMCON = 0x07;
    //5 first ports output
    TRISA = 0xE0;

    //Turn of all relays
    if (INVERT_RELAYS)
        RELAYS = 0b11111111;
    else
        RELAYS = 0b00000000;

    //All ports input
    TRISB = 0b11110111;

    //Timer 0 used for delays
    //Internal clock as source
    T0CS = 0;

    //Prescaler to the timer module
    PSA = 0;

    //Prescaler set to 1:4 triggering the interrupt 250000 times a second
    PS0 = 1;
    PS1 = 0;
    PS2 = 0;

    //Turn on Timer 0 interrupt
    T0IE = 1;

    //Turn on the external interrupt
    //INTE = 1;

    disable_autosearch();

    //Turn on global interrupts
    GIE = 1;

    //Turn on the input that wass active last time the uC was powered off
    active_relay = non_volatile_active_relay;

    if (INVERT_RELAYS)
        RELAYS = 0b11111111 ^ ((unsigned char) (1 << active_relay));
    else
        RELAYS = (1 << active_relay);

    delayMs_noninteruptable(1000);
    I2S_RELAY = 1;
}

void set_relay(unsigned char nr)
{
    if (nr != non_volatile_active_relay)
    {
        non_volatile_active_relay = nr;

        if (INVERT_RELAYS)
            RELAYS = 0b11111111 ^ ((unsigned char) (1 << nr));
        else
            RELAYS = (1 << nr);

        //Rest the pulse count
        signal_pulses = 0;
    }
}

void enable_autosearch(void)
{
    signal_autosearch = 1;

    //Start with the next input
    next_input();

    if (state != STATE_BUTTON_PRESS)
        state = STATE_SET_RELAY;
}

void disable_autosearch(void)
{
    //Turn of interrupt on change on port B
    RBIE = 0;

    signal_autosearch = 0;
}

void enable_pulse_count(void)
{
    unsigned char temp;

    //Port read before clearing RBIF
    temp = PORTB;
    //Clear interrupt flag
    RBIF = 0;

    //Enable interrupt on change on port B
    RBIE = 1;

    //Reset the pulse count
    signal_pulses = 0;
}

void next_input(void)
{
    //Skip to next input, wrapping arountd to the first,
    //if the last is reached
    if (active_relay >= 4)
        active_relay = 0;
    else
        active_relay++;
}

void delayMs(unsigned int ms)
{
    milliseconds = 0;

    while ((milliseconds < ms) && (state != STATE_BUTTON_PRESS))
    {
    };
}

//Delay routine that doesn't care for button presses

void delayMs_noninteruptable(unsigned int ms)
{
    milliseconds = 0;

    while ((milliseconds < ms))
    {
    };
}

void delayS(unsigned int s)
{
    unsigned int i;

    for (i = 0; i < s; i++)
        delayMs(1000);
}

void interrupt ISR(void)
{
    unsigned char temp;
    unsigned char i2s_state;

    //If this is a Timer 0 interrupt, take care of the counter
    if (T0IF)
    {
        //Make the counter count 256 - 6 before next interrupt yeilding 1ms between
        //each interrupt
        TMR0 = 10;

        if (milliseconds < ULONG_MAX)
            milliseconds++;
        else
            milliseconds = 0;

        T0IF = 0;
    }
    //If this is an interrupt comming from RB0
    //the button has been pressed
    if (INTF)
    {
        state = STATE_BUTTON_PRESS;

        //Clear the interrupt flag
        INTF = 0;
    }
    if (RBIF)
    {
        if (I2S_DATA != i2s_state)
            if (signal_pulses > 10)
                RBIE = 0;
            else
                signal_pulses++;

        i2s_state = I2S_DATA;

        temp = PORTB;
        RBIF = 0;
    }
}
