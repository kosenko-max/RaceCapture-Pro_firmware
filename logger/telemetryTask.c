/*
 * telemetryTask.c
 *
 *  Created on: Mar 6, 2012
 *      Author: brent
 */
#include "telemetryTask.h"
#include "task.h"
#include "semphr.h"
#include "loggerConfig.h"
#include "sampleRecord.h"
#include "modp_numtoa.h"
#include "loggerHardware.h"
#include "loggerConfig.h"
#include "usart.h"
#include "cellTelemetry.h"
#include "btTelemetry.h"
#include "consoleConnectivity.h"

static ConnParams g_connParams;

//wait time for sample queue. can be portMAX_DELAY to wait forever, or zero to not wait at all
#define TELEMETRY_QUEUE_WAIT_TIME					0
//#define TELEMETRY_QUEUE_WAIT_TIME					portMAX_DELAY

#define TELEMETRY_TASK_PRIORITY					( tskIDLE_PRIORITY + 4 )
#define TELEMETRY_STACK_SIZE  					1000
#define SAMPLE_RECORD_QUEUE_SIZE				10


portBASE_TYPE queueTelemetryRecord(SampleRecord * sr){
	if (NULL != g_sampleRecordQueue){
		return xQueueSend(g_connParams.sampleQueue, &sr, TELEMETRY_QUEUE_WAIT_TIME);
	}
	else{
		return errQUEUE_EMPTY;
	}
}

void createConnectivityTask(){
	g_connParams.sampleQueue = xQueueCreate(SAMPLE_RECORD_QUEUE_SIZE,sizeof( SampleRecord *));
	if (NULL == g_connParams.sampleQueue){
		//TODO log error
		return;
	}

	switch(getWorkingLoggerConfig()->ConnectivityConfigs.connectivityMode){
		case CONNECTIVITY_MODE_CONSOLE:
			xTaskCreate( consoleConnectivityTask, ( signed portCHAR * ) "conn", TELEMETRY_STACK_SIZE, &g_connParams, TELEMETRY_TASK_PRIORITY, NULL );
			break;
		case CONNECTIVITY_MODE_BLUETOOTH:
			xTaskCreate( btTelemetryTask, ( signed portCHAR * ) "connBT", TELEMETRY_STACK_SIZE, &g_connParams, TELEMETRY_TASK_PRIORITY, NULL );
			break;
		case CONNECTIVITY_MODE_CELL:
			xTaskCreate( cellTelemetryTask, ( signed portCHAR * ) "connCell", TELEMETRY_STACK_SIZE, &g_connParams, TELEMETRY_TASK_PRIORITY, NULL );
			break;
	}
}
