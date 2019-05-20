/* Lab #<4> - <Riley Poole> */

/*set the mode
 * 0 - printf command in speed
 * 1 - no printf command in speed
 */
#define mode 0

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include "DIO.h" //I included
#include <stdint.h>//I included
#include <math.h>//I included
#include "matlabfiles.h"

/* prototypes */
void initializeSM(void);
void high(void);
void low(void);
double vel(void);
void stop(void);
void speed(void);
void wait(void);
void request_N_and_M(void);
void full_speed(void);
void m1_state(void);
void error(void);

//global definitions for saving matlab code
#define IMAX 2400 //max points
double speed_buffer[IMAX]; //speed buffer
double M_matlab; //value of M to be saved in matlab
double N_matlab; //value of N to be saved in matlab
int speed_buffer_index; //array of the speeds returned by speed() and to be saved in matlab
NiFpga_Session myrio_session;
MyRio_Dio ch0, ch6, ch7;

/*other globals*/
MyRio_Encoder encC0;
typedef enum {STOP =0 , LOW , HIGH , SPEED, FULL_SPEED,M1_STATE,ERROR, EXIT} State_Type ; //exit must come last in this list
static void (*state_table[])(void) = {stop, low, high, speed,full_speed,m1_state,error};
static State_Type curr_state;
double wait_interval = 5e-3; //wait() is 5ms
int out;
int N;
int M;
int vel_flag = 0; //vel flag should be global so other functions can reset vel
uint32_t old_count=0, current_count;
double count_delta;
int N;
double BTI;
int Clock;
int full_speed_flag = 0; //full speed state initialized to off
int m1_state_flag;
int run_current;

/*
 * Function: 	main()
 * Purpose: 	test the state machine by setting various duty cycles
 * Parameters:	none
 * Returns: 	NiFpga_Status - status of myRIO open and close
 */
