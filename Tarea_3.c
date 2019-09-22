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
#define MINUTES_TASK_NAME			"minutes_task"
#define HOURS_TASK_NAME				"hours_task"
#define ALARM_TASK_NAME				"alarm_task"
#define PRINT_TASK_NAME				"print_task"

#define INITIAL_SECONDS				(15U)
#define INITIAL_MINUTES 			(17U)
#define INITIAL_HOURS    			(4U )

#define EVENT_SECONDS_BITMASK		(1 << 0)
#define EVENT_MINUTES_BITMASK		(1 << 1)
#define EVENT_HOURS_BITMASK			(1 << 2)

#define SECONDS_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define MINUTES_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define HOURS_TASK_PRIORITY			(configMAX_PRIORITIES - 1U)
#define ALARM_TASK_PRIORITY			(configMAX_PRIORITIES - 1U)
#define PRINT_TASK_PRIORITY			(configMAX_PRIORITIES - 1U)

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
typedef enum {seconds_type, minutes_type, hours_type} time_types_t;
typedef struct
{
	time_types_t time_type;
	uint8_t value;
}time_msg_t;


/////////////////////////////////////////
/* Global variables */
/////////////////////////////////////////
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
SemaphoreHandle_t xHours_semaphore 			= NULL;
SemaphoreHandle_t xUART_semaphore 			= NULL;


/////////////////////////////////////////
/* Events */
/////////////////////////////////////////
static EventGroupHandle_t alarm_event_group = NULL;


/////////////////////////////////////////
/* Tasks */
/////////////////////////////////////////
static void seconds_task(void *pvParameters);
static void minutes_task(void *pvParameters);
static void hours_task(void *pvParameters);
static void alarm_task(void *pvParameters);
static void print_task(void *pvParameters);







int main(void) {

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
  	/* Init FSL debug console. */
    BOARD_InitDebugConsole();


    /* Create Semaphores */
    CREATE_SEMAPHORE(xMinutes_semaphore);
    CREATE_SEMAPHORE(xHours_semaphore);
    CREATE_SEMAPHORE(xUART_semaphore);


    /* Create Event */
    CREATE_EVENTGROUP(alarm_event_group);


    /* Create Queue */
    CREATE_QUEUE(UART_mailbox, 4U, sizeof(time_msg_t) );


    /* Create tasks. */
    CREATE_TASK(seconds_task, SECONDS_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, SECONDS_TASK_PRIORITY, NULL);
    CREATE_TASK(minutes_task, MINUTES_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, MINUTES_TASK_PRIORITY, NULL);
    CREATE_TASK(hours_task, HOURS_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, HOURS_TASK_PRIORITY, NULL);
    CREATE_TASK(alarm_task, ALARM_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, ALARM_TASK_PRIORITY, NULL);
    CREATE_TASK(print_task, PRINT_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, PRINT_TASK_PRIORITY, NULL);


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

	/* Free UART semaphore */
	xSemaphoreGive(xUART_semaphore);

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
    	if(alarm_seconds == seconds)
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
static void minutes_task(void *pvParameters)
{
	static uint8_t minutes     = INITIAL_MINUTES;
	static time_msg_t uart_msg = {minutes_type, INITIAL_MINUTES};

    for (;;)
    {

    	/* Wait for semaphore to free. */
    	while(pdPASS != xSemaphoreTake(xMinutes_semaphore, 0) );

        /* Increment minutes count. */
    	minutes++;


    	/* When minutes reaches 60: */
    	if(60U == minutes)
    	{
    		/* Reset counter */
    		minutes = 0;
    		/* Free semaphore */
    		xSemaphoreGive(xHours_semaphore);
    	}


    	/* If seconds equals alarm seconds: */
    	if(alarm_minutes == minutes)
    	{
    		xEventGroupSetBits(alarm_event_group, EVENT_MINUTES_BITMASK);
    	}


    	/* Send message to UART with the current time. */
    	uart_msg.value = minutes;
    	xQueueSendToBack(UART_mailbox, &uart_msg, 0);


    }
}
static void hours_task(void *pvParameters)
{
	static uint8_t hours       = INITIAL_HOURS;
	static time_msg_t uart_msg = {hours_type, INITIAL_HOURS};

    for (;;)
    {

    	/* Wait for semaphore to free. */
    	while(pdPASS != xSemaphoreTake(xHours_semaphore, 0) );

        /* Increment minutes count. */
    	hours++;


    	/* When minutes reaches 60: */
    	if(24U == hours)
    	{
    		/* Reset counter */
    		hours = 0;
    	}


    	/* If seconds equals alarm seconds: */
    	if(alarm_hours == hours)
    	{
    		xEventGroupSetBits(alarm_event_group, EVENT_HOURS_BITMASK);
    	}


    	/* Send message to UART with the current time. */
    	uart_msg.value = hours;
    	xQueueSendToBack(UART_mailbox, &uart_msg, 0);


    }
}
static void alarm_task(void *pvParameters)
{
	for(;;)
	{
		/* Wait for event group */
		while(  (EVENT_SECONDS_BITMASK | EVENT_MINUTES_BITMASK | EVENT_HOURS_BITMASK) !=
				xEventGroupWaitBits(alarm_event_group,
									EVENT_SECONDS_BITMASK | EVENT_MINUTES_BITMASK | EVENT_HOURS_BITMASK,
									pdTRUE,
									pdTRUE,
									1U)
			 );

		/* Take semaphore */
		while(pdPASS != xSemaphoreTake(xUART_semaphore, 0) );
		/* Critic Section */
		PRINTF("ALARM!\n");
		/* Release semaphore */
		xSemaphoreGive(xUART_semaphore);

	}
}
static void print_task(void *pvParameters)
{
	static uint8_t seconds = INITIAL_SECONDS;
	static uint8_t minutes = INITIAL_MINUTES;
	static uint8_t hours   = INITIAL_HOURS;
	static time_msg_t received_message = {0};

	for(;;)
	{
		/* Wait to receive from queue. */
		while(pdPASS != xQueueReceive( UART_mailbox, &received_message,0U) );

		switch(received_message.time_type)
		{
		case seconds_type: seconds = received_message.value; break;
		case minutes_type: minutes = received_message.value; break;
		case hours_type  : hours   = received_message.value; break;
		default: for(;;); break;
		}

		/* Take semaphore */
		while(pdPASS != xSemaphoreTake(xUART_semaphore, 0) );
		/* Critic Section */
		PRINTF("%02d:%02d:%02d\n",hours, minutes, seconds);
		/* Release semaphore */
		xSemaphoreGive(xUART_semaphore);

	}
}

