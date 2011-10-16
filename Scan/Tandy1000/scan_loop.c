/* Copyright (C) 2011 by Jacob Alexander
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// ----- Includes -----

// AVR Includes
#include <avr/interrupt.h>

// Project Includes
#include <led.h>
#include <print.h>

// Local Includes
#include "scan_loop.h"



// ----- Defines -----

// Pinout Defines
#define CLK_READ  PIND
#define CLK_PORT PORTD
#define CLK_DDR   DDRD
#define CLK_PIN      1

#define DATA_READ  PIND
#define DATA_PORT PORTD
#define DATA_DDR   DDRD
#define DATA_PIN      0

#define INTR_PORT PORTD
#define INTR_DDR   DDRD
#define INTR_PIN      0



// ----- Macros -----
#define READ_CLK       CLK_READ &   (1 <<  CLK_PIN) ? 1 : 0
#define READ_DATA     DATA_READ &   (1 << DATA_PIN) ? 0 : 1

#define UNSET_INTR()  INTR_DDR &= ~(1 << INTR_PIN)
#define   SET_INTR()  INTR_DDR |=  (1 << INTR_PIN)



// ----- Variables -----

uint8_t KeyIndex_Array[KEYBOARD_SIZE + 1];

// Scan Code Retrieval Variables
uint8_t inputData    = 0xFF;
uint8_t packet_index = 0;



// ----- Functions -----

// Setup
inline void scan_setup()
{
	// Setup inputs
	CLK_DDR  &= ~(1 << CLK_PIN);
	DATA_DDR &= ~(1 << DATA_PIN);

	// Setup Pull-up's
	CLK_PORT  &= ~(1 << CLK_PIN);  // (CLK)
	DATA_PORT &= ~(1 << DATA_PIN); // (/DATA)

	// Setup Keyboard Interrupt
	INTR_DDR  &= ~(1 << INTR_PIN);
	INTR_PORT &= ~(1 << INTR_PIN);

	/* Interrupt Style (Not working fully)
	cli();
	// Setup interrupt on the CLK pin TODO Better defines
	EICRA |= 0x03; // Rising Edge Interrupt
	EIMSK |= (1 << INT0);

	// Setup interrupt on the DATA pin TODO Better defines
	EICRA |= 0x08; // Falling Edge Interrupt
	EIMSK |= (1 << INT1);
	sei();
	*/
}


// Main Detection Loop
inline uint8_t scan_loop()
{
	/*
	// Packet Read
	if ( packet_index == 8 )
	{
		// Disable Error LED, proper key found
		errorLED( 0 );

//#ifdef MAX_DEBUG
		// Crazy Debug (Read the Scan Code)
		char tmpStr[3];
		hexToStr_op( inputData, tmpStr, 2 );
		dPrintStrsNL( "Read Data: 0x", tmpStr );
//#endif
		// - Map the scan code to the index array -
		// If the 8th bit is high, remove the keypress, else, add the keypress
		// The lower 7 bits are the array index
		KeyIndex_Array[(inputData & 0x7F)] = (inputData & 0x80) ? 0x00 : 0x80;

		// Reset Containers
		packet_index = 0;
		inputData = 0xFF;
	}
	// Bad Packet
	else if ( packet_index > 8 )
	{
		// Signal Error
		errorLED( 1 );

		char tmpStr[3];
		int8ToStr( packet_index, tmpStr );
		erro_dPrint( "Big packet? Mismatched... ", tmpStr );

		packet_index = 0;
		inputData = 0xFF;
	}
	*/

	// Disable keyboard interrupt (does nothing if already off)
	UNSET_INTR();

	// Read the clock 8 times
	if ( READ_CLK )
	{
		// Mis-read packet, set back to 0
		if ( packet_index == -1 )
			packet_index = 0;

		// Append 1 bit of data
		inputData &= ~(READ_DATA << packet_index);
		packet_index++;

		// 8 Bits have been read
		if ( packet_index == 8 )
		{
			// Wait till clock edge falls
			while ( READ_CLK );

			// Sample both lines to make sure this is not a data value
			//  and definitely the end of packet data blip
			uint16_t badDataCounter = 0;
			while ( !( READ_DATA ) && !( READ_CLK ) )
					badDataCounter++;

			if ( badDataCounter < 25 )
			{
//#ifdef MAX_DEBUG
				// Crazy Debug (Read the Scan Code)
				char tmpStr[3];
				hexToStr_op( inputData, tmpStr, 2 );
				dbug_dPrint( "Read Data: 0x", tmpStr );
//#endif
				// - Map the scan code to the index array -
				// If the 8th bit is high, remove the keypress, else, add the keypress
				// The lower 7 bits are the array index
				KeyIndex_Array[(inputData & 0x7F)] = (inputData & 0x80) ? 0x00 : 0x80;
			}
			// Even though this is a mis-read packet, we still know what the value is
			else
			{
				// Signal Error
				errorLED( 1 );
				char tmpStr[3];
				hexToStr_op( inputData, tmpStr, 2 );
				erro_dPrint( "Bad packet? Mismatched... 0x", tmpStr );
			}

			// Reset Containers
			inputData = 0xFF;
			packet_index = 0;

			// Interrupt the keyboard, so we don't get packet pieces...
			SET_INTR();

			// Do not wait for next clock, let USB do it's thing (if desired)
			return packet_index;
		}

		// Wait till clock edge falls
		while ( READ_CLK );
	}

	// Interrupt keyboard if there is no pending packet
	SET_INTR();

	return packet_index;
}

// Detection interrupt, signalled by a clock pulse from CLK_PIN
ISR(INT0_vect)
{
	//cli(); // Disable Interrupts

	// Append 1 bit of data
	//inputData &= ~(READ_DATA << packet_index);
	packet_index++;

	//sei(); // Re-enable Interrupts
}

// Data Detected
ISR(INT1_vect)
{
	// Append 1 bit of data
	inputData &= ~(1 << packet_index);
	packet_index++;

	// Disable Clk Signal (Not needed if there's a data signal)
	EIFR |= (1 << INTF0);
}


