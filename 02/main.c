/* Lab #<2> - <Riley Poole> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"

/* prototypes */
int getchar_keypad(void);
void wait_ent(void);
char *fgets_keypad(char *buf, int buflen);

/* definitions */
#define bufflen 80//maximum number of characters in LCD is also the buffer length
#define test_getchar 0 //set to one if you want to debug getchar_keypad separately

/*
 * Name:		main()
 * Purpose: 	Prompt the user for two input strings entered on the keypad
 * Paramters: 	None
 * Returns: 	int - status which is an NiFpga_Status type which is an int32_t
 */
int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open(); /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //declare local variables
	char buff1[bufflen]; //buffer1 stores the first string
	char buff2[bufflen]; //buffer2 stores the second string

	//clear the screen and ask for two input strings
	while(1)//run this code forever
	{
		if(test_getchar == 0){

		//input string one
		printf_lcd("\fInput 1\n");
		fgets_keypad(buff1,bufflen);

		//input string two
		printf_lcd("\fInput 2\n");
		fgets_keypad(buff2,bufflen);

		//print the results to console
		printf("%s %s\n",buff1,buff2); //stdout is buffered needs
		wait_ent();

		}
		else if(test_getchar == 1){
			char a,b;
			printf_lcd("\fTest Getchar\n");
			a = getchar_keypad();
			b = getchar_keypad();
			printf("%c %c\n",a,b);
		}
	}

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */

	return status;
}

/*
 *Name: 		Getchar_keypad
 *Purpose:		Function that recieves a character from the keypad and returns that character to the program
 *Parameters: 	None
 *Returns: 		int - character from keypad;
 */
int getchar_keypad(void){
	static int n = 0; //number of characters in the buffer
	static char buff[bufflen]; //buffer to hold the characters from the keypad
	static char *pbuff; //pointer to the buffer position
	static char gotchar; //incoming character

	//if there is nothing in the buffer when getchar_keypad is called
	if(n == 0){
		pbuff = buff;
		//while the input key is not ENT gotchar = current key in
		while((gotchar = getkey())!=ENT){
			//if there is space in the buffer add the character to the buffer and display it on the screen
			if(n<bufflen){
				switch(gotchar){
					case DEL:
						if(n!=0){
						pbuff--;
						n--;
						putchar_lcd('\b');
						putchar_lcd(' ');
						putchar_lcd('\b');
						}
						//can only delete if there is something in the buffer
						if(n==0){
					    //nothing happens
						}
						break;
					//add character to the buffer and display the character
					default:
						*pbuff = gotchar;
						pbuff++;
						n++;
						putchar_lcd(gotchar);
						*pbuff = EOF; //set last entry to EOF
						break;
				}
			}
		}
		//enter key has been pressed exits the while loop
		n++;
		pbuff = buff;
	}//if there is something int he buffer when the function is called
	if(n>1){
		n--;
		return(*(pbuff++));
	}
	//nothing is in the buffer when the function is called
	if (n==1){
		n--;
		return EOF;
	}
	return 256;
}

/*
 *Name: 		Wait for Enter Key
 *Purpose:		Pauses the Program and Waits for Enter Key
 *Parameters: 	None
 *Returns: 		None
 */
void wait_ent(void){
	printf_lcd("\fPress ENT");
	while(getkey() != ENT);
}

/*
 * fgets_keypad.c
 *
 *  Created on: Sep 21, 2015
 *      Author: garbini
 *  	Created  for ME 477
 * 		 Limited to the only character stream available on the
 * 		 ME 477 stations:  the keypad. (stdin)
 */
#include <stdio.h>
#include "me477.h"

char *fgets_keypad(char *buf, int buflen)
{
    char *bufend;
    char *p;
    int c;

    p = buf;
    bufend = buf + buflen - 1;
    while (p < bufend) {
        c = getchar_keypad();		/* replace with get */
        if (c == EOF)
        	break;
        *p++ = c;
    }
	if(p == buf) return NULL;
    *p = '\0';
    return buf;
}




