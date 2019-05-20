/* Lab #<5> - <Riley Poole> */

//#define INPUT_STARTS_HIGH
#define INPUT_STARTS_LOW

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h> //I included
#include "DIIRQ.h"//I included
#include "IRQConfigure.h"//I included
#include "DIO.h"

//set up the interrupt direction
#ifdef INPUT_STARTS_HIGH
const Irq_Dio_Type TriggerType = Irq_Dio_FallingEdge;
const int READ_BIT_END = 0;
#endif

#ifdef INPUT_STARTS_LOW
const Irq_Dio_Type TriggerType = Irq_Dio_RisingEdge;
const int READ_BIT_END = 1;
#endif

/*structure delcarations*/
typedef struct{
	NiFpga_IrqContext irqContext; //IRQ context reserved
	NiFpga_Bool irqThreadRdy; //IRQ thread ready flag
	uint8_t irqNumber; //IRQ number value
} ThreadResource ;

/*declarations*/
NiFpga_Status status;
MyRio_Dio ch0;
//
ThreadResource irqThread0;
pthread_t thread;
MyRio_IrqDi irqDI0;
const uint8_t IrqNumber = 2;
const uint32_t Count = 1;

/*prototypes*/
void wait(void) ;
void* DI_Irq_Thread(void *resource);

int main(int argc, char **argv){

	//open the myRIO
    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //initialize the ditital output
	//initialize channels 0 , 6, 7
	ch0.dir = DIOA_70DIR;
	ch0.out = DIOA_70OUT;
	ch0.in = DIOA_70IN;
	ch0.bit = 0;

	//specify IRQ channel settings
	irqDI0.dioCount = IRQDIO_A_0CNT;
	irqDI0.dioIrqNumber = IRQDIO_A_0NO;
	irqDI0.dioIrqEnable = IRQDIO_A_70ENA;
	irqDI0.dioIrqRisingEdge = IRQDIO_A_70RISE;
	irqDI0.dioIrqFallingEdge = IRQDIO_A_70FALL;
	irqDI0.dioChannel = Irq_Dio_A0;

	//initiazte the IRQ number resource for new thread
	irqThread0.irqNumber = IrqNumber;

	//register DIO IRQ. Terminate if not successful
	status = Irq_RegisterDiIrq(
		&irqDI0,
		&(irqThread0.irqContext),
		IrqNumber,
		Count,
		TriggerType);

	//set the indicator to allow the new thread
	irqThread0.irqThreadRdy = NiFpga_True;

	//create new thread to catch the IRQ
	status = pthread_create(
		&thread,
		NULL,
		DI_Irq_Thread,
		&irqThread0);

	//OTHER MAIN TASKS GO HERE

	//main loop ends after 60 counts
	int count;
	for(count=0; count<= 60; count++){
		//wait one second
		int j;
		for(j = 0; j<200; j++){
			wait();
		}
		//clear the display and print the value of count
		printf_lcd("\f%d",count);
	}

	//signal the interrupt thread to stop and wait unti lit terminates
	irqThread0.irqThreadRdy = NiFpga_False;
	pthread_join(thread,NULL);

	//unregister the interrupt
	status = Irq_UnregisterDiIrq(
		&irqDI0,
		&(irqThread0.irqContext),
		IrqNumber);

	//close the myRIO
	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

void* DI_Irq_Thread(void *resource){

	//cast teh parameter
	ThreadResource* threadResource = (ThreadResource*) resource;

	//while the main thread does not signal this thread to stop
	uint32_t irqAssert = 0;
	while(threadResource->irqThreadRdy == NiFpga_True){

		//wait for the IRQ
		Irq_Wait( //wait for thread timeout
			threadResource->irqContext,
			threadResource->irqNumber,
			&irqAssert,
			(NiFpga_Bool*) &(threadResource->irqThreadRdy));

		//if the numbered IRQ has been asserted
		if(irqAssert & ( 1 << threadResource -> irqNumber)){

			NiFpga_Bool read_bit = 0;
			do{
				read_bit = Dio_ReadBit(&ch0);
				printf_lcd("\finterrupt_");
				int i;
				for(i=0; i<5; i++)
				wait();
			}
			while(read_bit == READ_BIT_END);
			Irq_Acknowledge(irqAssert);
		}
	}
	//terminate the new thread and return from the function
	pthread_exit(NULL);
	return NULL;
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
