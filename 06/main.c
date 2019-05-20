/* Lab #<6> - <Riley Poole> */
#define SATURATE(x,lo,hi) ( (x)<(lo) ? (lo) : (x)>(hi) ? (hi):(x) )

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include "TimerIRQ.h"
#include "matlabfiles.h"
#include "pthread.h"
#include "DIIRQ.h"
#include "AIO.h"
#include <math.h>

struct biquad{
	double b0; double b1; double b2;
	double a0; double a1; double a2;
	double x0; double x1; double x2;
	double y1; double y2;
};

//define the biquad structure
static struct biquad myFilter[] = {
{1.0000e+00, 9.9999e-01, 0.0000e+00,
1.0000e+00, -8.8177e-01, 0.0000e+00, 0, 0, 0, 0, 0},
{2.1878e-04, 4.3755e-04, 2.1878e-04,
1.0000e+00, -1.8674e+00, 8.8220e-01, 0, 0, 0, 0, 0}
};

typedef struct {
	NiFpga_IrqContext irqContext;
	NiFpga_Bool irqThreadRdy;
} ThreadResource;

void* Timer_Irq_Thread(void* resource);
double cascade(
	double xin,
	struct biquad*,
	int ns,
	double ymin,
	double ymax );

MyRio_Aio CI0, CO0;

uint32_t status;
MyRio_IrqTimer irqTimer0;
ThreadResource irqThread0;
pthread_t thread;

int err;
MATFILE matfile;
MATFILE* mf;
#define MATLAB_points 10000
double MATLAB_input[MATLAB_points];
int MATLAB_input_index = 0;
double MATLAB_output[MATLAB_points];
int MATLAB_output_index = 0;


/*----------------------------------------
Function main()
Purpose: Procedural section for testing filter; Setting up threads and interrupts
Parameters: none
Returns: status if error
*-----------------------------------------*/
int main(int argc, char **argv)
{

    status = MyRio_Open();
    if (MyRio_IsNotSuccess(status)) return status;

    //signal that the program is running
    printf_lcd("\fRunning");

    //set up the timer interrupt parameters
	irqTimer0.timerWrite = IRQTIMERWRITE;
	irqTimer0.timerSet = IRQTIMERSETTIME;
	int timeoutValue = 5;

	//register the timer interrupt
	status = Irq_RegisterTimerIrq(
		&irqTimer0,
		&irqThread0.irqContext,
		timeoutValue);

	//set the initial thread condition to run
	irqThread0.irqThreadRdy = NiFpga_True;

	//create a new thread
	pthread_create(
		&thread,
		NULL,
		Timer_Irq_Thread,
		&irqThread0);

	//halt the main thread until the backspace key is pressed
	while(getkey() != '\b'){}

	//end the timer thread
	irqThread0.irqThreadRdy = NiFpga_False;

	//wait for the timer thread to end then unregister the interrupt
	pthread_join(thread,NULL);
	status = Irq_UnregisterTimerIrq(
		&irqTimer0,
		irqThread0.irqContext);

	//signal that the prgram has ended
	printf_lcd("\fEnd...");

	status = MyRio_Close();
	return status;

	return 0;
}

/*----------------------------------------
Function Timer_Irq_Thread()
Purpose: Code to run during the timer interrupt
Parameters: void* resource - a pointer to void that is actually a pointer to a ThreadResource structure
Returns:
		procedure -	Sets the output on the DAC based on the previous values of cascade()
					saves the responses to a matlab file with pointers
		void* - No returns

*-----------------------------------------*/
void* Timer_Irq_Thread(void* resource){
	uint32_t irqAssert = 0;
	double xn;
	double yn;
	int myFilter_ns = 2;
	double ymin = -10.000;
	double ymax = 10.000;
	ThreadResource* threadResource = (ThreadResource*) resource;
	int timeoutValue = 500;

	extern NiFpga_Session myrio_session;

	//initialize the input output
	AIO_initialize(&CI0,&CO0);
	Aio_Write(&CO0,0.0);

	//while main indicates this thread should run
	while(irqThread0.irqThreadRdy == NiFpga_True)
			{
			//wait for the interrupt to be asserted
			Irq_Wait(
				threadResource -> irqContext,
				TIMERIRQNO,
				&irqAssert,
				(NiFpga_Bool*) &(threadResource -> irqThreadRdy));
			//write the timer registers
				NiFpga_WriteU32(
					myrio_session,
					IRQTIMERWRITE,
					timeoutValue);
				NiFpga_WriteBool(
					myrio_session,
					IRQTIMERSETTIME,
					NiFpga_True);

				//brinh in analog input for cascade
				xn = Aio_Read(&CI0);

				//use cascade to calculate the series of outputs
				yn = cascade(
					xn,
					myFilter,
					myFilter_ns,
					ymin,
					ymax);

				//save the responses to matlab
				if( MATLAB_input_index < (MATLAB_points))
					*(MATLAB_input+MATLAB_input_index++) = xn;
				if( MATLAB_output_index < MATLAB_points)
					*(MATLAB_output+MATLAB_output_index++) = yn;

				//write the output value to the analog channel
				Aio_Write(&CO0, yn);
				Irq_Acknowledge(irqAssert);
			}

	//save the responses to matlab
	mf = openmatfile("Lab.mat", &err);
	if(!mf) printf("Can't open mat file %d\n", err);
	matfile_addmatrix(mf, "Input", MATLAB_input, MATLAB_points, 1,0);
	matfile_addmatrix(mf, "Output", MATLAB_output, MATLAB_points, 1,0);

	//tell the thread to exit and end
	pthread_exit(NULL);
	return NULL; //I don't think you can do both lines here
}

/*----------------------------------------
Function cascade()
Purpose: Code to run during the timer interrupt
Parameters: void* resource - a pointer to void that is actually a pointer to a ThreadResource structure
Returns:
		procedure -	Sets the output on the DAC based on the previous values of cascade()
					saves the responses to a matlab file with pointers
		void* - No returns

*-----------------------------------------*/
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
	//the first input value to the cascade is the analong input value; then loop through each biquad twice
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





