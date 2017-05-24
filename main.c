/*
 * main.c
 *
 *  Created on: Nov 21, 2016
 *      Author: James
 *
 */

/*
 * Includes
 */
#include <msp430.h>
#include <intrinsics.h>
#include <stdint.h>
#include <stdio.h>
#include "lcd.h"

/*
 * Definitions
 */
#define BUTTONS2 BIT3 // Push button S2 is P1.3
#define CIRCUMFERENCE 16 // Units: inches
#define CONVERT_SEC 1000 // Miliseconds to seconds

/*
 * Function Prototypes
 */
void measure_period(); // Measures the period of each revolution of the wheel in milliseconds
void calculate_speed(); // Calculates speed in inches/second
void print_lcd(int speed, int distance, int max_speed); // Prints the current speed, distance, and max speed to the LCD display

/*
 * Data Members (global)
 */
int period; // Used for holding current period of bike in milliseconds
int speed; // Used for holding current speed of bike in inches/second
int max_speed; // Used for holding max speed of bike in inches/second
int trip_distance; // Used for holding current trip distance of bike in inches
int enable; // Used to avoid the timer resetting while the magnet is still positioned over the hall-effect sensor
// Ensures the timer runs once when the ADC10MEM value hits its threshold voltage for the first time (it will hit
// this threshold voltage multiple times due to the ADC10 sampling rate)
// Active low

/*
 * Main Program
 */
void main(void)
{
	// Stop watchdog timer
	WDTCTL = WDTPW | WDTHOLD;

	// Set clock frequency for LCD display use
	DCOCTL = CALDCO_8MHZ;
	BCSCTL1 = CALBC1_8MHZ;

	// ACLK divide by 8 (4.096 kHz now)
	BCSCTL1 = BCSCTL1 | DIVA_3;

	// Setup ADC10
	P1SEL |= BIT0; // ADC input pin P1.0
	ADC10CTL1 = INCH_0 + ADC10DIV_3; // Channel 0, ADC10CLK/3, uses default ADC10 clock (fast)
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; // Vcc & Vss as reference, sample and hold for 64 clock cycles, ADC on, ADC interrupt enable
	ADC10AE0 |= BIT0; // ADC input enable pin P1.0

	// Initialize data members
	period = 0;
	speed = 0;
	max_speed = 0;
	trip_distance = 0;
	enable = 0;

	// Initialize the LCD display
	lcd_init();

	// Setup button S2
	P1DIR = P1DIR & ~BUTTONS2; // Input for button presses
	P1REN = P1REN | BUTTONS2; // Internal pull-up resistor enable
	P1IE = P1IE | BUTTONS2; // Enable interrupts for the button
	P1OUT = P1OUT | BUTTONS2; // Set BIT3 of Port 1 to a 1 by default

	// Enable global interrupts
	__bis_SR_register(GIE);

	// Sampling and conversion start
	ADC10CTL0 |= ENC + ADC10SC;

	// Program loop (using a while loop instead of labels doesn't allow interrupts, I found)
	// Continually calculate speed and print to the LCD display while the ADC10 is sampling and converting near constantly
	here: calculate_speed();
	print_lcd(speed, trip_distance, max_speed);
	goto here;
}

/*
 * Reset Trip Distance ISR
 * - Reset trip_distance, max_speed, and speed data member variables
 */
#pragma vector = PORT1_VECTOR
__interrupt void button_s2_isr(void)
{
	P1IFG = P1IFG & ~BUTTONS2; // Clear interrupt flag

	// Reset trip distance, max speed, and current speed
	trip_distance = 0;
	max_speed = 0;
	period = 0; // Reset period, and thus, speed will be reset
}

/*
 * Hall Effect ISR via ADC10
 * - Calls the measure_period function when the ADC10 reaches the appropriate threshold voltage
 * - enable data member variable ensures measure_period is called when appropriate
 */
