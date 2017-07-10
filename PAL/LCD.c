#include "LCD.h"

#include <r_cg_serial.h>
#include <r_cg_timer.h>

t_backlighPol _polarity;   // Backlight polarity
uint8_t _Addr;             // I2C Address of the IO expander
uint8_t _backlightPinMask; // Backlight IO pin mask
uint8_t _backlightStsMask; // Backlight status mask
uint8_t _En = 0;               // LCD expander word for enable pin
uint8_t _Rw = 0;               // LCD expander word for R/W pin
uint8_t _Rs = 0;               // LCD expander word for Register Select pin
uint8_t _data_pins[4];     // LCD data lines
uint8_t _displayfunction;  // LCD_5x10DOTS or LCD_5x8DOTS, LCD_4BITMODE or 
                           // LCD_8BITMODE, LCD_1LINE or LCD_2LINE
uint8_t _displaycontrol;   // LCD base control command LCD on/off, blink, cursor
                           // all commands are "ored" to its contents.
uint8_t _displaymode;      // Text entry mode to the LCD
uint8_t _numlines;         // Number of lines of the LCD, initialized with begin()
uint8_t _cols;             // Number of columns in the LCD						  

void LCDSetBacklightPin(uint8_t, t_backlighPol);
void LCDSend(uint8_t, uint8_t);
void LCDWrite4bits(uint8_t, uint8_t);
void LCDPulseEnable (uint8_t);
void LCDCommand(uint8_t);
void LCDDisplay(void);

void LCDSetBacklightPin(uint8_t value, t_backlighPol pol)
{
   _backlightPinMask = ( 1 << value );
   _polarity = pol;
}

void LCDSetBacklight(uint8_t value) 
{
	uint8_t * const tx_buf = &_backlightStsMask;
	
   if (_backlightPinMask != 0x0)
   {
      if  (((_polarity == POSITIVE) && (value > 0)) || 
           ((_polarity == NEGATIVE ) && ( value == 0 )))
      {
         _backlightStsMask = _backlightPinMask & 0xFF;
      }
      else 
      {
         _backlightStsMask = _backlightPinMask & 0x00;
      }
	  R_IICA0_Master_Send(LCD_ADDRESS, tx_buf, 1, 100);
   }
}

