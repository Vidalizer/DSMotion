/*
 *  DSMotion kernel plugin
 *  Copyright (c) 2017 OperationNT
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/bt.h>
#include <psp2/motion.h>
#include <taihen.h>

//#include "log.h"
#include <string.h>
#include "../DSMotionLibrary.h"


extern unsigned int ksceKernelGetSystemTimeLow();

#define abs(x) (((x) < 0) ? -(x) : (x))

#define SONY_VID 0x054C

#define DS3_PID 0x0268

struct ds3_input_report {
	unsigned char report_id;
	unsigned char unk0;

	unsigned char select : 1;
	unsigned char l3     : 1;
	unsigned char r3     : 1;
	unsigned char start  : 1;
	unsigned char up     : 1;
	unsigned char right  : 1;
	unsigned char down   : 1;
	unsigned char left   : 1;

	unsigned char l2       : 1;
	unsigned char r2       : 1;
	unsigned char l1       : 1;
	unsigned char r1       : 1;
	unsigned char triangle : 1;
	unsigned char circle   : 1;
	unsigned char cross    : 1;
	unsigned char square   : 1;

	unsigned char ps       : 1;
	unsigned char not_used : 7;

	unsigned char unk1;

	unsigned char left_x;
	unsigned char left_y;
	unsigned char right_x;
	unsigned char right_y;

	unsigned int unk2;

	unsigned char up_sens;
	unsigned char right_sens;
	unsigned char down_sens;
	unsigned char left_sens;

	unsigned char L2_sens;
	unsigned char R2_sens;
	unsigned char L1_sens;
	unsigned char R1_sens;

	unsigned char triangle_sens;
	unsigned char circle_sens;
	unsigned char cross_sens;
	unsigned char square_sens;

	unsigned short unk3;
	unsigned char unk4;

	unsigned char status;
	unsigned char power_rating;
	unsigned char comm_status;
	unsigned int unk5;
	unsigned int unk6;
	unsigned char unk7;

	unsigned short accel_x;
	unsigned short accel_y;
	unsigned short accel_z;

	union {
		unsigned short gyro_z;
		unsigned short roll;
	};
} __attribute__((packed, aligned(32)));

#define DS4_PID   0x05C4
#define DS4_2_PID 0x09CC

struct ds4_input_report {
	unsigned char report_id;
	unsigned char left_x;
	unsigned char left_y;
	unsigned char right_x;
	unsigned char right_y;

	unsigned char dpad     : 4;
	unsigned char square   : 1;
	unsigned char cross    : 1;
	unsigned char circle   : 1;
	unsigned char triangle : 1;

	unsigned char l1      : 1;
	unsigned char r1      : 1;
	unsigned char l2      : 1;
	unsigned char r2      : 1;
	unsigned char share   : 1;
	unsigned char options : 1;
	unsigned char l3      : 1;
	unsigned char r3      : 1;

	unsigned char ps   : 1;
	unsigned char tpad : 1;
	unsigned char cnt1 : 6;

	unsigned char l_trigger;
	unsigned char r_trigger;

	unsigned char cnt2;
	unsigned char cnt3;

	unsigned char battery;

	signed short accel_x;
	signed short accel_y;
	signed short accel_z;

	union {
		signed short roll;
		signed short gyro_z;
	};
	union {
		signed short yaw;
		signed short gyro_y;
	};
	union {
		signed short pitch;
		signed short gyro_x;
	};

	unsigned char unk1[5];

	unsigned char battery_level : 4;
	unsigned char usb_plugged   : 1;
	unsigned char headphones    : 1;
	unsigned char microphone    : 1;
	unsigned char padding       : 1;

	unsigned char unk2[2];
	unsigned char trackpadpackets;
	unsigned char packetcnt;

	unsigned int finger1_id        : 7;
	unsigned int finger1_activelow : 1;
	unsigned int finger1_x         : 12;
	unsigned int finger1_y         : 12;

	unsigned int finger2_id        : 7;
	unsigned int finger2_activelow : 1;
	unsigned int finger2_x         : 12;
	unsigned int finger2_y         : 12;

} __attribute__((packed, aligned(32)));

static int ds3_connected = 0;
static struct ds3_input_report ds3_input;

static int ds4_connected = 0;
static struct ds4_input_report ds4_input;

static unsigned char* recv_buff = NULL;
static unsigned int ds_mac0 = 0;
static unsigned int ds_mac1 = 0;

#define NB_DATA 64

static struct accelGyroData previousData[NB_DATA];
static int currentData = NB_DATA-1;
static int globalCounter = 0;

unsigned int dsGetCurrentTimestamp()
{
    return ksceKernelGetSystemTimeLow();
}

unsigned int dsGetCurrentCounter()
{
    return globalCounter;
}

unsigned int dsGetSampledAccelGyro(unsigned int iSamplingTimeMS, signed short oAccel[3], signed short oGyro[3])
{
    if (!ds3_connected && !ds4_connected)
        return 0;

    int initIndex = currentData;
    struct accelGyroData* data = &previousData[initIndex];

    int accel_sum[3] = {data->accel[0], data->accel[1], data->accel[2]};
    int gyro_sum[3] = {data->gyro[0], data->gyro[1], data->gyro[2]};

    unsigned int initTime = data->timestamp;
    unsigned int samplingTimeNano = 1000 * iSamplingTimeMS;

    signed short index;
    for (index = 1; index < NB_DATA; index++)
    {
        int dataIndex = (initIndex-index+NB_DATA)%NB_DATA;
        data = &previousData[dataIndex];
        
        if (0 == data->counter || initTime-data->timestamp > samplingTimeNano)
            break;
        
        accel_sum[0] += data->accel[0];
        accel_sum[1] += data->accel[1];
        accel_sum[2] += data->accel[2];

        gyro_sum[0] += data->gyro[0];
        gyro_sum[1] += data->gyro[1];
        gyro_sum[2] += data->gyro[2];
    }
    
    signed short accel[3] = {accel_sum[0] / index, accel_sum[1] / index, accel_sum[2] / index};
    signed short gyro[3] = {gyro_sum[0] / index, gyro_sum[1] / index, gyro_sum[2] / index};

    ksceKernelMemcpyKernelToUser((uintptr_t)oAccel, (const void *)accel, 3*sizeof(signed short));
    ksceKernelMemcpyKernelToUser((uintptr_t)oGyro, (const void *)gyro, 3*sizeof(signed short));

    return index;
}

int dsGetInstantAccelGyro(unsigned int iIndex, struct accelGyroData* oData)
{
    if (!ds3_connected && !ds4_connected)
        return -1;

    int curIndex = (currentData-(iIndex%NB_DATA)+NB_DATA)%NB_DATA;
    ksceKernelMemcpyKernelToUser((uintptr_t)oData, (const void *)&previousData[curIndex], sizeof(struct accelGyroData));

    return 0;
}

static inline void ds3_input_reset(void)
{
	memset(&ds3_input, 0, sizeof(ds3_input));
}

static inline void ds4_input_reset(void)
{
	memset(&ds4_input, 0, sizeof(ds4_input));
}

static int is_ds3(const unsigned short vid_pid[2])
{
	return vid_pid[0] == SONY_VID && vid_pid[1] == DS3_PID;
}

static int is_ds4(const unsigned short vid_pid[2])
{
	return (vid_pid[0] == SONY_VID) && ((vid_pid[1] == DS4_PID) || (vid_pid[1] == DS4_2_PID));
}

#define DECL_FUNC_HOOK(name, ...) \
	static tai_hook_ref_t name##_ref; \
	static SceUID name##_hook_uid = -1; \
	static int name##_hook_func(__VA_ARGS__)

DECL_FUNC_HOOK(SceBt_ksceBtReadEvent, SceBtEvent *events, int num_events)
{
	int ret = TAI_CONTINUE(int, SceBt_ksceBtReadEvent_ref, events, num_events);

	if (ret >= 0)
    {
        for (int i = 0 ; i < num_events ; i++)
        {
            SceBtEvent* event = &events[i];
            //LOG("Connection event %d with %d %d\n", event->id, event->mac0, event->mac1);

            if (!ds3_connected && !ds4_connected && 0x05 == event->id)
            {
                unsigned short vid_pid[2];
                unsigned int result1 = ksceBtGetVidPid(event->mac0, event->mac1, vid_pid);
                //LOG("Vendor ID %d ; Product ID %d\n", vid_pid[0], vid_pid[1]);
                //log_flush();

                int connected = 0;
                if (is_ds4(vid_pid))
                {
                    ds4_input_reset();
                    ds4_connected = 1;
                    connected = 1;
                }
                else
                {
                    char name[0x79];
                    unsigned int result2 = ksceBtGetDeviceName(event->mac0, event->mac1, name);
                    if (is_ds3(vid_pid)|| (result1 == 0x802F5001 && result2 == 0x802F0C01))
                    {
                        ds3_input_reset();
                        ds3_connected = 1;
                        connected = 1;
                    }
                }
                
                if (connected)
                {
                    ds_mac0 = event->mac0;
                    ds_mac1 = event->mac1;
                    globalCounter = 0;
                }
            }
            else if ((ds3_connected || ds4_connected) && event->mac0 == ds_mac0 && event->mac1 == ds_mac1)
            {
                if (0x06 == event->id)
                {
                    ds3_connected = 0;
                    ds4_connected = 0;
                }
                else if (NULL != recv_buff)
                {
                    if (0x0A == event->id)
                    {
                        if ((ds4_connected && 0x11 == recv_buff[0]) || (ds3_connected && 0x01 == recv_buff[0]))
                        {
                            int newData = (currentData+1)%NB_DATA;
                            struct accelGyroData* data = &previousData[newData];

                            if (ds4_connected)
                            {
                                memcpy(&ds4_input, recv_buff, sizeof(ds4_input));

                                // Data from gyroscope and accelerometer seem inverted on DS4
                                data->accel[0] = ds4_input.gyro_x;
                                data->accel[1] = ds4_input.gyro_y;
                                data->accel[2] = ds4_input.gyro_z;

                                data->gyro[0] = ds4_input.accel_x;
                                data->gyro[1] = ds4_input.accel_y;
                                data->gyro[2] = ds4_input.accel_z;
                            }
                            else // if (ds3_connected)
                            {
                                memcpy(&ds3_input, recv_buff, sizeof(ds3_input));

                                // DS3 matching with DS4
                                data->accel[0] = -((signed short)ds3_input.accel_y)/4;
                                data->accel[1] = -((signed short)ds3_input.accel_z)/4;
                                data->accel[2] = ((signed short)ds3_input.accel_x)/4;

                                data->gyro[0] = 0;
                                data->gyro[1] = ((signed short)ds3_input.gyro_z+0x15FF)/10;
                                data->gyro[2] = 0;
                            }

                            data->timestamp = ksceKernelGetSystemTimeLow();
                            data->counter = (++globalCounter);
                            currentData = newData;
                        }
                        recv_buff = NULL;
                    }
                    else if (0x0B == event->id || 0x0C == event->id)
                        recv_buff = NULL;
                }
            }
        }
	}

	return ret;
}

DECL_FUNC_HOOK(SceBt_ksceBtHidTransfer, unsigned int mac0, unsigned int mac1, SceBtHidRequest *request)
{
	int ret = TAI_CONTINUE(int, SceBt_ksceBtHidTransfer_ref, mac0, mac1, request);

    if (ret >= 0 && (ds3_connected || ds4_connected) && mac0 == ds_mac0 && mac1 == ds_mac1)
    {
        if (NULL != request && NULL != request->buffer && request->length >= (ds4_connected?sizeof(ds4_input):sizeof(ds3_input)))
            recv_buff = (unsigned char*)request->buffer;
        else
            recv_buff = NULL;
    }
    
    return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));

#define BIND_FUNC_EXPORT_HOOK(name, pid, module, lib_nid, func_nid) \
	name##_hook_uid = taiHookFunctionExportForKernel((pid), \
		&name##_ref, (module), (lib_nid), (func_nid), name##_hook_func)

int module_start(SceSize argc, const void *args)
{
	int ret;
	tai_module_info_t SceBt_modinfo;

	//log_reset();
	//LOG("dsmotion kernel by OperationNT\n");

	SceBt_modinfo.size = sizeof(SceBt_modinfo);
	ret = taiGetModuleInfoForKernel(KERNEL_PID, "SceBt", &SceBt_modinfo);
	if (ret < 0) {
		//LOG("Error finding SceBt module\n");
		goto error_find_scebt;
	}

	/* SceBt hooks */
	BIND_FUNC_EXPORT_HOOK(SceBt_ksceBtReadEvent, KERNEL_PID, "SceBt", TAI_ANY_LIBRARY, 0x5ABB9A9D);
    //LOG("ksceBtReadEvent hook result: %x\n", SceBt_ksceBtReadEvent_hook_uid);

	BIND_FUNC_EXPORT_HOOK(SceBt_ksceBtHidTransfer, KERNEL_PID, "SceBt", TAI_ANY_LIBRARY, 0xF9DCEC77);
    //LOG("ksceBtHidTransfer hook result: %x\n", SceBt_ksceBtHidTransfer_hook_uid);
    
	//LOG("module_start finished successfully!\n");
    //log_flush();
    
    memset(previousData, 0, sizeof(previousData));

	return SCE_KERNEL_START_SUCCESS;

error_find_scebt:
	return SCE_KERNEL_START_FAILED;
}

#define UNBIND_FUNC_HOOK(name) \
	do { \
		if (name##_hook_uid > 0) { \
			taiHookReleaseForKernel(name##_hook_uid, name##_ref); \
		} \
	} while(0)

int module_stop(SceSize argc, const void *args)
{
	UNBIND_FUNC_HOOK(SceBt_ksceBtReadEvent);
    UNBIND_FUNC_HOOK(SceBt_ksceBtHidTransfer);

	//log_flush();

	return SCE_KERNEL_STOP_SUCCESS;
}
