/*
 * amdgpuinfo - AMD/ATI GPU usage info from fglrx driver using ADL
 *
 *    Copyright (C) 2016 Rodrigo Silva (MestreLion) <linux@rodrigosilva.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program. See <http://www.gnu.org/licenses/gpl.html>
 */

#include <stdlib.h>	// malloc, free
#include <dlfcn.h>	// dyopen, dlsym, dlclose
#include <stdarg.h>	// va_{list,start,end}
#include <string.h>	// memset
#include <stdio.h>	// printf
#include <argp.h>	// argp_parse

#ifndef LINUX
#define LINUX 1
#endif
#include <adl_sdk.h>


#define FANCTRL_SUPPORTS_READ  (ADL_DL_FANCTRL_SUPPORTS_PERCENT_READ | ADL_DL_FANCTRL_SUPPORTS_RPM_READ)
#define UNUSED(x)              (void)(x)


typedef int (*ADL_MAIN_CONTROL_CREATE)           (ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (*ADL_MAIN_CONTROL_DESTROY)          ();
typedef int (*ADL_ADAPTER_NUMBEROFADAPTERS_GET)  (int*);
typedef int (*ADL_ADAPTER_ADAPTERINFO_GET)       (LPAdapterInfo, int);
typedef int (*ADL_ADAPTER_ACTIVE_GET)            (int, int*);
typedef int (*ADL_OD5_TEMPERATURE_GET)           (int, int, ADLTemperature*);
typedef int (*ADL_OD5_FANSPEEDINFO_GET)          (int, int, ADLFanSpeedInfo*);
typedef int (*ADL_OD5_FANSPEED_GET)              (int, int, ADLFanSpeedValue*);
typedef int (*ADL_OD5_CURRENTACTIVITY_GET)       (int, ADLPMActivity*);

typedef struct GPUInfo
{
	int    index;
	char*  name;
	int    active;
	int    load;
	int    fan_caps;
	int    fan_rpm;
	int    fan_max_rpm;
	int    fan_perc;
	float  temperature;
} GPUInfo;


// Memory allocation function callback for ADL_Main_Control_Create()
static void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}


static void printerr(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


static void printerr_status(char* status, GPUInfo gpu)
{
	printerr("Cannot get %s for adapter %d: %s\n", status, gpu.index, gpu.name);
}


typedef struct Arguments
{
	int machine;
} Arguments;

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	Arguments* args = state->input;

	switch (key) {
		case 'm':
			args->machine = 1;
		break;

		case ARGP_KEY_ARG:
			argp_usage(state);
		break;

		default:
			return ARGP_ERR_UNKNOWN;
		break;
	}
	return 0;
}
const char *argp_program_version = "amdgpuinfo 1.0";
static void parse_args(int argc, char **argv, Arguments* args) {
	static char doc[] = "amdgpuinfo - AMD/ATI GPU usage info from fglrx driver";
	static struct argp_option options[] = {
		{"machine",  'm', 0, 0,  "Output in machine-friendly format", 0},
		{ 0, 0, 0, 0, 0, 0 }
	};
	static struct argp argp = {options, parse_opt, 0, doc, 0, 0, 0};
	argp_parse(&argp, argc, argv, 0, 0, args);
}




