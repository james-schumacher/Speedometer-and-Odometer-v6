/*
 * lcd_config.h
 *
 *  Created on: Nov 21, 2016
 *      Author: James
 *
 *  Source/Credit: https://github.com/kates/msp430-lcd/blob/master/lcd_config.h
 */

#ifndef LCD_CONFIG_H_
#define LCD_CONFIG_H_

/*
 * Define pins and ports here
 */

#define LCD_PORT P1OUT
#define LCD_DIR P1DIR

#define LCD_RS BIT1
#define LCD_EN BIT2

#define LCD_D4 BIT4
#define LCD_D5 BIT5
#define LCD_D6 BIT6
#define LCD_D7 BIT7
#define LCD_FCPU 8000000

#define LCD_COLUMNS 20
#define LCD_ROWS 4

// How many rows and colums?
// Default is 16x2

// #define LCD_COLUMNS 16
// #define LCD_ROWS 2

#endif /* LCD_CONFIG_H_ */
