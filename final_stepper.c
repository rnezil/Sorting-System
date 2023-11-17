
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "lcd.h"

volatile char disk_location;	// Black, White, Steel, Aluminum
volatile int homed_flag = 0;	// Set to 1 once homing sensor is tripped and bucket is homed on black
const int qrtr_turn = 5;		// 90 degree turn
const int half_turn = 10;		// 180 degree turn
int position = 0;			// stepper position in stepper array(0-3)
int disk_direction = 0;		// 0 = clockwise 1 = counter clockwise
int items_sorted = 0;		// for testing only
int stepper[4] = {0b00000011, 0b00011000, 0b00000101, 0b00101000};	// Stepper positions

void mTimer(int count);
void home();
void move(int c);
void sort(char list_item);
void print_results();

int main(int argc, char* argv[]){
    CLKPR = 0x80;	// This will be discussed later
	CLKPR = 0x01;	// Required to set CPU CLock to 8MHz
	
	cli();
	/* Timer instructions*/
	/* Sets timer 1 to run at 1MHz, note:  CPU clock is set to 8MHz.
	   Disable all function and use as pure timer */
	
	TCCR1B|= _BV(CS11);	/* _BV sets the bit to logic 1
						   Note the register is TCCR1B1
						   TCCR1 is the Timer/counter control resister 1
						   B is the 'B' register and 1 is bit 1
						   CS mean clock select, has the pre-scaler set to 8*/
	
	/*******************************************************************/
	DDRC = 0xFF;	// Sets Port C for output(LCD or LEDs)
	DDRA = 0xFF;	// Sets Port A for output(stepper)
	DDRD = 0x00;	// Sets Port D for input
	PORTD = 0x00;	// Sets Port D pin 7 to active low(0)
	DDRL = 0xFF;	// Sets Port L for output
	
	//Initialize LCD module
	InitLCD(LS_BLINK|LS_ULINE);

	//Clear the screen
	LCDClear();
	
	// Enable external interrupt 2 in falling edge mode
	EIMSK |= _BV(INT2);
	EICRA = 0x02;
	
	// Exit uninterruptable command sequence
	sei();
	
	char list[] = {'a','b','s','w','w','a','s','b','w','s','b'};	// For testing only	
	LCDWriteStringXY(0,0,"Disk is homing");							// Can be removed after testing
	home();
	
    while (1){
		// This for loop will be replaced with iteration through a linked list
		for(int i = 0; i < 11; i++){
			sort(list[i]);
			LCDClear();
			print_results();
			mTimer(2000);
		}
		items_sorted = 0;
    }
	return(0);
}

void mTimer(int count){
	int i;	// Tracks loop number
	i = 0;	// Initializes loop counter
	
	// Set the Waveform Generation mode bit description to Clear Timer on Compare Math mode (CTC) only
	TCCR1B |= _BV(WGM12);	// set WGM bits to 0100, see page 145
	// Note WGM is spread over 2 registers
	
	OCR1A = 0X03E8;	// Sets Output Compare Register for 1000 cycles = 1ms
	TCNT1 = 0X0000;	// Sets initial values of Timer Counter to 0x0000
	// Clear interrupt flag and start timer
	TIFR1 |= _BV(OCF1A);
	
	
	// Poll the timer to determine when the timer has reached 0x03E8
	while(i<count){
		if((TIFR1 & 0x02) == 0x02){
			TIFR1 |= _BV(OCF1A);	// clear interrupt flag by writing a '1' to the bit
			i++;	// increments counter
		}	// End if
	}	// End while
	return;
}	// End mTimer

//This function rotates the stepper clockwise until the homing sensor triggers the INT2 interrupt
void home(){
	while (homed_flag != 1){
		PORTA = stepper[position];
		mTimer(20);
		position++;
		if(position == 4){
			position = 0;
		}
	}
	LCDWriteStringXY(0,0,"Disk homed Black");	// Can be removed after testing
	mTimer(1000);
}

//This function moves the stepper clockwise(0) or counter clockwise(1) 90 degrees or 180 degrees
void move(int c){
	if (disk_direction == 0){
		while (c > 0){
			PORTA = stepper[position];
			mTimer(20);
			position++;
			c--;
			if(position == 4){
				position = 0;
			}
			if(c == 0){
				break;
			}
		}
	}
	if (disk_direction == 1){
		while (c > 0){
			PORTA = stepper[position];
			mTimer(20);
			position--;
			c--;
			if(position < 0){
				position = 3;
			}
			if(c == 0){
				break;
			}
		}
	}
}

// This function moves the sorting bucket to a location based on part in list
void sort(char list_item)
{
	switch(disk_location)
	{
		case 'b':
			switch(list_item)
			{
				case 'b':
					break;
					
				case 'a':
					disk_direction = 0;
					move(qrtr_turn);
					disk_location = 'a';
					break;
					
				case 's':
					disk_direction = 1;
					move(qrtr_turn);
					disk_location = 's';
					break;
					
				case 'w':
					move(half_turn);
					disk_location = 'w';
					break;
					
				default:
					// exit with error code
					exit(1);
			}
			break;
			
		case 'a':
			switch(list_item)
			{
				case 'a':
					break;
					
				case 'b':
					disk_direction = 1;
					move(qrtr_turn);
					disk_location = 'b';
					break;
					
				case 's':
					move(half_turn);
					disk_location = 's';
					break;
					
				case 'w':
					disk_direction = 0;
					move(qrtr_turn);
					disk_location = 'w';
					break;
					
				default:
					// exit with error code
					exit(1);
			}
			break;
			
		case 'w':
			switch(list_item)
			{
				case 'w':
					break;
					
				case 'b':
					move(half_turn);
					disk_location = 'b';
					break;
					
				case 'a':
					disk_direction = 1;
					move(qrtr_turn);
					disk_location = 'a';
					break;
					
				case 's':
					disk_direction = 0;
					move(qrtr_turn);
					disk_location = 's';
					break;
					
				default:
					// exit with error code
					exit(1);
			}
			break;
				
		case 's':
			switch(list_item)
			{
				case 's':
					break;
					
				case 'b':
					disk_direction = 0;
					move(qrtr_turn);
					disk_location = 'b';
					break;
					
				case 'a':
					move(half_turn);
					disk_location = 'a';
					break;
					
				case 'w':
					disk_direction = 1;
					move(qrtr_turn);
					disk_location = 'w';
					break;
					
				default:
					// exit with error code
					exit(1);
			}
			break;
			
		default:
			// exit with error code
			exit(1);
	}
	
	items_sorted++;
	// dump item into bucket
}

// Function to test sorting of a list
void print_results(){
	if (disk_location == 'b'){
		LCDWriteStringXY(0, 0, "Disk on Black" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 'w'){
		LCDWriteStringXY(0, 0, "Disk on White" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 's'){
		LCDWriteStringXY(0, 0, "Disk on Steel" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 'a'){
		LCDWriteStringXY(0, 0, "Disk on Aluminum" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
}

//Stepper homing interrupt
ISR(INT2_vect){
	disk_location = 'b';
	homed_flag = 1;
	EIMSK = 0x00;	// Disables the INT2 interrupt
}