int main (int argc, char **argv)
{
	int    i              = 0;
	int    retcode        = 0;
	int    iAdapters      = 0;

	void*             hDLL          = NULL;  // Handle to .so library
	LPAdapterInfo     lpAdapterInfo = NULL;  // Adapter info array

	ADLPMActivity     s_load = {sizeof(ADLPMActivity), -1, -1, -1, -1, -1, -1, -1, -1, 0};
	ADLFanSpeedInfo   s_fani = {sizeof(ADLFanSpeedInfo), 0, -1, -1, -1, -1};
	ADLFanSpeedValue  s_fanv = {sizeof(ADLFanSpeedValue), -1, -1, 0};
	ADLTemperature    s_temp = {sizeof(ADLTemperature), -1};
	GPUInfo           gpu;

	Arguments args;
	args.machine = 0;
	parse_args(argc, argv, &args);

	hDLL = dlopen("libatiadlxx.so", RTLD_LAZY | RTLD_GLOBAL);

	if (NULL == hDLL)
	{
		printerr("ADL library not found!\n");
		retcode = 1;
		goto cleanup;
	}

	ADL_MAIN_CONTROL_CREATE           ADL_Main_Control_Create          = (ADL_MAIN_CONTROL_CREATE)          dlsym(hDLL, "ADL_Main_Control_Create");
	ADL_MAIN_CONTROL_DESTROY          ADL_Main_Control_Destroy         = (ADL_MAIN_CONTROL_DESTROY)         dlsym(hDLL, "ADL_Main_Control_Destroy");
	ADL_ADAPTER_NUMBEROFADAPTERS_GET  ADL_Adapter_NumberOfAdapters_Get = (ADL_ADAPTER_NUMBEROFADAPTERS_GET) dlsym(hDLL, "ADL_Adapter_NumberOfAdapters_Get");
	ADL_ADAPTER_ADAPTERINFO_GET       ADL_Adapter_AdapterInfo_Get      = (ADL_ADAPTER_ADAPTERINFO_GET)      dlsym(hDLL, "ADL_Adapter_AdapterInfo_Get");
	ADL_ADAPTER_ACTIVE_GET            ADL_Adapter_Active_Get           = (ADL_ADAPTER_ACTIVE_GET)           dlsym(hDLL, "ADL_Adapter_Active_Get");
	ADL_OD5_CURRENTACTIVITY_GET       ADL_OD5_CurrentActivity_Get      = (ADL_OD5_CURRENTACTIVITY_GET)      dlsym(hDLL, "ADL_Overdrive5_CurrentActivity_Get");
	ADL_OD5_FANSPEEDINFO_GET          ADL_OD5_FanSpeedInfo_Get         = (ADL_OD5_FANSPEEDINFO_GET)         dlsym(hDLL, "ADL_Overdrive5_FanSpeedInfo_Get");
	ADL_OD5_FANSPEED_GET              ADL_OD5_FanSpeed_Get             = (ADL_OD5_FANSPEED_GET)             dlsym(hDLL, "ADL_Overdrive5_FanSpeed_Get");
	ADL_OD5_TEMPERATURE_GET           ADL_OD5_Temperature_Get          = (ADL_OD5_TEMPERATURE_GET)          dlsym(hDLL, "ADL_Overdrive5_Temperature_Get");

	if (
		NULL == ADL_Main_Control_Create          ||
		NULL == ADL_Main_Control_Destroy         ||
		NULL == ADL_Adapter_NumberOfAdapters_Get ||
		NULL == ADL_Adapter_AdapterInfo_Get      ||
		NULL == ADL_Adapter_Active_Get           ||
		NULL == ADL_OD5_Temperature_Get          ||
		NULL == ADL_OD5_FanSpeed_Get             ||
		NULL == ADL_OD5_CurrentActivity_Get
	)
	{
		printerr("ADL's API is missing!\n");
		retcode = 1;
		goto cleanup;
	}

	// Initialize ADL. The second parameter is 1, which means:
	// retrieve adapter information only for adapters that are physically present and enabled in the system
	if (ADL_OK != ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 0))
	{
		printerr("ADL Initialization Error!\n");
		retcode = 1;
		goto cleanup;
	}

	// Obtain the number of adapters for the system
	if (ADL_OK != ADL_Adapter_NumberOfAdapters_Get(&iAdapters))
	{
		printerr("Cannot get the number of adapters!\n");
		retcode = 1;
		goto cleanup;
	}

	if (iAdapters > 0)
	{
		lpAdapterInfo = malloc(sizeof(AdapterInfo) * iAdapters);
		memset(lpAdapterInfo, '\0', sizeof(AdapterInfo) * iAdapters);

		// Get the AdapterInfo structure for all adapters in the system
		ADL_Adapter_AdapterInfo_Get(lpAdapterInfo, sizeof(AdapterInfo) * iAdapters);
	}

	// Repeat for all available adapters in the system
	for (i = 0; i < iAdapters; i++)
	{
		gpu.index       = lpAdapterInfo[i].iAdapterIndex;
		gpu.active      = ADL_FALSE;

		// Skip adapters that are not active
		if ((ADL_OK != ADL_Adapter_Active_Get(gpu.index, &gpu.active)) ||
		    (ADL_TRUE != gpu.active))
			continue;

		gpu.name        = lpAdapterInfo[i].strAdapterName;
		gpu.fan_caps    =  0;
		gpu.load        = -1;
		gpu.fan_rpm     = -1;
		gpu.fan_max_rpm =  0;
		gpu.fan_perc    = -1;
		gpu.temperature = -273.15;

		// Load (Usage %)
		if (ADL_OK != ADL_OD5_CurrentActivity_Get(gpu.index, &s_load))
			printerr_status("GPU load", gpu);
		else
			gpu.load = s_load.iActivityPercent;

		// Fan info and capabilities
		if (ADL_OK != ADL_OD5_FanSpeedInfo_Get(gpu.index, 0, &s_fani))
			printerr_status("fan info", gpu);
		else {
			gpu.fan_caps    = s_fani.iFlags;
			gpu.fan_max_rpm = s_fani.iMaxRPM;
		}

		// Fan RPM
		if (gpu.fan_caps & ADL_DL_FANCTRL_SUPPORTS_RPM_READ) {
			s_fanv.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_RPM;
			if (ADL_OK != ADL_OD5_FanSpeed_Get(gpu.index, 0, &s_fanv))
				printerr_status("fan RPM", gpu);
			else
				gpu.fan_rpm = s_fanv.iFanSpeed;
		}

		// Fan speed percentage
		if (gpu.fan_caps & ADL_DL_FANCTRL_SUPPORTS_PERCENT_READ) {
			s_fanv.iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;
			if (ADL_OK != ADL_OD5_FanSpeed_Get(gpu.index, 0, &s_fanv))
				printerr_status("fan speed", gpu);
			else
				gpu.fan_perc = s_fanv.iFanSpeed;
		}

		// If fan does not support both percentage and RPM, calculate one from the other
		if (gpu.fan_max_rpm && ((gpu.fan_caps & FANCTRL_SUPPORTS_READ) != FANCTRL_SUPPORTS_READ)) {
			if (gpu.fan_caps & ADL_DL_FANCTRL_SUPPORTS_RPM_READ)
				gpu.fan_perc = (int)(100.0 * gpu.fan_rpm / gpu.fan_max_rpm);
			if (gpu.fan_caps & ADL_DL_FANCTRL_SUPPORTS_PERCENT_READ)
				gpu.fan_rpm = (int)(gpu.fan_max_rpm * gpu.fan_perc / 100.0);
		}

		// Temperature
		if (ADL_OK != ADL_OD5_Temperature_Get(gpu.index, 0, &s_temp))
			printerr_status("temperature", gpu);
		else
			gpu.temperature = s_temp.iTemperature / 1000.0;

		if (args.machine)
			printf("%d\t%d\t%d\t%d\t%d\t%f\t%s\n",
				gpu.index,
				gpu.load,
				gpu.fan_rpm,
				gpu.fan_max_rpm,
				gpu.fan_perc,
				gpu.temperature,
				gpu.name
			);
		else {
			printf("Adapter %d   : %s\n",			gpu.index,
									gpu.name);
			printf("GPU load    : %3d%%\n",			gpu.load);
			printf("Fan speed   : %4d/%4d RPM (%2d%%)\n",	gpu.fan_rpm,
									gpu.fan_max_rpm,
									gpu.fan_perc);
			printf("Temperature : %6.2f C\n",		gpu.temperature);
		}

		// For now, we're only interested in a single adapter
		break;
	}

	if (gpu.active != ADL_TRUE) {
		printerr("No active AMD/ATI adapters found\n");
		retcode = 2;
	}

cleanup:
	free(lpAdapterInfo);

	if (hDLL) {
		ADL_Main_Control_Destroy();
		dlclose(hDLL);
	}
	return retcode;
}
