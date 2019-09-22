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

#define INITIAL_SECONDS				(45U)
#define INITIAL_MINUTES 			(58U)
#define INITIAL_HOURS    			(23U)

#define EVENT_SECONDS_BITMASK		(1 << 0)
#define EVENT_MINUTES_BITMASK		(1 << 1)
#define EVENT_HOURS_BITMASK			(1 << 2)

#define SECONDS_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define MINUTES_TASK_PRIORITY		(configMAX_PRIORITIES - 1U)
#define HOURS_TASK_PRIORITY			(configMAX_PRIORITIES - 1U)
#define ALARM_TASK_PRIORITY			(configMAX_PRIORITIES - 2U)
#define PRINT_TASK_PRIORITY			(configMAX_PRIORITIES - 3U)

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
uint8_t alarm_seconds = 0;
uint8_t alarm_minutes = 0;
uint8_t alarm_hours   = 0;


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

/////////////////////////////////////////
/* Task Handles */
/////////////////////////////////////////
TaskHandle_t seconds_task_handle = 0;
TaskHandle_t minutes_task_handle = 0;
TaskHandle_t hours_task_handle   = 0;
TaskHandle_t alarm_task_handle   = 0;
TaskHandle_t print_task_handle   = 0;


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
    if(INITIAL_SECONDS == alarm_seconds) xEventGroupSetBits(alarm_event_group, EVENT_SECONDS_BITMASK);
    if(INITIAL_MINUTES == alarm_minutes) xEventGroupSetBits(alarm_event_group, EVENT_MINUTES_BITMASK);
    if(INITIAL_HOURS   == alarm_hours  ) xEventGroupSetBits(alarm_event_group, EVENT_HOURS_BITMASK);


    /* Create Queue */
    CREATE_QUEUE(UART_mailbox, 4U, sizeof(time_msg_t) );


    /* Create tasks. */
    CREATE_TASK(seconds_task, SECONDS_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, SECONDS_TASK_PRIORITY, &seconds_task_handle);
    CREATE_TASK(minutes_task, MINUTES_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, MINUTES_TASK_PRIORITY, &minutes_task_handle);
    CREATE_TASK(hours_task, HOURS_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, HOURS_TASK_PRIORITY, &hours_task_handle);
    CREATE_TASK(alarm_task, ALARM_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, ALARM_TASK_PRIORITY, &alarm_task_handle);
    CREATE_TASK(print_task, PRINT_TASK_NAME, configMINIMAL_STACK_SIZE, NULL, PRINT_TASK_PRIORITY, &print_task_handle);


    PRINTF("Alarm Clock Initialization Successful\r\n");


    /* Start scheduler */
    vTaskStartScheduler();
    for(;;);
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
    		/* Wake up minutes task */
    		vTaskResume( minutes_task_handle );
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
    	vTaskDelay( pdMS_TO_TICKS(1000U) );
    }
}
static void minutes_task(void *pvParameters)
{
	static uint8_t minutes     = INITIAL_MINUTES;
	static time_msg_t uart_msg = {minutes_type, INITIAL_MINUTES};

    for (;;)
    {
    	vTaskSuspend( minutes_task_handle );

        /* Increment minutes count. */
    	minutes++;


    	/* When minutes reaches 60: */
    	if(60U == minutes)
    	{
    		/* Reset counter */
    		minutes = 0;
    		/* Wake up hours_task*/
    		vTaskResume( hours_task_handle );
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
    	vTaskSuspend( hours_task_handle );

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
		PRINTF("ALARM!\r\n");
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


		if(0 == uxQueueMessagesWaiting(UART_mailbox))
		{
			/* Take semaphore */
			while(pdPASS != xSemaphoreTake(xUART_semaphore, 0) );
			/* Critic Section */
			PRINTF("%02d:%02d:%02d\r\n",hours, minutes, seconds);
			/* Release semaphore */
			xSemaphoreGive(xUART_semaphore);
		}

	}
}