int main(int argc, char **argv)
{
	NiFpga_Status status;
	status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
	if (MyRio_IsNotSuccess(status)) return status;

	request_N_and_M();

	BTI = N*wait_interval;

	//setup all interface conditions and initialize the statemachine
	status = EncoderC_initialize(myrio_session,&encC0);
	initializeSM();

	//start the main transition loop
	while(curr_state != EXIT){
		state_table[curr_state]();
		wait();
		Clock++;
		if(Clock == N+2)
			curr_state = ERROR;
	}
	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*
 * Function: 	initializeSM
 * Purpose: 	set the control registers to an initialization state
 *				prevent the motor from running
 *				set the initial state to low 0
 *				reset the clock
 * Parameters:	none
 * Returns: 	none
 */
void initializeSM(void){
	//initialize channels 0 , 6, 7
	ch0.dir = DIOA_70DIR;
	ch0.out = DIOA_70OUT;
	ch0.in = DIOA_70IN;
	ch0.bit = 0;

	ch6.dir = DIOA_70DIR;
	ch6.out = DIOA_70OUT;
	ch6.in = DIOA_70IN;
	ch6.bit = 6;

	ch7.dir = DIOA_70DIR;
	ch7.out = DIOA_70OUT;
	ch7.in = DIOA_70IN;
	ch7.bit = 7;

	//initialize the encoder interface
	NiFpga_Status status;
	status = EncoderC_initialize(
		myrio_session, //should be globally defined
		&encC0);
	if(status != NiFpga_Status_Success){
		printf("STATUS ERROR: %d %s",__LINE__,__FILE__);
	}

	//stop the motor set run =1 i.e set channel 0 to 1
	Dio_WriteBit(&ch0,NiFpga_False);
	run_current = 0;

	//set the initial state to low
	curr_state = LOW; //this is defined globally

	//check the M = 1 condition
	if(M == 1)
		curr_state = M1_STATE;

	//check full speed condition MUST BE AFTER M = 1 CONDITION TO CATCH M = 1 N = 1
	if(M/N >=1)
		curr_state = FULL_SPEED;

	//set the clock to 0
	Clock = 0; //globally defined
}

/*
 * Function: 	high()
 * Purpose: 	sets the PWM signal high
 * Parameters:	none
 * Returns: 	none
 */
void high(void){

	//read channel 6 and 7
	static NiFpga_Bool ch6_bit,ch7_bit;
	ch6_bit = Dio_ReadBit(&ch6);
	ch7_bit = Dio_ReadBit(&ch7);

	if(Clock == N){ //Clock and N are globally defined
		Clock = 0;
		Dio_WriteBit(&ch0,NiFpga_False);//set run to 0;
		run_current = 0;

		if(ch7_bit == NiFpga_False){
			curr_state = SPEED;
		}
		else if(ch6_bit == NiFpga_False){
			curr_state = STOP;
		}
		else{
			curr_state = LOW;
		}
	}
}

/*
 * Function: 	low()
 * Purpose: 	sets the PWM signal low
 * Parameters:	none
 * Returns: 	none
 */
void low(void){
	if(Clock == M){ //globally defined
		//set run = 1
		Dio_WriteBit(&ch0,NiFpga_True);
		run_current = 1;
		curr_state = HIGH;
	}
}


/*
 * Function: 	stop()
 * Purpose: 	stop the motor
 * 				set next state to "EXIT" leaving the while loop
 * Parameters:	none
 * Returns: 	none
 */
void stop(void){

	//stop the motor
	Dio_WriteBit(&ch0,NiFpga_True); //set run  = 1
	run_current = 1;

	//clear the LCD and print the message "stopping"
	printf_lcd("\fSTOPPING...");

	//set the current state to exit
	curr_state = EXIT;//curr_state and EXIT globally defined

	//save the responses to matlab file
		//open a .mat file
		static int err;
		static MATFILE* mf;
		mf = openmatfile("Lab.mat", &err);
		if(!mf) printf("Can't open mat file %d\n", err);

		//add a matrix
		matfile_addmatrix(
			mf,
			"Speed",
			speed_buffer,
			1, //speed is only a 1 row array
			IMAX,
			0);

		//save the variables m and n
		M_matlab = (double) M;
		N_matlab = (double) N;
		matfile_addmatrix(mf,"M",&M_matlab,1,1,0);
		matfile_addmatrix(mf,"N",&N_matlab,1,1,0);

		//add a string with you name
		matfile_addstring(
			mf,
			"Name",
			"Riley Poole");

		matfile_close(mf);

		//wait for one second before stopping
		int i;
		for(i=0;i<200;i++)
			wait();
}

/*
 * Function: 	request_N_and_M
 * Purpose: 	Get the period and time on from the user, calculate and display the duty cycle
 * Parameters:	none
 * Returns: 	none
 */
void request_N_and_M(void){

	//request from the user, the number N of wait intervals in each BTI
	char* N_prompt = "\fPeriod (N):";
	printf_lcd(N_prompt);
	int buff_len = 10;
	char buff[buff_len];
	fgets_keypad(buff,buff_len);
	sscanf(buff,"%d",&N);

	//request the number of M intervals that the motor is on in each BTI
	printf_lcd("\nOn Time (M):");
	fgets_keypad(buff,buff_len);
	sscanf(buff,"%d",&M);

	//give the user feedback
	double duty_cycle;
	duty_cycle = ((double)M)/((double)N)*100; //treat M and N as double
	printf_lcd("\nDuty Cycle:%.0f%%",duty_cycle);
}

/*
 * Function: 	speed()
 * Purpose: 	Calls vel() to read speed then converts to RPM
 * 				Prints current speed to LCD screen
 * 				Saves the results to matlab
 * Parameters:	none
 * Returns: 	none
 */
void speed(void){
	double rpm;

	//call vel to read the encoder
	rpm = vel();

	if(mode == 0)
		printf_lcd("\fspeed %g rpm",rpm);

	//save for matlab stop when reaching the maximum number of values
	static int i = 0;
	if(i<=IMAX){
		*(speed_buffer+i) = rpm;
		i++;
	}

	if(full_speed_flag == 1)
		curr_state = FULL_SPEED;
	else if(m1_state_flag == 1)
		curr_state = M1_STATE;
	else{
		curr_state = LOW;
	}
}

/*
 * Function: 	vel()
 * Purpose: 	read the encoder and calculate the speed in BDI/BTI
 * Parameters:	none
 * Returns: 	none
 */
double vel(void){
	double velocity;
	double rpm;
	static int vel_flag = 0;
	static int current_count,count_delta,old_count;
	if(vel_flag == 1){//check is not the first call
		//read the current encoder count
		current_count = Encoder_Counter(&encC0);

		//compute the speed as the difference between the current and previous counts
		count_delta = current_count - old_count;
		velocity = count_delta;
	}
	if(vel_flag == 0)
		old_count = current_count = Encoder_Counter(&encC0);

	old_count = current_count;
	vel_flag = 1;

	//convert the units
	rpm = velocity * 1./2000.* 1/BTI * 60.;

	return rpm; //returns velocity in terms of BDI / BTI

}


/*----------------------------------------
Function wait
Purpose: waits for ??? ms.
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

void full_speed(void){
	static int i = 0;

	full_speed_flag = 1;//assert full_speed ahead

	//read channel 6 and 7
	static NiFpga_Bool ch6_bit,ch7_bit;
	ch6_bit = Dio_ReadBit(&ch6);
	ch7_bit = Dio_ReadBit(&ch7);

	Dio_WriteBit(&ch0,NiFpga_False); //set run  = 0
	run_current = 0;

	if(ch6_bit == NiFpga_False)
		curr_state = STOP;
	if(ch7_bit == NiFpga_False && i == N) //i = N only run speed once per BTI
		curr_state = SPEED;
	i++;
	if(i > N)
		i = 0;
	Clock = 0; //in case ever left full_speed to prevent runaway
}

void m1_state(void){
	//assert that m1_state has been called
	m1_state_flag = 1;

	//read channel 6 and 7
	static NiFpga_Bool ch6_bit,ch7_bit;
	ch6_bit = Dio_ReadBit(&ch6);
	ch7_bit = Dio_ReadBit(&ch7);

	//for every N passes do one low state
	static int i = 0;
	if(i == 0){
		Dio_WriteBit(&ch0,NiFpga_False); //set current on
		run_current = 0;
	}
	else{
		Dio_WriteBit(&ch0,NiFpga_True); //set current off
		run_current = 1;
	}
	i++;
	if(i>N) //reset i
		i = 0;

	//put these at the end to make sure they have higher priority
	if(ch6_bit == NiFpga_False)
		curr_state = STOP;
	if(ch7_bit == NiFpga_False && i == N) //only once per cycle
		curr_state = SPEED;

	Clock = 0; //just in case M1 is left by accident there won't be an over run condition
}

void error(void){
	printf("RUNNAWAY!\nSHUTTING DOWN...\n");
	Dio_WriteBit(&ch0,NiFpga_True); //set current off
	run_current = 1;
	curr_state = STOP;
}




