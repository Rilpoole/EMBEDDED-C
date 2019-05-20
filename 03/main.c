/* Lab <3> - <Riley Poole> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include "UART.h"
#include "DIO.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h> //I included

/* prototypes */
int putchar_lcd(int input);
char getkey(void);
void wait(void);

/* definitions */
#define wait_ent while(getkey()!=ENT); //wait for the enter key

/*
 * Name: Main
 * Purpose: Procedural code to test the funcitonality of getkey() and putchar_lcd()
 * Parameters: None
 * Returns: 	int - status of the MyRio_Open or Myrio_close
 * 				void - outputs of getkey() and putchar_lcd() to the console and the LCD
 */
int main(int argc, char **argv)
{
	NiFpga_Status status;
	char gotkey = EOF;
	const int stringlen = 80; //arbitrary maximum string length
	char string[stringlen];

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //make at least one individual call to each of putchar_lcd() and getkey()
    while(1){
		//testing putchar
		printf_lcd("\fTesting Putchar\nPress Enter");
		printf_lcd("\f");
		putchar_lcd('A');
		wait_ent
		putchar_lcd('\b'); //non destructive backspace
		wait_ent
		putchar_lcd('\n');
		wait_ent
		putchar_lcd('A');
		wait_ent
		putchar_lcd('\v');
		wait_ent
		putchar_lcd('\f');
		wait_ent
		putchar_lcd('A');
		wait_ent
		//be sure to test the value out of range
		putchar_lcd(256);
		printf_lcd("\fFinished Testing\nPutchar()\nPress Enter");
		wait_ent

		//testing getkey
		printf_lcd("\fTesting getkey()\n");
		gotkey = getkey();
		printf("%c\n",gotkey);
		printf_lcd("Got Key");
		wait_ent
		printf_lcd("\fFinished Testing:\ngetkey()\nPress Enter");

		//collect an entire string using fgets_keypad()
		printf_lcd("\fTesting fgets_keypad\n");
		fgets_keypad(string,stringlen);
		printf_lcd("%\fThe string is\n%s",string);
		wait_ent
		printf_lcd("\fFinished Testing:\nfgets()\nPress Enter");
		wait_ent

		//write an entire string using printf_lcd()
		printf_lcd("\fThis is a test of f");
		wait_ent
		printf_lcd("\fThis is a test\v123\nof v");
		wait_ent
		printf_lcd("\fThis is a test\nof n");
		wait_ent
		printf_lcd("\fThis is a test\b \b\nof b"); //check this functionality
		wait_ent
		printf_lcd("\fEND");
		wait_ent
    }

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*
 *Name: putchar_lcd
 *Purpose:	Lowest level routine to place characters on the LCD screen
 *Inputs:	int - input character to be written to the LCD
 *Returns: 	int - the character written to the LCD
 *			int - EOF to signal an error
 */
int putchar_lcd(int input){
	static int putchar_initialization_flag = 0;
	static MyRio_Uart uart;
	if(putchar_initialization_flag == 0){
		/*
		 * initialize UART
		 */
		uart.name = "ASRL2::INSTR";
		uart.session = 0;
		int32_t status;
		status = Uart_Open(&uart,19200,8,Uart_StopBits1_0,Uart_ParityNone);
		if(status<VI_SUCCESS)
			return EOF;
		putchar_initialization_flag = 1;
	}

	/*
	 * write input character to lcd
	 * checking escape sequence
	 */
	uint8_t writeS[1]; //single element array to conform to Uart_Write()
	size_t nData = 1;
	if(input >=0 && input <= 255){ //allowable ascii characters
		switch(input){
		case '\f': //formfeed
			*writeS = 12;
			Uart_Write(&uart,writeS,nData);
			*writeS = 17;
			Uart_Write(&uart,writeS,nData);
			break;
		case '\b': //backspace
			*writeS = 8;
			Uart_Write(&uart,writeS,nData);
			break;
		case '\v': //return to start
			*writeS = 128;
			Uart_Write(&uart,writeS,nData);
			break;
		case '\n': //newline
			*writeS = 13;
			Uart_Write(&uart,writeS,nData);
			break;
		default:
			*writeS = input;
			Uart_Write(&uart,writeS,nData);
			break;
		}
		return input;
	} else //value must be out of range
		printf("Warning: \nInput Value Out of Range!\n");
		return EOF;
}

/*
 * Name:	getkey()
 * Purpose:	lowest level routine for getting a character from the keypad input
 * Inputs: 	none
 * Returns:	character input from the keypad
 */
char getkey(void){
	int low_bit_flag = 0;
	#define columns 4
	#define rows 4
	int bit;
	static MyRio_Dio Ch[8];
	static int getkey_initialization_flag = 0;
	int i = 0, j = 0;

	//row column table for channels
	int table[4][4] = { 	{'1','2','3', UP},
							{'4','5','6', DN},
							{'7','8','9',ENT},
							{'0','.','-',DEL} };

	//check this is the first time getkey() has been called
	if(getkey_initialization_flag == 0){
		//initialize the 8 digital chs
		for(i=0; i<8; i++){
			Ch[i].dir = DIOB_70DIR;
			Ch[i].out = DIOB_70OUT;
			Ch[i].in = DIOB_70IN;
			Ch[i].bit = i;
		}
		getkey_initialization_flag = 1;
	}
	MyRio_Dio* row[rows] = {&Ch[4],&Ch[5],&Ch[6],&Ch[7]};
	MyRio_Dio* column[columns] = {&Ch[0],&Ch[1],&Ch[2],&Ch[3]};

	//while low bit has not been detected search the table
	while(low_bit_flag ==0){
		for(i = 0; i<columns; i++){
			int setall = 0;
			for(setall = 0; setall<columns; setall++){
				bit = Dio_ReadBit(column[setall]);
			}
			Dio_WriteBit(column[i],NiFpga_False);
			for(j = 0; j<rows; j++){
				bit = Dio_ReadBit(row[j]);
				if(bit == NiFpga_False){
					low_bit_flag = 1;
					break;
				}
			}
			wait(); //make sure pushed down
			if(bit == NiFpga_False)
				break;
		}
	}
	while(Dio_ReadBit(row[j]) == NiFpga_False){
		//wait for pressed key to come back up
	}
	return table[j][i];
}

/*----------------------------------------
Function wait
Purpose: waits for xxx ms.
Parameters: none
Returns: none
*-----------------------------------------*/
void wait(void) {
	uint32_t i;
	i = 417000;
	while(i>0){
		i--;
	}
return;
}