void LCDInit(uint8_t cols, uint8_t lines, uint8_t dotsize)
{
	LCDSetBacklightPin(3, POSITIVE);
	
	_displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
	_Addr = LCD_ADDRESS;

	_En = ( 1 << 2 );
	_Rw = ( 1 << 1 );
	_Rs = ( 1 << 0 );

	// Initialise pin mapping
	_data_pins[0] = ( 1 << 4 );
	_data_pins[1] = ( 1 << 5 );
	_data_pins[2] = ( 1 << 6 );
	_data_pins[3] = ( 1 << 7 );	
	
	if (lines > 1) 
	{
	  _displayfunction |= LCD_2LINE;
	}
	_numlines = lines;
	_cols = cols;

	// for some 1 line displays you can select a 10 pixel high font
	// ------------------------------------------------------------
	if((dotsize != LCD_5x8DOTS) && (lines == 1)) 
	{
	  _displayfunction |= LCD_5x10DOTS;
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way before 4.5V so we'll wait 
	// 50
	// ---------------------------------------------------------------------------
	delay_ms(100); // 100ms delay

	//put the LCD into 4 bit or 8 bit mode
	// -------------------------------------
	if(!(_displayfunction & LCD_8BITMODE)) 
	{
	  // this is according to the hitachi HD44780 datasheet
	  // figure 24, pg 46
	  
	  // we start in 8bit mode, try to set 4 bit mode
	  LCDSend(0x03, FOUR_BITS);
	  delay_ms(5); // wait min 4.1ms
	  
	  // second try
	  LCDSend(0x03, FOUR_BITS);
	  delay_ms(5); // wait min 4.1ms
	  
	  // third go!
	  LCDSend(0x03, FOUR_BITS);
	  delay_ms(1);
	  
	  // finally, set to 4-bit interface
	  LCDSend(0x02, FOUR_BITS); 
	} 
	else 
	{
		// this is according to the hitachi HD44780 datasheet
		// page 45 figure 23

		// Send function set command sequence
		LCDCommand(LCD_FUNCTIONSET | _displayfunction);
		delay_ms(5);  // wait more than 4.1ms

		// second try
		LCDCommand(LCD_FUNCTIONSET | _displayfunction);
		delay_ms(1);

		// third go
		LCDCommand(LCD_FUNCTIONSET | _displayfunction);
	}

	// finally, set # lines, font size, etc.
	LCDCommand(LCD_FUNCTIONSET | _displayfunction);  

	// turn the display on with no cursor or blinking default
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;  
	LCDDisplay();

	// clear the LCD
	LCDClear();

	// Initialize to default text direction (for romance languages)
	_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	LCDCommand(LCD_ENTRYMODESET | _displaymode);

	LCDSetBacklight(1);
}

void LCDSend(uint8_t value, uint8_t mode) 
{
   if(mode == FOUR_BITS)
   {
      LCDWrite4bits((value & 0x0F), COMMAND);
   }
   else 
   {
      LCDWrite4bits((value >> 4), mode);
      LCDWrite4bits((value & 0x0F), mode);
   }
}

void LCDWrite4bits(uint8_t value, uint8_t mode) 
{
   uint8_t pinMapValue = 0;
   uint8_t i = 0;
   
   for(i = 0; i < 4; i++)
   {
      if(( value & 0x1) == 1)
      {
         pinMapValue |= _data_pins[i];
      }
      value = (value >> 1);
   }

   if (mode == DATA)
   {
      mode = _Rs;
   }
   
   pinMapValue |= (mode | _backlightStsMask);
   LCDPulseEnable(pinMapValue);
}


void LCDPulseEnable (uint8_t data)
{
	uint8_t temp_data = 0;
	uint8_t * const tx_buf = &temp_data;
	
	temp_data = (data | _En);
	R_IICA0_Master_Send(LCD_ADDRESS, tx_buf, 1, 100);	
	delay_ms(3);
	
	temp_data = (data & ~_En); 
	R_IICA0_Master_Send(LCD_ADDRESS, tx_buf, 1, 100);
	delay_ms(3);
}

void LCDClear(void)
{
   LCDCommand(LCD_CLEARDISPLAY);             // clear display, set cursor position to zero
   delay_ms(HOME_CLEAR_EXEC);    // this command is time consuming
}

void LCDCommand(uint8_t value) 
{
   LCDSend(value, COMMAND);
}

void LCDDisplay(void) 
{
   _displaycontrol |= LCD_DISPLAYON;
   LCDCommand(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LCDWrite(uint8_t value)
{
	LCDSend(value, DATA);	
}

void LCDHome()
{
   LCDCommand(LCD_RETURNHOME);  // set cursor position to zero
   delay_ms(HOME_CLEAR_EXEC);   // This command is time consuming
}

void LCDSetCursor(uint8_t col, uint8_t row)
{
   const int row_offsetsDef[]   = {0x00, 0x40, 0x14, 0x54}; // For regular LCDs
   const int row_offsetsLarge[] = {0x00, 0x40, 0x10, 0x50}; // For 16x4 LCDs
   
   if(row >= _numlines) 
   {
      row = _numlines-1;    // rows start at 0
   }
   
   // 16x4 LCDs have special memory map layout
   // ----------------------------------------
   if(_cols == 16 && _numlines == 4)
   {
      LCDCommand(LCD_SETDDRAMADDR | (col + row_offsetsLarge[row]));
   }
   else 
   {
      LCDCommand(LCD_SETDDRAMADDR | (col + row_offsetsDef[row]));
   }
}