#pragma vector = ADC10_VECTOR
__interrupt void adc_isr(void)
{
	if (ADC10MEM < 0x160 && enable == 0) // If within threshold voltage and enabled, measure period (magnet over hall-effect sensor)
	{
		enable = 1; // Disable the ability to call measure_period function
		measure_period(); // Start measuring the period of the revolution of the wheel
	}

	if (ADC10MEM >= 0x290 && enable == 1) // Position of the magnet is now out of range of the hall-effect sensor
	{
		enable = 0; // Enable the ability to measure period since the magnet is past the hall-effect sensor
	}

	// Ensure ADC10 is running as fast as possible
	ADC10CTL0 |= ENC + ADC10SC; // Sampling and conversion start
}

/*
 * measure_period()
 * - Overview: Determines the period between wheel rotations in milliseconds
 * - Details:
 * - Stops the timer and places the timer value into the period data member variable
 * - Resets the timer value, then restarts the timer to measure the number of milliseconds per rotation of the wheel
 * - Every call means there has been a rotation of the wheel and the trip_distance data member variable is incremented by the circumference of the wheel
 */
void measure_period()
{
	TACTL = TASSEL_1 + ID_2 + MC_0; // Stop Timer A, ACLK, divide by 4 (1.024 kHz now, period of ACLK is 1 ms), stop mode
	period = TAR; // Copy over the Timer A value into period
	TAR = 0; // Reset the TAR value for the next wheel cycle
	TACTL = TASSEL_1 + ID_2 + MC_2; // Start Timer A with ACLK,
									// divide by 4 (1.024 kHz), and
									// continuous mode

	// Increment trip distance
	trip_distance = trip_distance + 16;
}

/*
 * calculate_speed()
 * - Calculates the speed given the period and sets the speed data member variable
 * - Resets speed if 32 seconds have elapsed (0.49 in/s rounds to 0 in/s)
 * - Keeps track of the max speed
 * - Units: inches/second
 */
void calculate_speed()
{
	if (period == 0 || TAR > 32000) // If period is 0, speed has to be 0; if TAR value (current period) is greater than 32,000 ms, speed is less than 0.5 in/s and is rounded down to 0 in/s
	{
		speed = 0;

		if (TAR > 32000) // Once the Timer A value is beyond 32 s, there is no point in the timer continuing, especially since it will reset to 0 and start counting again after reaching 65 s
		{
			TACTL = TASSEL_1 + ID_2 + MC_0; // Stop Timer A
		}
	}

	if (period > 0 && TAR <= 32000) // If period is greater than 0 and TAR value (current period) is less than 32,000 ms, a non-zero speed value may be calculated
	{
		speed = (CIRCUMFERENCE * CONVERT_SEC) / period; // (16 in * 1000 ms/s) / (x ms) = (16 in * 1000 ms/s) * (1/x ms) = 16,000/x in/s

		if (speed > max_speed) // Update max_speed data member variable if current speed is higher
		{
			max_speed = speed;
		}
	}
}

/*
 * print_lcd()
 * - Prints the current speed, trip distance, and max speed to the LCD display
 * - Units: Speed: inches/second; Distance: inches
 */
void print_lcd(int speed_arg, int distance_arg, int max_speed_arg)
{
	char speed_buffer[10]; // 10 characters is enough to hold the speed (up to 3 chars) and text
	sprintf(speed_buffer, "Speed: %d", speed_arg); // Decimal format
	lcd_go_line(1); // Go to line 1 of the LCD display
	lcd_write_ln(speed_buffer); // Erase the current line and write the content of the argument to the LCD display

	char distance_buffer[15]; // 15 characters is enough to hold the speed (up to 5 chars) and text. Also, for an actual bike, the distance units would switch to mi. and speed units to mph
	sprintf(distance_buffer, "Distance: %d", distance_arg);
	lcd_go_line(2);
	lcd_write_ln(distance_buffer);

	char max_speed_buffer[6]; // 6 characters is enough to hold the speed (up to 3 chars) and text
	sprintf(max_speed_buffer, "M: %d", max_speed_arg);
	lcd_go_line(3);
	lcd_write_ln(max_speed_buffer);
}
