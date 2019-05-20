/* Lab #<7> - <Riley Poole> */

#define SATURATE(x,lo,hi) ( (x)<(lo) ? (lo) : (x)>(hi) ? (hi):(x) )
#define lo -7.5
#define hi 7.5

#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include "TimerIRQ.h" //I included
#include "pthread.h" //I included
#include "matlabfiles.h"//I included
#include "ctable2.h"//I included
#include "AIO.h"//I included
#include "DIO.h"//I included

typedef struct{
	NiFpga_IrqContext irqContext;
	table* a_table;
	NiFpga_Bool irqThreadRdy;
	table* atable;
} ThreadResource;
struct biquad{
double b0; double b1; double b2; //numerator
double a0; double a1; double a2; //denominator
double x0; double x1; double x2; //input
double y1; double y2;};
struct biquad myFilter;

void* Timer_Irq_Thread (void* resource);
double vel(void);
void setup_table(void);
void update_filter(void);
double cascade(
				double, //input
				struct biquad*, //biquad array
				int, //no.segments
				double, //min output
				double);
double rpm2rad(double rpm);
double bdi_bti2rpm(double bdi_bti);

#define table_num 6
table my_table[table_num];
char* Table_Title;
double* vref;
double* vact;
double* vdaout;
double* kp ;
double* ki ;
double* bti ;

NiFpga_Session myrio_session;
MyRio_Encoder encC0;
uint32_t irqAssert = 0;
int timeoutValue;
double timeoutValue_seconds;
double timeoutValue_ms;
MyRio_IrqTimer irqTimer0;
ThreadResource irqThread0;
pthread_t thread;
MyRio_Aio CI0, CO0;

/*matlab declarations*/
#define MATLAB_points 250
double MATLAB_velocity[MATLAB_points];
double* MATLAB_velocity_pointer = MATLAB_velocity;
double MATLAB_torque[MATLAB_points];
double* MATLAB_torque_pointer = MATLAB_torque;
double kvi = 0.41;
double kt = 0.11;
static int err;
MATFILE matfile;
static MATFILE* mf;
static double ref_cur;
static double ref_pre = 0;
static double MATLAB_ref_pre;

/*
 * Name: 		main()
 * Purpose:		Sets up threads and interrupts; ends the timer thread
 *				Initializes the encoder
 *				Runs the table editor
 * Parameters:	No inputs or parameters used
 * Return:		int 0 – for normal operation
 *				Procedural
 *					table on the LCD screen
 *					control of timer thread
 */
