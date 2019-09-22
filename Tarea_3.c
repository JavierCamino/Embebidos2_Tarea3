#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_debug_console.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "event_groups.h"
#include "queue.h"


/////////////////////////////////////////
/* Macros */
/////////////////////////////////////////
#define SECONDS_TASK_NAME			"seconds_task"

#define INITIAL_SECONDS				(15U)
#define INITIAL_MINUTES 			(17U)
#define INITIAL_HOURS    			(4U )

#define EVENT_SECONDS_BITMASK		(1 << 0)
#define EVENT_MINUTES_BITMASK		(1 << 1)
#define EVENT_HOURS_BITMASK			(1 << 2)

#define SECONDS_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define MINUTES_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define HOURS_TASK_PRIORITY			(configMAX_PRIORITIES - 1U)

#define CREATE_TASK(a,b,c,d,e,f)	{												\
										if( pdPASS != xTaskCreate(a,b,c,d,e,f) )	\
										{											\
											PRINTF("%s creation failed.\r\n",#b);	\
											for(;;);								\
										}											\
									}
#define CREATE_SEMAPHORE(r)			{												\
										r = xSemaphoreCreateBinary();				\
										if(NULL == r)								\
										{											\
											PRINTF("%s creation failed.\r\n",#r);	\
											for(;;);								\
										}											\
									}
#define CREATE_EVENTGROUP(r)		{												\
										r = xEventGroupCreate();					\
										if(NULL == r)								\
										{											\
											PRINTF("%s creation failed.\r\n",#r);	\
											for(;;);								\
										}											\
									}
#define CREATE_QUEUE(r,a,b)			{												\
										r = xQueueCreate(a,b);						\
										if(NULL == r)								\
										{											\
											PRINTF("%s creation failed.\r\n",#r);	\
											for(;;);								\
										}											\
									}



/////////////////////////////////////////
/* Data Types */
/////////////////////////////////////////
typedef enum {seconds_type, minutes_tipe, hours_type} time_types_t;
typedef struct
{
	time_types_t time_type;
	uint8_t value;
}time_msg_t;


/////////////////////////////////////////
/* Global variables */
/////////////////////////////////////////
uint8_t minutes_semaphore = 0xF4;
uint8_t hours_semaphore   = 0xF4;

uint8_t alarm_seconds = 5;
uint8_t alarm_minutes = 2;
uint8_t alarm_hours   = 3;


/////////////////////////////////////////
/* Queues */
/////////////////////////////////////////
static QueueHandle_t UART_mailbox 			= NULL;


/////////////////////////////////////////
/* Semaphores */
/////////////////////////////////////////
SemaphoreHandle_t xMinutes_semaphore 		= NULL;


/////////////////////////////////////////
/* Events */
/////////////////////////////////////////
static EventGroupHandle_t alarm_event_group = NULL;


/////////////////////////////////////////
/* Tasks */
/////////////////////////////////////////
static void seconds_task(void *pvParameters);






int main(void) {

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
  	/* Init FSL debug console. */
    BOARD_InitDebugConsole();

    /* Create Semaphores */
    CREATE_SEMAPHORE(xMinutes_semaphore);


    /* Create Event */
    CREATE_EVENTGROUP(alarm_event_group);


    /* Create Queue */
    CREATE_QUEUE(UART_mailbox, 4U, sizeof(time_msg_t) );


    /* Create tasks. */
    CREATE_TASK(seconds_task, SECONDS_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, SECONDS_TASK_PRIORITY, NULL);


    PRINTF("Hello World\n");

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");
    }
    return 0 ;
}



static void seconds_task(void *pvParameters)
{
	static uint8_t seconds     = INITIAL_SECONDS;
	static time_msg_t uart_msg = {seconds_type, INITIAL_SECONDS};

    for (;;)
    {

        /* Increment seconds count. */
    	seconds++;


    	/* When seconds reaches 60: */
    	if(60U == seconds)
    	{
    		/* Reset counter */
    		seconds = 0;
    		/* Free semaphore */
    		xSemaphoreGive(xMinutes_semaphore);
    	}


    	/* If seconds equals alarm seconds: */
    	if(alarm_seconds)
    	{
    		xEventGroupSetBits(alarm_event_group, EVENT_SECONDS_BITMASK);
    	}


    	/* Send message to UART with the current time. */
    	uart_msg.value = seconds;
    	xQueueSendToBack(UART_mailbox, &uart_msg, 0);


    	/* Execute periodically each second. */
    	vTaskDelay( pdMS_TO_TICKS(1000) );
    }
}
