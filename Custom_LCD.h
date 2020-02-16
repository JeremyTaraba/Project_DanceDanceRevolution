#ifndef __Custom_LCD_h__
#define __Custom_LCD_h__

void LCD_ClearScreen(void);
void LCD_init(void);
void LCD_WriteCommand (unsigned char Command);
void LCD_WriteData (unsigned char Data);
void LCD_DisplayString (unsigned char column, const unsigned char* string);
void LCD_Cursor (unsigned char column);
void LCD_Custom_Char (unsigned char loc, unsigned char *msg);
void delay_ms(int miliSec);
#endif

