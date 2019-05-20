/* Lab #1 - <Riley Poole> */

/*includes*/
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <stdarg.h>

/* prototypes */
double	double_in(char *prompt);
int printf_lcd(char* format, ...);

/* definitions */
#define max_input 80 //maximum user input
#define lcd_num_chars 80 //maximum characters allowed on lcd screen
#define PROMPT "\fThis is the prompt\n"

/*
 * The main function
 * Purpose: To show the functionality of double_in()
 * Parameters: None
 * Returns: None
 */
int main(int argc, char **argv)
{
	NiFpga_Status status;
	status = MyRio_Open(); /* Open NiFpga.*/
	if (MyRio_IsNotSuccess(status)) return status;

	while(1)
	{ //run this program forever
		double ans1 = 0.0; //for storing the first input
		double ans2 = 0.0; //for storing the second input

		//prompt the user twice for a floating point value
		ans1 = double_in(PROMPT);
		printf("\n#1 %f\n",ans1);
		ans2 = double_in(PROMPT);
		printf("#2 %f\n",ans2);
		printf_lcd("\f");
	}
	status = MyRio_Close(); /* Close NiFpga. */ return status;
}

/*
 * Name: double_in()
 * Purpose: Prompt the user for a floating point value and return that value
 * Parameters: char* pointer to the prompt string
 * Returns: double, the value inputed by the users;
 * 			error on lcd screen if incorrect input
 */
double	double_in(char *prompt)
{
	double input_value = 0.0; //for storing the value incoming from the keypad
	char input_string[max_input]; //for storing the incoming string from the keypad

	//prompt
	printf_lcd("\f%s",prompt);

	//determine if error input
	if(fgets_keypad(input_string,max_input) == NULL)
	{
		printf_lcd("\fShort. Try Again.");
		while(getkey()!=ENT);
		return 0.0;
	}
	else if(strstr(input_string,"[") || strstr(input_string,"]") != NULL)
	{
		printf_lcd("\fUp&Down Not Valid");
		while(getkey()!=ENT);
		return 0.0;
	}
	else if(strpbrk((input_string+1),"-") != NULL){
		printf_lcd("\fIncorrect \"-\" \nPosition");
		while(getkey()!=ENT);
		return 0.0;
	}
	else if(strstr(input_string,"..") != NULL)
	{
		printf_lcd("\fToo Many \".\"");
		while(getkey()!=ENT);
		return 0.0;
	}
	else
	{
		sscanf(input_string,"%lf",&input_value);
		return input_value;
		while(getkey()!=ENT);
	}

}

/*
 * Name: printf_lcd()
 * Purpose: 	Print a formatted string to the lcd and prevent buffer overflow
 * Parameters: 	char* format; pointer to format string similar to regular print f;
 * 				...; list of values to be used in format string similar to regular print f
 * Returns:		int; the number of characters successfully writen to the screen or zero if error
 */
int printf_lcd(char* format, ...){
	int i; //index for buffer
	int n; // number of characters written to the buffer
	char buffer[lcd_num_chars];
	va_list args;

	va_start(args,format);

	//create a buffer
	n = vsnprintf(buffer,lcd_num_chars,format,args);

	//print the buffer to the lcd screen
	for(i = 0; i<n; i++){ //prevents overflow past lcd length
		putchar_lcd( *(buffer+i) );
	}

	va_end(args);

	return n;
}
