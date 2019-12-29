#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "libxml/parser.h"
#include "SDL/SDL.h"
#include "ite/itp.h"
#include "ctrlboard.h"
#include "scene.h"

#ifdef _WIN32
    #include <crtdbg.h>
#else
    #include "openrtos/FreeRTOS.h"
    #include "openrtos/task.h"
#endif

extern void BackupInit(void);
extern void BackupRestore(void);
extern void BackupSyncFile(void);
extern void BackupDestroy(void);

extern mqd_t uartQueue;

//线程串口回调函数
static void* UartFunc(void* arg)
{

	//初始化一个控制板数据
	struct operate_data oper_data;
	memset(&oper_data, 0, sizeof(struct operate_data));

	//当前时间
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);



	//如果有开始时间
	if (yingxue_base.yure_begtime.tv_sec != 0){
		if ( (yingxue_base.yure_begtime.tv_sec <= cur_time.tv_sec + 60 * 2) && 
			 (yingxue_base.yure_begtime.tv_sec <= cur_time.tv_sec - 60 * 2)
			)
		{
			//发送预热命令
			oper_data.data_0 = 0xEB;
			oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
			oper_data.data_2 = 0x09;
			oper_data.data_3 = 0x02;
			oper_data.data_5 = yingxue_base.huishui_temp;
			send_uart_cmd(&oper_data);
			yingxue_base.yure_begtime.tv_sec = 0;
			yingxue_base.yure_begtime.tv_usec = 0;
		}
	}
	//结束时间
	else if (yingxue_base.yure_endtime.tv_sec != 0){
		if ((yingxue_base.yure_endtime.tv_sec <= cur_time.tv_sec + 60 * 2) &&
			(yingxue_base.yure_endtime.tv_sec <= cur_time.tv_sec - 60 * 2)
			)
		{
			//发送关闭
			oper_data.data_0 = 0xEB;
			oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
			oper_data.data_2 = 0x09;
			oper_data.data_3 = 0x00;
			oper_data.data_5 = yingxue_base.huishui_temp;
			send_uart_cmd(&oper_data);
			yingxue_base.yure_endtime.tv_sec = 0;
			yingxue_base.yure_endtime.tv_usec = 0;
		}
	}
	//ssize_t mq_receive(mqd_t msgid, char *msg, size_t msg_len, unsigned int *msg_prio)
	else if (mq_receive(uartQueue, &oper_data, sizeof(struct operate_data), 0) != -1){
		send_uart_cmd(&oper_data);
	}
	//发送应答
	else{
		oper_data.data_0 = 0xEB;
		oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
		oper_data.data_2 = 0x00;
		oper_data.data_3 = 0x00;
		send_uart_cmd(&oper_data);
	}

	

	printf("pthread run\n");
	//usleep(1000);
	return;
}

int SDL_main(int argc, char *argv[])
{

	//建立一个串口线程


	//建立一个消息
/*	struct mq_attr mq_uart_attr;
	mq_uart_attr.mq_flags = 0;
	mq_uart_attr.mq_maxmsg = 1;
	mq_uart_attr.mq_msgsize = 2;
	uartQueue = mq_open("scene", O_CREAT | O_NONBLOCK, 0644, &mq_uart_attr);


	pthread_t task;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_create(&task, &attr, UartFunc, NULL);*/
	//end

    int ret = 0;
    int restryCount = 0;
    
#ifdef CFG_LCD_MULTIPLE
    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_RESET, (void*)0);
#endif

#if !defined(CFG_LCD_INIT_ON_BOOTING) && !defined(CFG_BL_SHOW_LOGO)
    ScreenClear();
#endif

#ifdef CFG_CHECK_FILES_CRC_ON_BOOTING
    BackupInit();
retry_backup:
    ret = UpgradeInit();
    if (ret)
    {
        if (++restryCount > 2)
        {
            printf("cannot recover from backup....\n");
            goto end;
        }
        BackupRestore();
        goto retry_backup;
    }
    BackupSyncFile();
#else
    ret = UpgradeInit();
    if (ret)
        goto end;
#endif

#ifdef CFG_LCD_MULTIPLE
    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
    usleep(100000);
    ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
#endif // CFG_LCD_MULTIPLE

#ifdef	CFG_DYNAMIC_LOAD_TP_MODULE
	//This function must be in front of SDL_Init().
	DynamicLoadTpModule();
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        printf("Couldn't initialize SDL: %s\n", SDL_GetError());

    ConfigInit();

#ifdef CFG_NET_ENABLE
    NetworkInit();
    #ifdef CFG_NET_WIFI

    #else 
        WebServerInit();
    #endif
#endif // CFG_NET_ENABLE

    ScreenInit();
    ExternalInit();
#if defined(CFG_UPGRADE_FROM_UART)
	UpgradeUartInit();
#endif
    StorageInit();
    AudioInit();
    PhotoInit();

    SceneInit();
    SceneLoad();
    ret = SceneRun();

    SceneExit();

    ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_EXIT, NULL);

    PhotoExit();
    AudioExit();
#if defined(CFG_UPGRADE_FROM_UART)
	UpgradeUartExit();
#endif
    ExternalExit();

#ifdef CFG_NET_ENABLE
    if (ret != QUIT_UPGRADE_WEB)
        WebServerExit();

    xmlCleanupParser();
#endif // CFG_NET_ENABLE

    ConfigExit();
    SDL_Quit();

#ifdef CFG_CHECK_FILES_CRC_ON_BOOTING
    BackupDestroy();
#endif

#ifdef _WIN32
    _CrtDumpMemoryLeaks();
#else
    if (0)
    {
    #if (configUSE_TRACE_FACILITY == 1)
        static signed char buf[2048];
        vTaskList(buf);
        puts(buf);
    #endif
        malloc_stats();

    #ifdef CFG_DBG_RMALLOC
        Rmalloc_stat(__FILE__);
    #endif
    }
#endif // _WIN32

end:
    ret = UpgradeProcess(ret);
    itp_codec_standby();
    exit(ret);
    return ret;
}
