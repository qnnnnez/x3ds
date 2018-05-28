#include <3ds.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "ini.h"

const char *config_path = "/x3ds.ini";

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;

typedef struct
{
    u32 kDown;
    u32 kHeld;
    u32 kUp;
    circlePosition pad;
    circlePosition cstick;
    touchPosition touch;
} InputStatus;


void feedStatus(InputStatus *status)
{
	status->kDown = hidKeysDown();
	status->kHeld = hidKeysHeld();
	status->kUp = hidKeysUp();
	hidCircleRead(&(status->pad));
    hidCstickRead(&(status->cstick));
    hidTouchRead(&(status->touch));
}

typedef struct
{
    InputStatus status;
    u32 sock;
    struct sockaddr_in remote;
    Handle statusMutex;
    Handle sockMutex;
    int running;
    int samplePerSecond;
} ThreadScannerArgs;

void threadScanner(void *_args)
{
    // entry point for scanner thread
    // this thread scans input and update input status
    // and then send the status to remote host
    volatile ThreadScannerArgs *args = (ThreadScannerArgs*)_args;
    double sampleInterval = 1.0f / args->samplePerSecond;
    u64 lastSample = svcGetSystemTick();
    double clock = 268111856; // ~268mHz
	while (args->running)
    {
        svcWaitSynchronization(&args->statusMutex, U64_MAX); // just wait for mutex forever
        hidScanInput();
        feedStatus(&args->status);
        svcReleaseMutex(&args->statusMutex);
        
        svcWaitSynchronization(&args->sockMutex, U64_MAX);
        sendto(args->sock, &args->status, sizeof(args->status), 0, (struct sockaddr *)&args->remote, sizeof(args->remote));
        svcReleaseMutex(&args->sockMutex);
        
        u64 now = svcGetSystemTick();
        u64 d_ticks = now - lastSample;
        lastSample = now;
        double dt = d_ticks / clock;
        double spare = sampleInterval - dt;
        svcSleepThread(spare * 1000000000); // unit: ns
    }
}

int main(int argc, char **argv)
{
	//Matrix containing the name of each key. Useful for printing when a key is pressed
	char keysNames[32][32] = {
		"KEY_A", "KEY_B", "KEY_SELECT", "KEY_START",
		"KEY_DRIGHT", "KEY_DLEFT", "KEY_DUP", "KEY_DDOWN",
		"KEY_R", "KEY_L", "KEY_X", "KEY_Y",
		"", "", "KEY_ZL", "KEY_ZR",
		"", "", "", "",
		"KEY_TOUCH", "", "", "",
		"KEY_CSTICK_RIGHT", "KEY_CSTICK_LEFT", "KEY_CSTICK_UP", "KEY_CSTICK_DOWN",
		"KEY_CPAD_RIGHT", "KEY_CPAD_LEFT", "KEY_CPAD_UP", "KEY_CPAD_DOWN"
	};

	// Initialize services
	gfxInitDefault();

	//Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_TOP, NULL);

    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if(SOC_buffer == NULL)
    {
        printf("memalign: failed to allocate\n");
        goto exit_gfx;
    }

    int ret = socInit(SOC_buffer, SOC_BUFFERSIZE);
    if (ret != 0)
    {
        printf("socInit failed: %d\n", ret);
        goto exit_gfx;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("socket: %d %s\n", errno, strerror(errno));
        goto exit_soc;
    }

    FILE *fConfig = fopen(config_path, "r");
    if (fConfig == NULL)
    {
        printf("Cannot open file for read: %s\n", config_path);
        goto exit_soc;
    }
    fseek(fConfig, 0, SEEK_END);
    int configSize = ftell(fConfig);
    fseek(fConfig, 0, SEEK_SET);
    char *configData = (char*)malloc(configSize+1);
    fread(configData, 1, configSize, fConfig);
    configData[configSize] = '\0';
    fclose(fConfig);

    ini_t *ini = ini_load(configData, NULL);
    free(configData);
    int remote_index = ini_find_property(ini, INI_GLOBAL_SECTION, "remote", 0);
    char const *remote = ini_property_value(ini, INI_GLOBAL_SECTION, remote_index);
    int port_index = ini_find_property(ini, INI_GLOBAL_SECTION, "port", 0);
    char const *port_str = ini_property_value(ini, INI_GLOBAL_SECTION, port_index);
    int port = atoi(port_str);
    int sps_index = ini_find_property(ini, INI_GLOBAL_SECTION, "sample_per_second", 0);
    char const *sps_str = ini_property_value(ini, INI_GLOBAL_SECTION, sps_index);
    int sps = atoi(sps_str);
    printf("Sending data to: %s:%d\nsamples per second: %d\n", remote, port, sps);

    volatile ThreadScannerArgs scannerArgs;
    scannerArgs.running = 1;
    scannerArgs.samplePerSecond = sps;
    scannerArgs.sock = sock;
    memset(&scannerArgs.remote, 0 ,sizeof(scannerArgs.remote));
    scannerArgs.remote.sin_family = AF_INET;
    scannerArgs.remote.sin_port = htons(port);
    inet_pton(AF_INET, remote, &(scannerArgs.remote.sin_addr));
    svcCreateMutex(&scannerArgs.statusMutex, 0);
    svcCreateMutex(&scannerArgs.sockMutex, 0);
    Thread scanner = threadCreate(threadScanner, &scannerArgs, 40960, 0x35, -1, 1); 

	printf("Press Start+Select to exit.\n");

	// Main loop
	while (aptMainLoop())
	{
        svcWaitSynchronization(&scannerArgs.statusMutex, U64_MAX);
		if ((scannerArgs.status.kHeld & KEY_START) && (scannerArgs.status.kHeld & KEY_SELECT))
        {
            break; // return to hbmenu
        }
        printf("\r(%04d, %04d); (%04d, %04d); (%03d, %03d)",
                scannerArgs.status.pad.dx, scannerArgs.status.pad.dy,
                scannerArgs.status.cstick.dx, scannerArgs.status.cstick.dy,
                scannerArgs.status.touch.px, scannerArgs.status.touch.py);
        svcReleaseMutex(&scannerArgs.statusMutex);

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

    printf("\nExiting ...");
    scannerArgs.running = 0;
    threadJoin(scanner, U64_MAX);
    close(sock);
    svcSleepThread(2000000000);
	// Exit services
    socExit();
	gfxExit();
	return 0;

    // error happened
exit_soc:
    socExit();
exit_gfx:
	gfxExit();
    return 1;
}