int main(int argc, char **argv){
	int timeoutValue = 5;

	//open tht MyRio
	NiFpga_Status status;
    status = MyRio_Open();
    if (MyRio_IsNotSuccess(status)) return status;

    //set up convenient names for the table and initialize the values
	setup_table();

	//register the timer
	irqTimer0.timerWrite = IRQTIMERWRITE;
	irqTimer0.timerSet = IRQTIMERSETTIME;
	status = Irq_RegisterTimerIrq(
			&irqTimer0,
			&irqThread0.irqContext,
			timeoutValue);

	//set the thread to run and create the thread
	irqThread0.irqThreadRdy = NiFpga_True;
	pthread_create(
		&thread,
		NULL,
		Timer_Irq_Thread,
		&irqThread0);

	EncoderC_initialize(
			myrio_session,
			&encC0
			);

	//until a backspace occurs run the ctable2() function
	ctable2(Table_Title,my_table,table_num);

	//set the new thread to off and wait for it to finish
	irqThread0.irqThreadRdy = NiFpga_False;
	pthread_join(thread,NULL);

	status = Irq_UnregisterTimerIrq(
		&irqTimer0,
		irqThread0.irqContext);

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*
 * Name: 		Timer_Irq_Thread
 * Purpose:		catches the interrupt thread
 *				Initializes the analog I/O
 *				calculates velocity error and calls cascade to determine next voltage
 *				Saves data to matlab file
 *				Update the filter based on the table information
 * Parameters:	void* resource – this is the ThreadResouce structure and is immediately recast
 * Return:		void* 0 – for normal operation
 *				Procedural
 *					updates the filter
 */
void* Timer_Irq_Thread (void* resource){
	timeoutValue = (*bti)*1000;
	timeoutValue_seconds = timeoutValue*1e-6;
	timeoutValue_ms = timeoutValue*1e-3;

	double vel_error;
	int myFilter_ns = 1;

	//cast the threadResource structure back to the appropriate type
	ThreadResource* threadResource = (ThreadResource*) resource;

	//initialize the analog I/O
	AIO_initialize(&CI0, &CO0);
	Aio_Write(&CO0, 0.0);

	//while the main thread signals this one to contine
	while(irqThread0.irqThreadRdy == NiFpga_True){

		Irq_Wait(
			threadResource -> irqContext,
			TIMERIRQNO,
			&irqAssert,
			(NiFpga_Bool*) &(threadResource -> irqThreadRdy));

		//if the irq is asserted schedule the next interrupt
		if(irqAssert){
				NiFpga_WriteU32(	myrio_session,
									IRQTIMERWRITE,
									timeoutValue);
				NiFpga_WriteBool(	myrio_session,
									IRQTIMERSETTIME,
									NiFpga_True);

				*vact = vel();

				//calcaulte the new values within the filter
				update_filter();

				//calculate the current error in rad/s
				vel_error = rpm2rad((*vact-*vref));

				//calculate the next value for analog out based on cascade
				*vdaout = cascade(
									-vel_error,
									&myFilter,
									myFilter_ns,
									lo,
									hi);

				//write this value to the analog output
				Aio_Write(&CO0, *vdaout);

				//matlab code
				//check for a change in reference
				ref_cur = *vref;
				if(ref_cur != ref_pre){
					MATLAB_ref_pre = ref_pre;
					MATLAB_velocity_pointer = MATLAB_velocity;
					MATLAB_torque_pointer = MATLAB_torque;
					ref_pre = ref_cur;
				}
				//if the arrays have not yet been exceeded save the values
				if( MATLAB_velocity_pointer < (MATLAB_velocity+MATLAB_points))
					*MATLAB_velocity_pointer++ = rpm2rad(*vact);
				if( MATLAB_torque_pointer < MATLAB_torque+MATLAB_points)
					*MATLAB_torque_pointer++ = *vdaout * kvi * kt;
				//end matlab code
				Irq_Acknowledge(irqAssert);
		}
	}
	//write matlab file
	mf = openmatfile("RileyLab7.mat", &err);
	if(!mf) printf("Can't open mat file %d\n", err);
	matfile_addstring(mf, "Run", "08");
	matfile_addstring(mf, "Name", "Riley Poole");
	matfile_addmatrix(mf, "Velocity", MATLAB_velocity, MATLAB_points, 1, 0);
	matfile_addmatrix(mf, "Torque", MATLAB_torque, MATLAB_points, 1, 0);
	matfile_addmatrix(mf, "Current_Reference", vref, 1, 1, 0);
	matfile_addmatrix(mf, "Previous_Reference", &MATLAB_ref_pre, 1, 1, 0);
	matfile_addmatrix(mf, "Kp", kp, 1, 1, 0);
	matfile_addmatrix(mf, "Ki", ki, 1, 1, 0);
	matfile_addmatrix(mf, "Bti",bti, 1, 1, 0);
	matfile_close(mf);

	return 0;
}

/*
 * Name: 		cascade()
 * Purpose:		calculate the output level based on the filter parameters set in the biquad
 * 				checks for a saturation condition
				Update the filter based on the table information
 * Parameters:	double xin - next input to the biquad cascade from the analog input
 * 				struct biquad* fa - the biquad structure that defines the filter parameters
 * 				int ns - number of sections in the biquad structure
 * 				double ymin - the minimum possible output value
 * 				double yman - the maximum possible output value
 * Return:		double - the output of the serial biquad structure
 */
double cascade(
					double xin, //input
					struct biquad* fa, //biquad array
					int ns, //no.segments
					double ymin, //min output
					double ymax ) //max output
{
	double y0;
	int i;
	struct biquad* f = fa;

	//iterate over the biquad structure to find the next output value
	y0 = xin;
	for(i=0;i<ns;i++){
		f -> x0 = y0;
		y0 = (f->b0*f->x0 + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2) / f->a0;
		f->y2 = f->y1; f->y1 = y0; f->x2 = f->x1; f->x1 = f->x0;
		f++;
	}
	y0 = SATURATE(y0, ymin, ymax);
	return y0;
}

/*
 * Name: 		update_filter()
 * Purpose:		update the filter based on the current table values
 * Parameters:	None
 * Return:		None - procedural
 */
void update_filter(void){
	myFilter.a0 = 1.0;
	myFilter.a1 = -1.0;
	myFilter.a2 = 0.0; //PI controller
	myFilter.b0 = (*kp) + 0.5*(*ki)*(timeoutValue_seconds); //from table
	myFilter.b1 = -(*kp) + 0.5*(*ki)*(timeoutValue_seconds); //from table
	myFilter.b2 = 0.0; //PI controller
}

/*
 * Name: 		vel()
 * Purpose:		calculate the current velocity in rpm
 * Parameters:	None
 * Return:		double rpm – the velocity of the motor in rpm
 */
double vel(void){
	double velocity;
	double rpm;
	static int vel_flag = 0;
	static int current_count,count_delta,old_count;

	//initialize vel if this is the first call
	if(vel_flag == 0) old_count = current_count = Encoder_Counter(&encC0);
	if(vel_flag == 1){//check is not the first call
		//read the current encoder count
		current_count = Encoder_Counter(&encC0);

		//compute the speed as the difference between the current and previous counts
		count_delta = current_count - old_count;
		velocity = count_delta;
	}

	old_count = current_count;
	vel_flag = 1;

	//convert the units
	rpm = bdi_bti2rpm(velocity);
	return rpm;
}

/*
 * Name: 		setup_table()
 * Purpose:		initialize the table used in main
				set the convenient names for calling elsewhere in the program
				assign the table to the ThreadResource structure
 * Parameters:	None
 * Return:		no return
				Procedural
					initialized table and table variables
					table set to the correct element of ThreadResource
 */
void setup_table(void){
	//set the timeout and timeout_seconds relationship
	timeoutValue_seconds = timeoutValue*1e-6;
	timeoutValue_ms = timeoutValue*1e-3;

    //initialize the table editor variables
	Table_Title = "Table Title";

	(my_table+0) -> e_label = "V_ref rpm: ";
	(my_table+0) -> e_type = 1;
	(my_table+0) -> value = 0.0;

	(my_table+1) -> e_label = "V_act rpm: ";
	(my_table+1) -> e_type = 0;
	(my_table+1) -> value = 0.0;

	(my_table+2) -> e_label = "VDAout mv: ";
	(my_table+2) -> e_type = 0;
	(my_table+2) -> value = 0.0;

	(my_table+3) -> e_label = "Kp V-s/r: ";
	(my_table+3) -> e_type = 1;
	(my_table+3) -> value = 1.0;

	(my_table+4) -> e_label = "Ki V/r: ";
	(my_table+4) -> e_type = 1;
	(my_table+4) -> value = 0.0;

	(my_table+5) -> e_label = "BTI ms: ";
	(my_table+5) -> e_type = 1;
	(my_table+5) -> value = timeoutValue_ms; //microseconds to miliseconds

	//convenient names
	vref =  &((my_table+0) -> value);
	vact =  &((my_table+1) -> value);
	vdaout =  &((my_table+2) -> value);
	kp =  &((my_table+3) -> value);
	ki =  &((my_table+4) -> value);
	bti =  &((my_table+5) -> value);

	//set initial values
	*kp = 0.1;
	*ki = 2.0;
	*bti = 5;
	*vref = 0;

	//assign the table to the threadresource structure
	irqThread0.atable = my_table;
}

/*
 * Name: 		rpm2rad()
 * Purpose:		change the units of the input from rpm to radians
 * Parameters:	double rpm – the value in units of rpm
 * Return:		double rad – the value in radians/second
 */
double rpm2rad(double rpm){
	double rad;
	rad = rpm*2.*3.14/60.;
	return rad;
}

/*
 * Name: 		bdi_bti2rpm()
 * Purpose:		change the units of the input from bdi/bti to rpm which is more natural
 * Parameters:	double bdi_bti
 * Return:		double rpm – the value of the input variable with units changed from bdi/bti to rpm
 */
double bdi_bti2rpm(double bdi_bti){
	double rpm;
	rpm = bdi_bti * (1.0/2000.0) * (1.0/timeoutValue_seconds) * 60.0;//bti is converted from milliseconds to seconds
	return rpm;
}
