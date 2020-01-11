#include <sys/ioctl.h>
#include <sys/time.h>
#include <assert.h>
#include <math.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "SDL/SDL.h"
#include "ite/itp.h"
#include "scene.h"
#include "ctrlboard.h"

#ifdef _WIN32
#include <crtdbg.h>

#ifndef CFG_VIDEO_ENABLE
#define DISABLE_SWITCH_VIDEO_STATE
#endif
#endif // _WIN32

#ifndef CFG_POWER_WAKEUP_DOUBLE_CLICK_INTERVAL
#define DOUBLE_KEY_INTERVAL 200
#endif

#define FPS_ENABLE
#define DOUBLE_KEY_ENABLE

#define GESTURE_THRESHOLD           40
#define MAX_COMMAND_QUEUE_SIZE      8
#define MOUSEDOWN_LONGPRESS_DELAY   1000

extern ITUActionFunction actionFunctions[];
extern void resetScene(void);

//信息对象
extern mqd_t uartQueue;

//樱雪线程锁
pthread_mutex_t msg_mutex = 0;//PTHREAD_MUTEX_INITIALIZER;


unsigned long confirm_down_time = 0;
//串口的数据
struct uart_data_tag uart_data;

//樱雪显示控制数据
struct yingxue_base_tag yingxue_base;

//环形队列缓存
struct chain_list_tag chain_list;

static const unsigned short crc16tab[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

// status
static QuitValue    quitValue;
static bool         inVideoState;

// command
typedef enum
{
	CMD_NONE,
	CMD_LOAD_SCENE,
	CMD_CALL_CONNECTED,
	CMD_GOTO_MAINMENU,
	CMD_CHANGE_LANG,
	CMD_PREDRAW
} CommandID;

#define MAX_STRARG_LEN 32

typedef struct
{
	CommandID   id;
	int         arg1;
	int         arg2;
	char        strarg1[MAX_STRARG_LEN];
} Command;

static mqd_t        commandQueue = -1;

// scene
ITUScene            theScene;
static SDL_Window   *window;
static ITUSurface   *screenSurf;
static int          screenWidth;
static int          screenHeight;
static float        screenDistance;
static bool         isReady;
static int          periodPerFrame;

#if defined(CFG_USB_MOUSE) || defined(_WIN32)
static ITUIcon      *cursorIcon;
#endif

extern void ScreenSetDoubleClick(void);


//樱雪
struct main_uart_chg g_main_uart_chg_data;

//点击上下键，回调函数
//@param widget 点击空间
//@param state 0向上 1向下
static void node_widget_up_down(struct node_widget *widget, unsigned char state)
{
	struct node_widget *t_node_widget = NULL;
	struct ITUWidget *t_widget = NULL;
	char t_buf[20] = { 0 };
	int t_num = 0;

	//主页面上下调整温度
	if (yingxue_base.clock_state == 0){

		if (state == 0){
			g_main_uart_chg_data.shezhi_temp = g_main_uart_chg_data.shezhi_temp + 1;
		}
		else{
			g_main_uart_chg_data.shezhi_temp = g_main_uart_chg_data.shezhi_temp - 1;
		}

		

		t_widget = ituSceneFindWidget(&theScene, "Text17");
		sprintf(t_buf, "%d", g_main_uart_chg_data.shezhi_temp);
		ituTextSetString(t_widget, t_buf);

		send_uart_cmd(NULL);
	}else {
		if (widget->state == 1){ //如果已经锁定
			if (strcmp(widget->name, "Background2") == 0){
				t_widget = ituSceneFindWidget(&theScene, "Text3");
				t_num = atoi(ituTextGetString((ITUText*)t_widget));
				if (state == 0){
					t_num = t_num + 1;
				}
				else{
					t_num = t_num - 1;
				}
				sprintf(t_buf, "%d", t_num);
				ituTextSetString(t_widget, t_buf);
			}
			else if (strcmp(widget->name, "Background3") == 0){
				t_widget = ituSceneFindWidget(&theScene, "Text42");
				t_num = atoi(ituTextGetString((ITUText*)t_widget));
				if (state == 0){
					t_num = t_num + 1;
				}
				else{
					t_num = t_num - 1;
				}
				sprintf(t_buf, "%d", t_num);
				ituTextSetString(t_widget, t_buf);
			}
			else if (strcmp(widget->name, "Background4") == 0){

				t_widget = ituSceneFindWidget(&theScene, "Text43");
				t_num = atoi(ituTextGetString((ITUText*)t_widget));
				if (state == 0){
					t_num = t_num + 1;
				}
				else{
					t_num = t_num - 1;
				}
				sprintf(t_buf, "%d", t_num);
				ituTextSetString(t_widget, t_buf);
			}
			else if (strcmp(widget->name, "chushui_Background13") == 0){
				t_widget = ituSceneFindWidget(&theScene, "Text38");
				t_num = atoi(ituTextGetString((ITUText*)t_widget));
				if (state == 0){
					t_num = t_num + 1;
				}
				else{
					t_num = t_num - 1;
				}
				sprintf(t_buf, "%d", t_num);
				ituTextSetString(t_widget, t_buf);

			}
		}
		else{
			if (state == 0){
				if (widget->up)
					t_node_widget = widget->up;
			}
			else{
				if (widget->down){
					t_node_widget = widget->down;
				}
			}

			if (t_node_widget){
				//如果之前的控件是需要整个变换背景，
				if ((strcmp(curr_node_widget->name, "BackgroundButton47") == 0) ||
					(strcmp(curr_node_widget->name, "BackgroundButton65") == 0) ||
					(strcmp(curr_node_widget->name, "BackgroundButton60") == 0) ||
					(strcmp(curr_node_widget->name, "BackgroundButton68") == 0) ||
					(strcmp(curr_node_widget->name, "moshi_BackgroundButton10") == 0) ||
					(strcmp(curr_node_widget->name, "moshi_BackgroundButton11") == 0) ||
					(strcmp(curr_node_widget->name, "moshi_BackgroundButton12") == 0) ||
					(strcmp(curr_node_widget->name, "moshi_BackgroundButton13") == 0) ||
					(strcmp(curr_node_widget->name, "chushui_BackgroundButton73") == 0) ||
					(strcmp(curr_node_widget->name, "chushui_BackgroundButton1") == 0)


					){
					t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
					ituWidgetSetVisible(t_widget, false);
					t_widget = ituSceneFindWidget(&theScene, curr_node_widget->name);
					ituWidgetSetVisible(t_widget, true);
				}

				//如果现在的控件是需要变换背景
				if ((strcmp(t_node_widget->name, "BackgroundButton47") == 0) ||
					(strcmp(t_node_widget->name, "BackgroundButton65") == 0) ||
					(strcmp(t_node_widget->name, "BackgroundButton60") == 0) ||
					(strcmp(t_node_widget->name, "BackgroundButton68") == 0) ||
					(strcmp(t_node_widget->name, "moshi_BackgroundButton10") == 0) ||
					(strcmp(t_node_widget->name, "moshi_BackgroundButton11") == 0) ||
					(strcmp(t_node_widget->name, "moshi_BackgroundButton12") == 0) ||
					(strcmp(t_node_widget->name, "moshi_BackgroundButton13") == 0) ||
					(strcmp(t_node_widget->name, "chushui_BackgroundButton73") == 0) ||
					(strcmp(t_node_widget->name, "chushui_BackgroundButton1") == 0)
					){
					t_widget = ituSceneFindWidget(&theScene, t_node_widget->focus_back_name);
					ituWidgetSetVisible(t_widget, true);
					t_widget = ituSceneFindWidget(&theScene, t_node_widget->name);
					ituWidgetSetVisible(t_widget, false);

					//原来的控件去掉边框
					//原来控件是radio
					if (strcmp(curr_node_widget->focus_back_name, "radio") == 0){
						t_widget = ituSceneFindWidget(&theScene, curr_node_widget->name);
						ituWidgetSetActive(t_widget, false);
					}
					//控件普通
					else{
						t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
						ituWidgetSetVisible(t_widget, false);
					}
					curr_node_widget = t_node_widget;
				}
				else if (strcmp(t_node_widget->focus_back_name, "radio") == 0){
					t_widget = ituSceneFindWidget(&theScene, t_node_widget->name);
					ituFocusWidget(t_widget);
					curr_node_widget = t_node_widget;
				}
				else{
					//隐藏当前控件选中状态
					t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
					ituWidgetSetVisible(t_widget, false);
					//显示当前的控件
					t_widget = ituSceneFindWidget(&theScene, t_node_widget->focus_back_name);
					ituWidgetSetVisible(t_widget, true);
					curr_node_widget = t_node_widget;
				}
			}
		}
	
	}
	
	
	


}

//点击回调事件
//@param widget 点击空间
//@param state 
static void main_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	if (strcmp(widget->name, "BackgroundButton47") == 0){
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "yureSprite") == 0){
		t_widget = ituSceneFindWidget(&theScene, "yureLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "moshiSprite") == 0){
		t_widget = ituSceneFindWidget(&theScene, "moshiLayer");
		ituLayerGoto((ITULayer *)t_widget);


	}
	//点击单项按键
	else if (strcmp(widget->focus_back_name, "radio") == 0){
		//如果两次点击都是同一，取消
		if (widget == yingxue_base.yure_time_widget){
			t_widget = ituSceneFindWidget(&theScene, widget->name);
			ituCheckBoxSetChecked((ITUCheckBox *)t_widget, false);
			yingxue_base.yure_time_widget = NULL;
		}
		//如果不一样，先去掉以前的状态
		else{
			if (yingxue_base.yure_time_widget){
				t_widget = ituSceneFindWidget(&theScene, yingxue_base.yure_time_widget->name);
				ituCheckBoxSetChecked((ITUCheckBox *)t_widget, false);
			}
			t_widget = ituSceneFindWidget(&theScene, widget->name);
			ituCheckBoxSetChecked((ITUCheckBox *)t_widget, true);
		}
		yingxue_base.yure_time_widget = widget;
	}
	//支持长按
	else if (widget->type == 1){
		if (widget->state == 0){
			//锁定
			widget->state = 1;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, true);
		}
		else{
			//解除锁定
			widget->state = 0;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, false);
		}
	}
}

//预热点击事件
static void yure_node_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	if (strcmp(widget->name, "BackgroundButton47") == 0){
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//Background27
	else if (strcmp(widget->name, "BackgroundButton14") == 0){
		//单次巡航 从现在到2个小时结束
		yingxue_base.yure_mode = 1;
		gettimeofday(&yingxue_base.yure_begtime, NULL);
		//2个小时 
		yingxue_base.yure_endtime.tv_sec = yingxue_base.yure_begtime.tv_sec + 60 * 60 * 2;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "BackgroundButton19") == 0){
		//全天候模式：
		//启动：从点击巡航模式，即刻开始，发送一次串口命令：预热中的数据2“循环预热”
		yingxue_base.yure_mode = 2;
		gettimeofday(&yingxue_base.yure_begtime, NULL);
		yingxue_base.yure_endtime.tv_sec = 9999;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//BackgroundButton20
	else if (strcmp(widget->name, "BackgroundButton20") == 0){
		/*
		预约模式：
		启动：定时时间到达，发送一次串口命令：预热中的数据2“循环预热”，
		结束：自动延迟1个小时，发送一次串口命令，TFT在发送 预热命令  0- 预热关闭是吧
		*/
		yingxue_base.yure_mode = 3;
		gettimeofday(&yingxue_base.yure_begtime, NULL);
		struct tm *tm;
		tm = localtime(&yingxue_base.yure_begtime.tv_sec);
		tm->tm_hour = yingxue_base.yure_set_count;
		yingxue_base.yure_endtime.tv_sec = mktime(tm);
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//设置预热时间
	else if (strcmp(widget->name, "BackgroundButton2") == 0){
		//设置预约时间
		t_widget = ituSceneFindWidget(&theScene, "yureshijianLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//设置预热回水温度和北京时间
	else if (strcmp(widget->name, "BackgroundButton21") == 0){
		t_widget = ituSceneFindWidget(&theScene, "yureshezhiLayer");
		ituLayerGoto((ITULayer *)t_widget);

	}
}

//预热时间设置事件
static void yure_settime_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	if (strcmp(widget->name, "BackgroundButton65") == 0){
		t_widget = ituSceneFindWidget(&theScene, "yureLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//点击单项按键
	else if (strcmp(widget->focus_back_name, "radio") == 0){
		//如果两次点击都是同一，取消
		if (widget == yingxue_base.yure_time_widget){
			t_widget = ituSceneFindWidget(&theScene, widget->name);
			ituCheckBoxSetChecked((ITUCheckBox *)t_widget, false);
			yingxue_base.yure_time_widget = NULL;
			yingxue_base.yure_set_count = 0;
		}
		//如果不一样，先去掉以前的状态
		else{
			if (yingxue_base.yure_time_widget){
				t_widget = ituSceneFindWidget(&theScene, yingxue_base.yure_time_widget->name);
				ituCheckBoxSetChecked((ITUCheckBox *)t_widget, false);
			}
			t_widget = ituSceneFindWidget(&theScene, widget->name);
			ituCheckBoxSetChecked((ITUCheckBox *)t_widget, true);
			yingxue_base.yure_set_count = widget->value;
		}
		yingxue_base.yure_time_widget = widget;
	}
	//支持长按
	else if (widget->type == 1){
		if (widget->state == 0){
			//锁定
			widget->state = 1;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, true);
		}
		else{
			//解除锁定
			widget->state = 0;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, false);
		}
	}
}

//预热回水温度和北京时间
static void yure_yureshezhiLayer_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	char *t_buf = NULL;
	unsigned char num = 0;
	if (strcmp(widget->name, "BackgroundButton60") == 0){
		t_widget = ituSceneFindWidget(&theScene, "yureLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	//支持长按
	else if (widget->type == 1){
		if (widget->state == 0){
			//锁定
			widget->state = 1;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, true);
		}
		else{
			//解除锁定Background2 Background3 Background4
			//改变数据
			//回水温度
			if (strcmp(widget->name, "Background2") == 0){
				t_widget = ituSceneFindWidget(&theScene, "Text3");
				t_buf = ituTextGetString(t_widget);
				num = atoi(t_buf);
				yingxue_base.huishui_temp = num;
			}
			//北京时间小时
			else if ((strcmp(widget->name, "Background3") == 0) || (strcmp(widget->name, "Background4") == 0)){
				struct timeval curr_time;
				struct tm *t_tm;
				gettimeofday(&curr_time, NULL);
				t_tm = localtime(&curr_time);
				if (strcmp(widget->name, "Background3") == 0){
					t_widget = ituSceneFindWidget(&theScene, "Text42");
					t_buf = ituTextGetString(t_widget);
					num = atoi(t_buf);
					t_tm->tm_hour = num;
				}
				else{
					t_widget = ituSceneFindWidget(&theScene, "Text43");
					t_buf = ituTextGetString(t_widget);
					num = atoi(t_buf);
					t_tm->tm_min = num;
				}
				curr_time.tv_sec = mktime(t_tm);
				settimeofday(&curr_time, NULL);

			}
			widget->state = 0;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, false);
		}
	}
}

//模式设置回调事件
static void moshi_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	//初始化一个控制板数据
	struct operate_data oper_data;

	ITUWidget *t_widget = NULL;
	if (strcmp(widget->name, "BackgroundButton68") == 0){
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton10") == 0){
		//发送模式命令就发指令 4 ： 模式设置  ： 默认 0 ，设置温度 ： XX ， 定升设定  ： 默认值时发0 
		memset(&oper_data, 0, sizeof(struct operate_data));
		oper_data.data_0 = 0xEB;
		oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
		oper_data.data_2 = 0x04;
		oper_data.data_3 = 0x00;
		oper_data.data_4 = yingxue_base.normal_moshi.temp;
		struct timespec tm;
		memset(&tm, 0, sizeof(struct timespec));
		tm.tv_sec += 2;
		mq_timedsend(uartQueue, &oper_data, sizeof(struct operate_data), 1, &tm);

		yingxue_base.moshi_mode = 1;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton11") == 0){

		//发送模式命令就发指令 4 ： 模式设置  ： 默认 0 ，设置温度 ： XX ， 定升设定  ： 默认值时发0 
		memset(&oper_data, 0, sizeof(struct operate_data));
		oper_data.data_0 = 0xEB;
		oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
		oper_data.data_2 = 0x04;
		oper_data.data_3 = 0x00;
		oper_data.data_4 = yingxue_base.super_moshi.temp;
		struct timespec tm;
		memset(&tm, 0, sizeof(struct timespec));
		tm.tv_sec += 2;
		mq_timedsend(uartQueue, &oper_data, sizeof(struct operate_data), 1, &tm);

		yingxue_base.moshi_mode = 2;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton12") == 0){

		//发送模式命令就发指令 4 ： 模式设置  ： 默认 0 ，设置温度 ： XX ， 定升设定  ： 默认值时发0 
		memset(&oper_data, 0, sizeof(struct operate_data));
		oper_data.data_0 = 0xEB;
		oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
		oper_data.data_2 = 0x04;
		oper_data.data_3 = 0x00;
		oper_data.data_4 = yingxue_base.eco_moshi.temp;
		struct timespec tm;
		memset(&tm, 0, sizeof(struct timespec));
		tm.tv_sec += 2;
		mq_timedsend(uartQueue, &oper_data, sizeof(struct operate_data), 1, &tm);

		yingxue_base.moshi_mode = 3;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton13") == 0){

		//发送模式命令就发指令 4 ： 模式设置  ： 默认 0 ，设置温度 ： XX ， 定升设定  ： 默认值时发0 
		memset(&oper_data, 0, sizeof(struct operate_data));
		oper_data.data_0 = 0xEB;
		oper_data.data_1 = 0x03 << 5 | 0x07 << 2 | 0x01;
		oper_data.data_2 = 0x04;
		oper_data.data_3 = 0x00;
		oper_data.data_4 = yingxue_base.fruit_moshi.temp;
		struct timespec tm;
		memset(&tm, 0, sizeof(struct timespec));
		tm.tv_sec += 2;
		mq_timedsend(uartQueue, &oper_data, sizeof(struct operate_data), 1, &tm);

		yingxue_base.moshi_mode = 4;
		t_widget = ituSceneFindWidget(&theScene, "MainLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
}


//出水回调事件
static void chushui_widget_confirm_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	char *t_buf;
	int num = 0;
	t_widget = ituSceneFindWidget(&theScene, "moshiLayer");
	if (strcmp(widget->name, "chushui_BackgroundButton73") == 0){
		ituLayerGoto((ITULayer *)t_widget);
	}
	//支持长按
	else if (widget->type == 1){
		if (widget->state == 0){
			//锁定
			widget->state = 1;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, true);
		}
		else{
			//解除锁定
			widget->state = 0;
			t_widget = ituSceneFindWidget(&theScene, widget->checked_back_name);
			ituWidgetSetVisible(t_widget, false);
		}
	}
	else if (strcmp(widget->name, "chushui_BackgroundButton1") == 0){
		//Text38
		t_widget = ituSceneFindWidget(&theScene, "Text38");
		t_buf = ituTextGetString((ITUText*)t_widget);
		num = atoi(t_buf);
		if (yingxue_base.select_set_moshi_mode > 0){
			if (yingxue_base.select_set_moshi_mode == 1){
				yingxue_base.normal_moshi.temp = num;
			}
			else if (yingxue_base.select_set_moshi_mode == 2){
				yingxue_base.super_moshi.temp = num;
			}
			else if (yingxue_base.select_set_moshi_mode == 3){
				yingxue_base.eco_moshi.temp = num;
			}
			else if (yingxue_base.select_set_moshi_mode == 4){
				yingxue_base.fruit_moshi.temp = num;
			}
		}
		t_widget = ituSceneFindWidget(&theScene, "moshiLayer");
		ituLayerGoto((ITULayer *)t_widget);
	}
}


//长按模式
static void moshi_widget_longpress_cb(struct node_widget *widget, unsigned char state)
{
	ITUWidget *t_widget = NULL;
	t_widget = ituSceneFindWidget(&theScene, "chushui");
	//chushui
	if (strcmp(widget->name, "moshi_BackgroundButton10") == 0){
		yingxue_base.select_set_moshi_mode = 1;
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton11") == 0){
		yingxue_base.select_set_moshi_mode = 2;
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton12") == 0){
		yingxue_base.select_set_moshi_mode = 3;
	}
	else if (strcmp(widget->name, "moshi_BackgroundButton13") == 0){
		yingxue_base.select_set_moshi_mode = 4;
	}
	ituLayerGoto(t_widget);
}


//串口消息
mqd_t uartQueue = -1;

struct main_data g_main_data;


//樱雪基础数据
//struct yingxue_base_tag yingxue_base;

//当前选中的控件
struct node_widget *curr_node_widget;

//主界面
struct node_widget mainlayer_0;
struct node_widget mainlayer_1;
struct node_widget mainlayer_2;


//预热界面
struct node_widget yureLayer_0;
struct node_widget yureLayer_1;
struct node_widget yureLayer_2;
struct node_widget yureLayer_3;
struct node_widget yureLayer_4;
struct node_widget yureLayer_5;


struct node_widget yureshijian_widget_0; //预热时间控制控件1
struct node_widget yureshijian_widget_num_1; //预热时间控制控件2
struct node_widget yureshijian_widget_num_2; //预热时间控制控件3
struct node_widget yureshijian_widget_num_3; //预热时间控制控件4
struct node_widget yureshijian_widget_num_4; //预热时间控制控件5
struct node_widget yureshijian_widget_num_5; //预热时间控制控件6
struct node_widget yureshijian_widget_num_6; //预热时间控制控件2
struct node_widget yureshijian_widget_num_7; //预热时间控制控件3
struct node_widget yureshijian_widget_num_8; //预热时间控制控件4
struct node_widget yureshijian_widget_num_9; //预热时间控制控件5
struct node_widget yureshijian_widget_num_10; //预热时间控制控件6
struct node_widget yureshijian_widget_num_11; //预热时间控制控件2
struct node_widget yureshijian_widget_num_12; //预热时间控制控件3
struct node_widget yureshijian_widget_num_13; //预热时间控制控件4
struct node_widget yureshijian_widget_num_14; //预热时间控制控件5
struct node_widget yureshijian_widget_num_15; //预热时间控制控件6
struct node_widget yureshijian_widget_num_16; //预热时间控制控件2
struct node_widget yureshijian_widget_num_17; //预热时间控制控件3
struct node_widget yureshijian_widget_num_18; //预热时间控制控件4
struct node_widget yureshijian_widget_num_19; //预热时间控制控件5
struct node_widget yureshijian_widget_num_20; //预热时间控制控件6
struct node_widget yureshijian_widget_num_21; //预热时间控制控件6
struct node_widget yureshijian_widget_num_22; //预热时间控制控件6
struct node_widget yureshijian_widget_num_23; //预热时间控制控件6
struct node_widget yureshijian_widget_num_24; //预热时间控制控件6

//预约时间
struct node_widget yureshezhiLayer_0;
struct node_widget yureshezhiLayer_1;
struct node_widget yureshezhiLayer_2;
struct node_widget yureshezhiLayer_3;

//模式
struct node_widget moshiLayer_0;
struct node_widget moshiLayer_1;
struct node_widget moshiLayer_2;
struct node_widget moshiLayer_3;
struct node_widget moshiLayer_4;

//出水模式
struct node_widget chushui_0;
struct node_widget chushui_1;
struct node_widget chushui_2;

//发送串口命令
void send_uart_cmd(struct operate_data *var_opt_data)
{
	printf("send to uart\n");
	return;
}

//预热时间
static void yure_settime_init()
{

	yureshijian_widget_0.up = NULL;
	yureshijian_widget_0.down = &yureshijian_widget_num_1;
	yureshijian_widget_0.focus_back_name = "BackgroundButton30"; //选中
	yureshijian_widget_0.name = "BackgroundButton65";//未选中
	yureshijian_widget_0.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_0.updown_cb = node_widget_up_down;



	yureshijian_widget_num_1.value = 1;
	yureshijian_widget_num_1.up = &yureshijian_widget_0;
	yureshijian_widget_num_1.down = &yureshijian_widget_num_2;
	yureshijian_widget_num_1.focus_back_name = "radio";
	yureshijian_widget_num_1.name = "RadioBox2";
	yureshijian_widget_num_1.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_1.updown_cb = node_widget_up_down;

	yureshijian_widget_num_2.value = 2;
	yureshijian_widget_num_2.up = &yureshijian_widget_num_1;
	yureshijian_widget_num_2.down = &yureshijian_widget_num_3;
	yureshijian_widget_num_2.focus_back_name = "radio";
	yureshijian_widget_num_2.name = "RadioBox5";
	yureshijian_widget_num_2.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_2.updown_cb = node_widget_up_down;

	yureshijian_widget_num_3.value = 3;
	yureshijian_widget_num_3.up = &yureshijian_widget_num_2;
	yureshijian_widget_num_3.down = &yureshijian_widget_num_4;
	yureshijian_widget_num_3.focus_back_name = "radio";
	yureshijian_widget_num_3.name = "RadioBox31";
	yureshijian_widget_num_3.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_3.updown_cb = node_widget_up_down;

	yureshijian_widget_num_4.value = 4;
	yureshijian_widget_num_4.up = &yureshijian_widget_num_3;
	yureshijian_widget_num_4.down = &yureshijian_widget_num_5;
	yureshijian_widget_num_4.focus_back_name = "radio";
	yureshijian_widget_num_4.name = "RadioBox32";
	yureshijian_widget_num_4.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_4.updown_cb = node_widget_up_down;

	yureshijian_widget_num_5.value = 5;
	yureshijian_widget_num_5.up = &yureshijian_widget_num_4;
	yureshijian_widget_num_5.down = &yureshijian_widget_num_6;
	yureshijian_widget_num_5.focus_back_name = "radio";
	yureshijian_widget_num_5.name = "RadioBox59";
	yureshijian_widget_num_5.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_5.updown_cb = node_widget_up_down;

	yureshijian_widget_num_6.value = 6;
	yureshijian_widget_num_6.up = &yureshijian_widget_num_5;
	yureshijian_widget_num_6.down = &yureshijian_widget_num_7;
	yureshijian_widget_num_6.focus_back_name = "radio";
	yureshijian_widget_num_6.name = "RadioBox58";
	yureshijian_widget_num_6.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_6.updown_cb = node_widget_up_down;

	yureshijian_widget_num_7.value = 7;
	yureshijian_widget_num_7.up = &yureshijian_widget_num_6;
	yureshijian_widget_num_7.down = &yureshijian_widget_num_8;
	yureshijian_widget_num_7.focus_back_name = "radio";
	yureshijian_widget_num_7.name = "RadioBox57";
	yureshijian_widget_num_7.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_7.updown_cb = node_widget_up_down;

	yureshijian_widget_num_8.value = 8;
	yureshijian_widget_num_8.up = &yureshijian_widget_num_7;
	yureshijian_widget_num_8.down = &yureshijian_widget_num_9;
	yureshijian_widget_num_8.focus_back_name = "radio";
	yureshijian_widget_num_8.name = "RadioBox56";
	yureshijian_widget_num_8.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_8.updown_cb = node_widget_up_down;

	yureshijian_widget_num_9.value = 9;
	yureshijian_widget_num_9.up = &yureshijian_widget_num_8;
	yureshijian_widget_num_9.down = &yureshijian_widget_num_10;
	yureshijian_widget_num_9.focus_back_name = "radio";
	yureshijian_widget_num_9.name = "RadioBox75";
	yureshijian_widget_num_9.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_9.updown_cb = node_widget_up_down;


	yureshijian_widget_num_10.value = 10;
	yureshijian_widget_num_10.up = &yureshijian_widget_num_9;
	yureshijian_widget_num_10.down = &yureshijian_widget_num_11;
	yureshijian_widget_num_10.focus_back_name = "radio";
	yureshijian_widget_num_10.name = "RadioBox74";
	yureshijian_widget_num_10.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_10.updown_cb = node_widget_up_down;

	yureshijian_widget_num_11.value = 11;
	yureshijian_widget_num_11.up = &yureshijian_widget_num_10;
	yureshijian_widget_num_11.down = &yureshijian_widget_num_12;
	yureshijian_widget_num_11.focus_back_name = "radio";
	yureshijian_widget_num_11.name = "RadioBox73";
	yureshijian_widget_num_11.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_11.updown_cb = node_widget_up_down;

	yureshijian_widget_num_12.value = 12;
	yureshijian_widget_num_12.up = &yureshijian_widget_num_11;
	yureshijian_widget_num_12.down = &yureshijian_widget_num_13;
	yureshijian_widget_num_12.focus_back_name = "radio";
	yureshijian_widget_num_12.name = "RadioBox72";
	yureshijian_widget_num_12.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_12.updown_cb = node_widget_up_down;

	yureshijian_widget_num_13.value = 13;
	yureshijian_widget_num_13.up = &yureshijian_widget_num_12;
	yureshijian_widget_num_13.down = &yureshijian_widget_num_14;
	yureshijian_widget_num_13.focus_back_name = "radio";
	yureshijian_widget_num_13.name = "RadioBox67";
	yureshijian_widget_num_13.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_13.updown_cb = node_widget_up_down;

	yureshijian_widget_num_14.value = 14;
	yureshijian_widget_num_14.up = &yureshijian_widget_num_13;
	yureshijian_widget_num_14.down = &yureshijian_widget_num_15;
	yureshijian_widget_num_14.focus_back_name = "radio";
	yureshijian_widget_num_14.name = "RadioBox66";
	yureshijian_widget_num_14.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_14.updown_cb = node_widget_up_down;

	yureshijian_widget_num_15.value = 15;
	yureshijian_widget_num_15.up = &yureshijian_widget_num_14;
	yureshijian_widget_num_15.down = &yureshijian_widget_num_16;
	yureshijian_widget_num_15.focus_back_name = "radio";
	yureshijian_widget_num_15.name = "RadioBox65";
	yureshijian_widget_num_15.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_15.updown_cb = node_widget_up_down;

	yureshijian_widget_num_16.value = 16;
	yureshijian_widget_num_16.up = &yureshijian_widget_num_15;
	yureshijian_widget_num_16.down = &yureshijian_widget_num_17;
	yureshijian_widget_num_16.focus_back_name = "radio";
	yureshijian_widget_num_16.name = "RadioBox64";
	yureshijian_widget_num_16.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_16.updown_cb = node_widget_up_down;

	yureshijian_widget_num_17.value = 17;
	yureshijian_widget_num_17.up = &yureshijian_widget_num_16;
	yureshijian_widget_num_17.down = &yureshijian_widget_num_18;
	yureshijian_widget_num_17.focus_back_name = "radio";
	yureshijian_widget_num_17.name = "RadioBox92";
	yureshijian_widget_num_17.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_17.updown_cb = node_widget_up_down;

	yureshijian_widget_num_18.value = 18;
	yureshijian_widget_num_18.up = &yureshijian_widget_num_17;
	yureshijian_widget_num_18.down = &yureshijian_widget_num_19;
	yureshijian_widget_num_18.focus_back_name = "radio";
	yureshijian_widget_num_18.name = "RadioBox91";
	yureshijian_widget_num_18.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_18.updown_cb = node_widget_up_down;

	yureshijian_widget_num_19.value = 19;
	yureshijian_widget_num_19.up = &yureshijian_widget_num_18;
	yureshijian_widget_num_19.down = &yureshijian_widget_num_20;
	yureshijian_widget_num_19.focus_back_name = "radio";
	yureshijian_widget_num_19.name = "RadioBox90";
	yureshijian_widget_num_19.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_19.updown_cb = node_widget_up_down;

	yureshijian_widget_num_20.value = 20;
	yureshijian_widget_num_20.up = &yureshijian_widget_num_19;
	yureshijian_widget_num_20.down = &yureshijian_widget_num_21;
	yureshijian_widget_num_20.focus_back_name = "radio";
	yureshijian_widget_num_20.name = "RadioBox89";
	yureshijian_widget_num_20.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_20.updown_cb = node_widget_up_down;

	yureshijian_widget_num_21.value = 21;
	yureshijian_widget_num_21.up = &yureshijian_widget_num_20;
	yureshijian_widget_num_21.down = &yureshijian_widget_num_22;
	yureshijian_widget_num_21.focus_back_name = "radio";
	yureshijian_widget_num_21.name = "RadioBox83";
	yureshijian_widget_num_21.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_21.updown_cb = node_widget_up_down;

	yureshijian_widget_num_22.value = 22;
	yureshijian_widget_num_22.up = &yureshijian_widget_num_21;
	yureshijian_widget_num_22.down = &yureshijian_widget_num_23;
	yureshijian_widget_num_22.focus_back_name = "radio";
	yureshijian_widget_num_22.name = "RadioBox82";
	yureshijian_widget_num_22.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_22.updown_cb = node_widget_up_down;

	yureshijian_widget_num_23.value = 23;
	yureshijian_widget_num_23.up = &yureshijian_widget_num_22;
	yureshijian_widget_num_23.down = &yureshijian_widget_num_24;
	yureshijian_widget_num_23.focus_back_name = "radio";
	yureshijian_widget_num_23.name = "RadioBox81";
	yureshijian_widget_num_23.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_23.updown_cb = node_widget_up_down;

	yureshijian_widget_num_24.value = 24;
	yureshijian_widget_num_24.up = &yureshijian_widget_num_23;
	yureshijian_widget_num_24.down = NULL;
	yureshijian_widget_num_24.focus_back_name = "radio";
	yureshijian_widget_num_24.name = "RadioBox80";
	yureshijian_widget_num_24.confirm_cb = yure_settime_widget_confirm_cb;
	yureshijian_widget_num_24.updown_cb = node_widget_up_down;
}

//初始化控件
static void
node_widget_init(void)
{
	curr_node_widget = NULL;
	//主页面
	mainlayer_0.up = NULL;
	mainlayer_0.down = &mainlayer_1;
	mainlayer_0.focus_back_name = "Background100";
	mainlayer_0.name = "yureSprite";
	mainlayer_0.confirm_cb = main_widget_confirm_cb;
	mainlayer_0.updown_cb = node_widget_up_down;

	mainlayer_1.up = &mainlayer_0;
	mainlayer_1.down = &mainlayer_2;
	mainlayer_1.focus_back_name = "Background134";
	mainlayer_1.name = "BackgroundButton3";
	mainlayer_1.confirm_cb = main_widget_confirm_cb;
	mainlayer_1.updown_cb = node_widget_up_down;

	mainlayer_2.up = &mainlayer_1;
	mainlayer_2.down = NULL;
	mainlayer_2.focus_back_name = "Background102";
	mainlayer_2.name = "moshiSprite";
	mainlayer_2.confirm_cb = main_widget_confirm_cb;
	mainlayer_2.updown_cb = node_widget_up_down;


	//预热界面
	yureLayer_0.up = NULL;
	yureLayer_0.down = &yureLayer_1;
	yureLayer_0.focus_back_name = "BackgroundButton78";
	yureLayer_0.name = "BackgroundButton47";
	yureLayer_0.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_0.updown_cb = node_widget_up_down;

	yureLayer_1.up = &yureLayer_0;
	yureLayer_1.down = &yureLayer_2;
	yureLayer_1.focus_back_name = "Background27";
	yureLayer_1.name = "BackgroundButton14";
	yureLayer_1.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_1.updown_cb = node_widget_up_down;

	yureLayer_2.up = &yureLayer_1;
	yureLayer_2.down = &yureLayer_3;
	yureLayer_2.focus_back_name = "Background30";
	yureLayer_2.name = "BackgroundButton19";
	yureLayer_2.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_2.updown_cb = node_widget_up_down;


	yureLayer_3.up = &yureLayer_2;
	yureLayer_3.down = &yureLayer_4;
	yureLayer_3.focus_back_name = "Background132";
	yureLayer_3.name = "BackgroundButton20";
	yureLayer_3.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_3.updown_cb = node_widget_up_down;

	yureLayer_4.up = &yureLayer_3;
	yureLayer_4.down = &yureLayer_5;
	yureLayer_4.focus_back_name = "Background94";
	yureLayer_4.name = "BackgroundButton2";
	yureLayer_4.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_4.updown_cb = node_widget_up_down;

	yureLayer_5.up = &yureLayer_4;
	yureLayer_5.down = NULL;
	yureLayer_5.focus_back_name = "Background46";
	yureLayer_5.name = "BackgroundButton21";
	yureLayer_5.confirm_cb = yure_node_widget_confirm_cb;
	yureLayer_5.updown_cb = node_widget_up_down;

	//预约时间
	yureshezhiLayer_0.up = NULL;
	yureshezhiLayer_0.down = &yureshezhiLayer_1;
	yureshezhiLayer_0.focus_back_name = "BackgroundButton85";
	yureshezhiLayer_0.name = "BackgroundButton60";
	yureshezhiLayer_0.confirm_cb = yure_yureshezhiLayer_widget_confirm_cb;
	yureshezhiLayer_0.updown_cb = node_widget_up_down;

	yureshezhiLayer_1.up = &yureshezhiLayer_0;
	yureshezhiLayer_1.down = &yureshezhiLayer_2;
	yureshezhiLayer_1.focus_back_name = "Background37";
	yureshezhiLayer_1.checked_back_name = "Background45";
	yureshezhiLayer_1.name = "Background2";
	yureshezhiLayer_1.confirm_cb = yure_yureshezhiLayer_widget_confirm_cb;
	yureshezhiLayer_1.updown_cb = node_widget_up_down;
	yureshezhiLayer_1.type = 1;

	yureshezhiLayer_2.up = &yureshezhiLayer_1;
	yureshezhiLayer_2.down = &yureshezhiLayer_3;
	yureshezhiLayer_2.focus_back_name = "Background33";
	yureshezhiLayer_2.checked_back_name = "Background105";
	yureshezhiLayer_2.name = "Background3";
	yureshezhiLayer_2.confirm_cb = yure_yureshezhiLayer_widget_confirm_cb;
	yureshezhiLayer_2.updown_cb = node_widget_up_down;
	yureshezhiLayer_2.type = 1;


	yureshezhiLayer_3.up = &yureshezhiLayer_2;
	yureshezhiLayer_3.down = NULL;
	yureshezhiLayer_3.focus_back_name = "Background40";
	yureshezhiLayer_3.checked_back_name = "Background107";
	yureshezhiLayer_3.name = "Background4";
	yureshezhiLayer_3.confirm_cb = yure_yureshezhiLayer_widget_confirm_cb;
	yureshezhiLayer_3.updown_cb = node_widget_up_down;
	yureshezhiLayer_3.type = 1;


	//模式
	moshiLayer_0.up = NULL;
	moshiLayer_0.down = &moshiLayer_1;
	moshiLayer_0.focus_back_name = "BackgroundButton33";
	moshiLayer_0.name = "BackgroundButton68";
	moshiLayer_0.confirm_cb = moshi_widget_confirm_cb;
	moshiLayer_0.updown_cb = node_widget_up_down;

	moshiLayer_1.up = &moshiLayer_0;
	moshiLayer_1.down = &moshiLayer_2;
	moshiLayer_1.focus_back_name = "moshi_BackgroundButton80";
	moshiLayer_1.name = "moshi_BackgroundButton10";
	moshiLayer_1.confirm_cb = moshi_widget_confirm_cb;
	moshiLayer_1.updown_cb = node_widget_up_down;
	moshiLayer_1.long_press_cb = moshi_widget_longpress_cb;

	moshiLayer_2.up = &moshiLayer_1;
	moshiLayer_2.down = &moshiLayer_3;
	moshiLayer_2.focus_back_name = "moshi_BackgroundButton79";
	moshiLayer_2.name = "moshi_BackgroundButton11";
	moshiLayer_2.confirm_cb = moshi_widget_confirm_cb;
	moshiLayer_2.updown_cb = node_widget_up_down;
	moshiLayer_2.long_press_cb = moshi_widget_longpress_cb;

	moshiLayer_3.up = &moshiLayer_2;
	moshiLayer_3.down = &moshiLayer_4;
	moshiLayer_3.focus_back_name = "moshi_BackgroundButton81";
	moshiLayer_3.name = "moshi_BackgroundButton12";
	moshiLayer_3.confirm_cb = moshi_widget_confirm_cb;
	moshiLayer_3.updown_cb = node_widget_up_down;
	moshiLayer_3.long_press_cb = moshi_widget_longpress_cb;

	moshiLayer_4.up = &moshiLayer_3;
	moshiLayer_4.down = NULL;
	moshiLayer_4.focus_back_name = "moshi_BackgroundButton82";
	moshiLayer_4.name = "moshi_BackgroundButton13";
	moshiLayer_4.confirm_cb = moshi_widget_confirm_cb;
	moshiLayer_4.updown_cb = node_widget_up_down;
	moshiLayer_4.long_press_cb = moshi_widget_longpress_cb;

	//出水
	chushui_0.up = NULL;
	chushui_0.down = &chushui_1;
	chushui_0.focus_back_name = "chushui_BackgroundButton7";
	chushui_0.name = "chushui_BackgroundButton73";
	chushui_0.confirm_cb = chushui_widget_confirm_cb;
	chushui_0.updown_cb = node_widget_up_down;

	chushui_1.up = &chushui_0;
	chushui_1.down = &chushui_2;
	chushui_1.focus_back_name = "chushui_Background37";
	chushui_1.checked_back_name = "chushui_Background45";
	chushui_1.name = "chushui_Background13";
	chushui_1.confirm_cb = chushui_widget_confirm_cb;
	chushui_1.updown_cb = node_widget_up_down;
	chushui_1.type = 1;

	chushui_2.up = &chushui_1;
	chushui_2.down = NULL;
	chushui_2.focus_back_name = "chushui_Background51";
	chushui_2.name = "chushui_BackgroundButton1";
	chushui_2.confirm_cb = chushui_widget_confirm_cb;
	chushui_2.updown_cb = node_widget_up_down;



	//预热时间
	yure_settime_init();
}
//接受主板来的命令 0成功 1未完成 -1失败
char recv_uart_cmd()
{
	g_main_data.state = 16;

	//第0针
	g_main_data.data[0] = 0x00;
	g_main_data.data[1] = 0x00;
	//帧
	g_main_data.data[2] = 0x00 << 4 | 0x11;
	//数据
	g_main_data.data[3] = 0x00;
	//水流显示	Bit4	1 — 有水    
	//风机显示	Bit5	1 — 风机开
	//火焰显示	Bit6	1 — 有火
	g_main_data.data[4] = 0x00;
	//[0][5]	出水温度   6
	g_main_data.data[8] = 0x11;
	//[0][8]   错误代码 故障代码故障状态([0][1].2=1) — 故障代码
	g_main_data.data[11] = 0x11;
	return 0;
}






void SceneInit(void)
{
	struct mq_attr  attr;
	ITURotation     rot;

#ifdef CFG_LCD_ENABLE
	screenWidth = ithLcdGetWidth();
	screenHeight = ithLcdGetHeight();

	window = SDL_CreateWindow("Display Control Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, 0);
	if (!window)
	{
		printf("Couldn't create window: %s\n", SDL_GetError());
		return;
	}

	// init itu
	ituLcdInit();

#ifdef CFG_M2D_ENABLE
	ituM2dInit();
#else
	ituSWInit();
#endif // CFG_M2D_ENABLE

	ituSceneInit(&theScene, NULL);

#ifdef CFG_ENABLE_ROTATE
	//ituSceneSetRotation(&theScene, ITU_ROT_90, CFG_LCD_WIDTH, CFG_LCD_HEIGHT);//for screen image and touch direction
	ituSetRotation(ITU_ROT_270);//just for screen image
#endif

#ifdef CFG_VIDEO_ENABLE
	ituFrameFuncInit();
#endif // CFG_VIDEO_ENABLE

#ifdef CFG_PLAY_VIDEO_ON_BOOTING
#ifndef CFG_BOOT_VIDEO_ENABLE_WINDOW_MODE
	rot = itv_get_rotation();

	if (rot == ITU_ROT_90 || rot == ITU_ROT_270)
		PlayVideo(0, 0, ithLcdGetHeight(), ithLcdGetWidth(), CFG_BOOT_VIDEO_BGCOLOR, CFG_BOOT_VIDEO_VOLUME);
	else
		PlayVideo(0, 0, ithLcdGetWidth(), ithLcdGetHeight(), CFG_BOOT_VIDEO_BGCOLOR, CFG_BOOT_VIDEO_VOLUME);
#else
	PlayVideo(CFG_VIDEO_WINDOW_X_POS, CFG_VIDEO_WINDOW_Y_POS, CFG_VIDEO_WINDOW_WIDTH, CFG_VIDEO_WINDOW_HEIGHT, CFG_BOOT_VIDEO_BGCOLOR, CFG_BOOT_VIDEO_VOLUME);
#endif
#endif

#ifdef CFG_PLAY_MJPEG_ON_BOOTING
#ifndef CFG_BOOT_VIDEO_ENABLE_WINDOW_MODE
	rot = itv_get_rotation();

	if (rot == ITU_ROT_90 || rot == ITU_ROT_270)
		PlayMjpeg(0, 0, ithLcdGetHeight(), ithLcdGetWidth(), CFG_BOOT_VIDEO_BGCOLOR, 0);
	else
		PlayMjpeg(0, 0, ithLcdGetWidth(), ithLcdGetHeight(), CFG_BOOT_VIDEO_BGCOLOR, 0);
#else
	PlayMjpeg(CFG_VIDEO_WINDOW_X_POS, CFG_VIDEO_WINDOW_Y_POS, CFG_VIDEO_WINDOW_WIDTH, CFG_VIDEO_WINDOW_HEIGHT, CFG_BOOT_VIDEO_BGCOLOR, 0);
#endif
#endif

	screenSurf = ituGetDisplaySurface();

	ituFtInit();
	ituFtLoadFont(0, CFG_PRIVATE_DRIVE ":/font/" CFG_FONT_FILENAME, ITU_GLYPH_8BPP);

	//ituSceneInit(&theScene, NULL);
	ituSceneSetFunctionTable(&theScene, actionFunctions);

	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_COMMAND_QUEUE_SIZE;
	attr.mq_msgsize = sizeof(Command);

	commandQueue = mq_open("scene", O_CREAT | O_NONBLOCK, 0644, &attr);
	assert(commandQueue != -1);

	screenDistance = sqrtf(screenWidth * screenWidth + screenHeight * screenHeight);

	isReady = false;
	periodPerFrame = MS_PER_FRAME;
#endif
}

void SceneExit(void)
{
#ifdef CFG_LCD_ENABLE
	mq_close(commandQueue);
	commandQueue = -1;

	resetScene();

	if (theScene.root)
	{
		ituSceneExit(&theScene);
	}
	ituFtExit();

#ifdef CFG_M2D_ENABLE
	ituM2dExit();
#ifdef CFG_VIDEO_ENABLE
	ituFrameFuncExit();
#endif // CFG_VIDEO_ENABLE
#else
	ituSWExit();
#endif // CFG_M2D_ENABLE

	SDL_DestroyWindow(window);
#endif
}

void SceneLoad(void)
{
	Command cmd;

	if (commandQueue == -1)
		return;

	isReady = false;

	cmd.id = CMD_LOAD_SCENE;

	mq_send(commandQueue, (const char *)&cmd, sizeof(Command), 0);
}

void SceneGotoMainMenu(void)
{
	Command cmd;

	if (commandQueue == -1)
		return;

	cmd.id = CMD_GOTO_MAINMENU;
	mq_send(commandQueue, (const char *)&cmd, sizeof(Command), 0);
}

void SceneChangeLanguage(void)
{
	Command cmd;

	if (commandQueue == -1)
		return;

	cmd.id = CMD_CHANGE_LANG;
	mq_send(commandQueue, (const char *)&cmd, sizeof(Command), 0);
}

void ScenePredraw(int arg)
{
	Command cmd;

	if (commandQueue == -1)
		return;

	cmd.id = CMD_PREDRAW;
	mq_send(commandQueue, (const char *)&cmd, sizeof(Command), 0);
}

void SceneSetReady(bool ready)
{
	isReady = ready;
}

static void LoadScene(void)
{
#ifdef CFG_LCD_ENABLE
	uint32_t tick1, tick2;

	resetScene();
	if (theScene.root)
	{
		ituSceneExit(&theScene);
	}

	// load itu file
	tick1 = SDL_GetTicks();

#ifdef CFG_LCD_MULTIPLE
	{
		char filepath[PATH_MAX];

		sprintf(filepath, CFG_PRIVATE_DRIVE ":/itu/%ux%u/ctrlboard.itu", ithLcdGetWidth(), ithLcdGetHeight());
		ituSceneLoadFileCore(&theScene, filepath);
	}
#else
	ituSceneLoadFileCore(&theScene, CFG_PRIVATE_DRIVE ":/ctrlboard.itu");
#endif // CFG_LCD_MULTIPLE

	tick2 = SDL_GetTicks();
	printf("itu loading time: %dms\n", tick2 - tick1);

	if (theConfig.lang != LANG_ENG)
		ituSceneUpdate(&theScene, ITU_EVENT_LANGUAGE, theConfig.lang, 0, 0);

	//ituSceneSetRotation(&theScene, ITU_ROT_90, ithLcdGetWidth(), ithLcdGetHeight());

	tick1 = tick2;

#if defined(CFG_USB_MOUSE) || defined(_WIN32)
	cursorIcon = ituSceneFindWidget(&theScene, "cursorIcon");
	if (cursorIcon)
	{
		ituWidgetSetVisible(cursorIcon, true);
	}
#endif // defined(CFG_USB_MOUSE) || defined(_WIN32)

	tick2 = SDL_GetTicks();
	printf("itu init time: %dms\n", tick2 - tick1);

	ExternalProcessInit();
#endif
}

void SceneEnterVideoState(int timePerFrm)
{
	if (inVideoState)
	{
		return;
	}

#ifndef DISABLE_SWITCH_VIDEO_STATE
#ifdef CFG_VIDEO_ENABLE
	ituFrameFuncInit();
#endif
	screenSurf = ituGetDisplaySurface();
	inVideoState = true;
	if (timePerFrm != 0)
		periodPerFrame = timePerFrm;
#endif
}

void SceneLeaveVideoState(void)
{
	if (!inVideoState)
	{
		return;
	}

#ifndef DISABLE_SWITCH_VIDEO_STATE
#ifdef CFG_VIDEO_ENABLE
	ituFrameFuncExit();
#endif
#ifdef CFG_LCD_ENABLE
	ituLcdInit();
#endif
#ifdef CFG_M2D_ENABLE
	ituM2dInit();
#else
	ituSWInit();
#endif

	screenSurf = ituGetDisplaySurface();
	periodPerFrame = MS_PER_FRAME;
#endif
	inVideoState = false;
}

static void GotoMainMenu(void)
{
	ITULayer *mainMenuLayer = ituSceneFindWidget(&theScene, "mainMenuLayer");
	assert(mainMenuLayer);
	ituLayerGoto(mainMenuLayer);
}

static void ProcessCommand(void)
{
	Command cmd;

	while (mq_receive(commandQueue, (char *)&cmd, sizeof(Command), 0) > 0)
	{
		switch (cmd.id)
		{
		case CMD_LOAD_SCENE:
			LoadScene();
#if defined(CFG_PLAY_VIDEO_ON_BOOTING)
			ituScenePreDraw(&theScene, screenSurf);
			WaitPlayVideoFinish();
#elif defined(CFG_PLAY_MJPEG_ON_BOOTING)
			ituScenePreDraw(&theScene, screenSurf);
			WaitPlayMjpegFinish();
#endif
			ituSceneStart(&theScene);
			break;

		case CMD_GOTO_MAINMENU:
			GotoMainMenu();
			break;

		case CMD_CHANGE_LANG:
			ituSceneUpdate(&theScene, ITU_EVENT_LANGUAGE, theConfig.lang, 0, 0);
			ituSceneUpdate(&theScene, ITU_EVENT_LAYOUT, 0, 0, 0);
			break;

#if !defined(CFG_PLAY_VIDEO_ON_BOOTING) && !defined(CFG_PLAY_MJPEG_ON_BOOTING)
		case CMD_PREDRAW:
			ituScenePreDraw(&theScene, screenSurf);
			break;
#endif
		}
	}
}

static bool CheckQuitValue(void)
{
	if (quitValue)
	{
		if (ScreenSaverIsScreenSaving() && theConfig.screensaver_type == SCREENSAVER_BLANK)
			ScreenSaverRefresh();

		return true;
	}
	return false;
}

static void CheckStorage(void)
{
	StorageAction action = StorageCheck();

	switch (action)
	{
	case STORAGE_SD_INSERTED:
		ituSceneSendEvent(&theScene, EVENT_CUSTOM_SD_INSERTED, NULL);
		break;

	case STORAGE_SD_REMOVED:
		ituSceneSendEvent(&theScene, EVENT_CUSTOM_SD_REMOVED, NULL);
		break;

	case STORAGE_USB_INSERTED:
		ituSceneSendEvent(&theScene, EVENT_CUSTOM_USB_INSERTED, NULL);
		break;

	case STORAGE_USB_REMOVED:
		ituSceneSendEvent(&theScene, EVENT_CUSTOM_USB_REMOVED, NULL);
		break;

	case STORAGE_USB_DEVICE_INSERTED:
	{
		ITULayer *usbDeviceModeLayer = ituSceneFindWidget(&theScene, "usbDeviceModeLayer");
		assert(usbDeviceModeLayer);

		ituLayerGoto(usbDeviceModeLayer);
	}
	break;

	case STORAGE_USB_DEVICE_REMOVED:
	{
		ITULayer *mainMenuLayer = ituSceneFindWidget(&theScene, "mainMenuLayer");
		assert(mainMenuLayer);

		ituLayerGoto(mainMenuLayer);
	}
	break;
	}
}

static void CheckExternal(void)
{
	ExternalEvent   ev;
	int             ret = ExternalReceive(&ev);

	if (ret)
	{
		ScreenSaverRefresh();
		ExternalProcessEvent(&ev);
	}
}

#if defined(CFG_USB_MOUSE) || defined(_WIN32)

static void CheckMouse(void)
{
	if (ioctl(ITP_DEVICE_USBMOUSE, ITP_IOCTL_IS_AVAIL, NULL))
	{
		if (!ituWidgetIsVisible(cursorIcon))
			ituWidgetSetVisible(cursorIcon, true);
	}
	else
	{
		if (ituWidgetIsVisible(cursorIcon))
			ituWidgetSetVisible(cursorIcon, false);
	}
}

#endif // defined(CFG_USB_MOUSE) || defined(_WIN32)

#define TEST_PORT       ITP_DEVICE_UART3
#define TEST_DEVICE     itpDeviceUart3	
#define TEST_BAUDRATE   "115200"

static int create_chain_list(struct chain_list_tag *p_chain_list)
{
	p_chain_list->rear = 0;
	p_chain_list->front = 0;
	p_chain_list->count = 0;
	memset(p_chain_list->buf, 0, sizeof(p_chain_list->buf));
	return 0;
}

static int in_chain_list(struct chain_list_tag *p_chain_list, unsigned char src)
{
	int position = (p_chain_list->rear + 1) % MAX_CHAIN_NUM;
	//已经满
	if (position == p_chain_list->front){
		return 0;
	}
	*(p_chain_list->buf + p_chain_list->rear) = src;
	p_chain_list->rear = position;
	return 1;
}


static int out_chain_list(struct chain_list_tag *p_chain_list, unsigned char *src)
{
	//空
	if (p_chain_list->rear == p_chain_list->front){
		return 0;
	}
	*src = *(p_chain_list->buf + p_chain_list->front);
	p_chain_list->front = (p_chain_list->front + 1) % MAX_CHAIN_NUM;
	return 1;
}
unsigned short crc16_ccitt(const char *buf, int len)
{
	register int counter;
	register unsigned short crc = 0;
	for (counter = 0; counter < len; counter++)
		crc = (crc << 8) ^ crc16tab[((crc >> 8) ^ *(char *)buf++) & 0x00FF];
	return crc;
}

//分析数据
unsigned char
process_data(struct uart_data_tag *dst, struct chain_list_tag *p_chain_list)
{
	unsigned char flag = 0;
	unsigned short crc = 0;
	unsigned char buf = 0;

	while (out_chain_list(p_chain_list, &buf)){
		//上一个数据错误，等待一下个0xEA
		if (dst->state == 1){
			//下一个0xEA 重新计算
			if (buf == 0xEA){
				(dst->buf_data)[dst->count++] = buf;
				dst->state = 0;
			}
		}
		else{
			dst->buf_data[dst->count++] = buf;
			//接受完17个数据 结束
			if (dst->count == 17){
				//检查crc 
				//检查crc 
				crc = crc16_ccitt(dst->buf_data + 1, 14);
				if (((unsigned char)(crc >> 8) == dst->buf_data[15]) &&
					((unsigned char)crc == dst->buf_data[16])
					){
					flag = 0;
				}
				else{
					flag = 1;
				}
				//如果出错
				if (flag == 1){
					dst->state = 1;
					dst->count = 0;
				}
				else{
					dst->count = 0;
					dst->state = 2;
					return 0;
				}
			}
		}
	}
	return 0;
}



//分析得到的数组
void process_frame(struct main_uart_chg *dst, const unsigned char *src)
{
	src = src + 2;
	//判断第几个帧
	//第0帧
	if ((*src & 0x0f) == 0x00){

		//初始化
		dst->state_show = 0;

		//[2]主板信息   [0][0] 
		src += 2;

		//[0][1]
		//得到主机状态
		dst->machine_state = *src & 0x03;

		//判断是否故障
		dst->is_err = *src & 0x04;

		//判断状态 流水
		if (*src & 0x10){
			dst->state_show = 0x01;
			//风机
		}
		else if (*src & 0x20){
			dst->state_show = dst->state_show | 0x2;
			//火焰
		}
		else if (*src & 0x40){
			dst->state_show = dst->state_show | 0x4;
			//风压
		}
		else if (*src & 80){
			dst->state_show = dst->state_show | 0x8;
		}

		//设置温度 [0][4]
		src += 3;
		dst->shezhi_temp = *src++;

		//出水温度[0][5]
		dst->chushui_temp = *src++;

		//进水温度[0][6]
		dst->jinshui_temp = *src++;

		//错误代码或者比例阀电流
		src++;
		dst->err_no = *src++;

		printf("frame 00\n");
		//第1帧
	}
	else if ((*src & 0x0f) == 0x01){
		printf("frame 01\n");

		//第2帧
	}
	else if ((*src & 0x0f) == 0x02){
		printf("frame 02\n");

	}
	else if ((*src & 0x0f) == 0x03){
		printf("frame 03\n");

	}
}


//线程串口回调函数
static void* UartFunc(void* arg)
{
	//主线程发送消息队列
	struct main_pthread_mq_tag main_pthread_mq;
	uint8_t getstr1[10];
	//默认应答
	uint8_t texBufArray[] = { 0xEB, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xD8, 0x2A };
	int len = 0;
	int flag = 0;
	struct   timespec tm;
	tm.tv_nsec = 1;
	while (1){
		
		memset(getstr1, 0, sizeof(getstr1));
		len = read(TEST_PORT, getstr1, sizeof(getstr1));
		//如果串口有数据
		if (len > 0){

			//写入环形缓存
			for (int i = 0; i < len; i++){
				flag = in_chain_list(&chain_list, getstr1[i]);
			}
			process_data(&uart_data, &chain_list);
			//已经完成
			if (uart_data.state == 2){
				for (int j = 0; j< uart_data.count; j++){
					printf("0x%02X ", uart_data.buf_data[j]);

				}
				printf("\nfinish \n");
				uart_data.state = 0;
				uart_data.count = 0;

				//分析收到的数
				process_frame(&g_main_uart_chg_data, uart_data.buf_data);

				//延迟20ms 发送回复信息
				usleep(10);
				write(TEST_PORT, texBufArray, sizeof(texBufArray));

			}
		}
		
		usleep(1);
	}
}




int SceneRun(void)
{


	//樱雪
	//初始锁
	if (pthread_mutex_init(&msg_mutex, NULL) != 0){
		printf("error mutex func=%s,line=%d\n", __func__, __LINE__);
	}

	//缓存时间
	struct timeval buf_tm;

	//闪烁的时间
	unsigned int shansuo_t = 0;
	
	//消息队列
	struct mq_attr mq_uart_attr;
	mq_uart_attr.mq_flags = 0;
	mq_uart_attr.mq_maxmsg = 10;
	mq_uart_attr.mq_msgsize = sizeof(struct main_pthread_mq_tag);
	uartQueue = mq_open("scene", O_CREAT | O_NONBLOCK, 0644, &mq_uart_attr);


	memset(&uart_data, 0, sizeof(struct uart_data_tag));


	//主线程发送消息队列
	struct main_pthread_mq_tag main_pthread_mq;


	create_chain_list(&chain_list);


	pthread_t task;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_create(&task, &attr, UartFunc, NULL);

	node_widget_init();

	unsigned char flag = 0;

	uint8_t texBufArray[] = { 0xEB, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xD8, 0x2A };
	struct timeval curtime;
	SDL_Event   ev;
	int         delay, frames, lastx, lasty;
	uint32_t    tick, dblclk, lasttick, mouseDownTick;
	static bool first_screenSurf = true, sleepModeDoubleClick = false;
#if defined(CFG_POWER_WAKEUP_IR)
	static bool sleepModeIR = false;
#endif

	/* Watch keystrokes */
	dblclk = frames = lasttick = lastx = lasty = mouseDownTick = 0;



	for (;;)
	{
		
		bool result = false;

		//樱雪

		//缓存时间
		gettimeofday(&buf_tm, NULL);
		
		//主页面运行


		if (CheckQuitValue())
			break;

#ifdef CFG_LCD_ENABLE
		ProcessCommand();
#endif
		CheckExternal();
		CheckStorage();

#if defined(CFG_USB_MOUSE) || defined(_WIN32)
		if (cursorIcon)
			CheckMouse();
#endif     // defined(CFG_USB_MOUSE) || defined(_WIN32)

		tick = SDL_GetTicks();

#ifdef FPS_ENABLE
		frames++;
		if (tick - lasttick >= 1000)
		{
			//printf("fps: %d\n", frames);
			frames = 0;
			lasttick = tick;
		}
#endif     // FPS_ENABLE

#ifdef CFG_LCD_ENABLE
		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_KEYDOWN:
				ScreenSaverRefresh();
				result = ituSceneUpdate(&theScene, ITU_EVENT_KEYDOWN, ev.key.keysym.sym, 0, 0);
				switch (ev.key.keysym.sym)
				{
					//case SDLK_UP:
				case 1073741884:
					curr_node_widget->updown_cb(curr_node_widget, 0);
					break;
				case SDLK_UP:
					curr_node_widget->updown_cb(curr_node_widget, 0);
					break;
				case 1073741889:
					//case SDLK_DOWN:
					curr_node_widget->updown_cb(curr_node_widget, 1);
					break;
				case SDLK_DOWN:
					//case SDLK_DOWN:
					curr_node_widget->updown_cb(curr_node_widget, 1);
					break;

				case 1073741883:
					curtime = buf_tm;
					break;
				case 13://回车
					curtime = buf_tm;
					break;
				case 1073741885:
					printf("power on\off");
					break;
					
				case SDLK_LEFT: //开机和关机
					pthread_mutex_lock(&msg_mutex);
					if (g_main_uart_chg_data.run_state == 0){
						//开机
						ScreenOn();
						ituLayerGoto(ituSceneFindWidget(&theScene, "welcom"));
						g_main_uart_chg_data.run_state = 3;

					}
					else if (g_main_uart_chg_data.run_state == 2){
						ituLayerGoto(ituSceneFindWidget(&theScene, "welcom"));
						g_main_uart_chg_data.run_state = 1;
					}
					pthread_mutex_unlock(&msg_mutex);

					break;
				case SDLK_RIGHT:
					//ituSceneSendEvent(&theScene, EVENT_CUSTOM_KEY3, NULL);
					ScreenOn();
					break;

				case SDLK_INSERT:
					break;

#ifdef _WIN32
				case SDLK_e:
					result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHPINCH, 20, 30, 40);
					break;

				case SDLK_f:
				{
					ITULayer *usbDeviceModeLayer = ituSceneFindWidget(&theScene, "usbDeviceModeLayer");
					assert(usbDeviceModeLayer);

					ituLayerGoto(usbDeviceModeLayer);
				}
				break;

				case SDLK_g:
				{
					ExternalEvent ev;

					ev.type = EXTERNAL_SHOW_MSG;
					strcpy(ev.buf1, "test");

					ScreenSaverRefresh();
					ExternalProcessEvent(&ev);
				}
				break;

#endif          // _WIN32
				}
				if (result && !ScreenIsOff() && !StorageIsInUsbDeviceMode())
					AudioPlayKeySound();

				break;

			case SDL_KEYUP:
				result = ituSceneUpdate(&theScene, ITU_EVENT_KEYUP, ev.key.keysym.sym, 0, 0);
				switch (ev.key.keysym.sym)
				{
				case 1073741883:
					printf("sdlk_keyup longpress\n");
					unsigned long t_curr = 0;
					struct timeval t_time = { 0 };
					gettimeofday(&t_time, NULL);
					t_curr = t_time.tv_sec - curtime.tv_sec;
					if (t_curr >= 2){
						printf("long press\n");
						if (curr_node_widget->long_press_cb)
							curr_node_widget->long_press_cb(curr_node_widget, 1);
					}
					else{
						printf("ok\n");
						curr_node_widget->confirm_cb(curr_node_widget, 2);
					}

					break;
				}

				break;

			case SDL_MOUSEMOTION:
				ScreenSaverRefresh();
#if defined(CFG_USB_MOUSE) || defined(_WIN32)
				if (cursorIcon)
				{
					ituWidgetSetX(cursorIcon, ev.button.x);
					ituWidgetSetY(cursorIcon, ev.button.y);
					ituWidgetSetDirty(cursorIcon, true);
					//printf("mouse: move %d, %d\n", ev.button.x, ev.button.y);
				}
#endif             // defined(CFG_USB_MOUSE) || defined(_WIN32)
				result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEMOVE, ev.button.button, ev.button.x, ev.button.y);
				break;

			case SDL_MOUSEBUTTONDOWN:
				ScreenSaverRefresh();
				printf("mouse: down %d, %d\n", ev.button.x, ev.button.y);
				{
					mouseDownTick = SDL_GetTicks();
#ifdef DOUBLE_KEY_ENABLE
					if (mouseDownTick - dblclk <= 200)
					{
						result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEDOUBLECLICK, ev.button.button, ev.button.x, ev.button.y);
						dblclk = 0;
					}
					else
#endif             // DOUBLE_KEY_ENABLE
					{
						result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEDOWN, ev.button.button, ev.button.x, ev.button.y);
						dblclk = mouseDownTick;
						lastx = ev.button.x;
						lasty = ev.button.y;
					}
					if (result && !ScreenIsOff() && !StorageIsInUsbDeviceMode())
						AudioPlayKeySound();

#ifdef CFG_SCREENSHOT_ENABLE
					if (ev.button.x < 50 && ev.button.y > CFG_LCD_HEIGHT - 50)
						Screenshot(screenSurf);
#endif             // CFG_SCREENSHOT_ENABLE
				}
				break;

			case SDL_MOUSEBUTTONUP:
				if (SDL_GetTicks() - dblclk <= 200)
				{
					int xdiff = abs(ev.button.x - lastx);
					int ydiff = abs(ev.button.y - lasty);

					if (xdiff >= GESTURE_THRESHOLD && xdiff > ydiff)
					{
						if (ev.button.x > lastx)
						{
							printf("mouse: slide to right\n");
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDERIGHT, xdiff, ev.button.x, ev.button.y);
						}
						else
						{
							printf("mouse: slide to left\n");
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDELEFT, xdiff, ev.button.x, ev.button.y);
						}
					}
					else if (ydiff >= GESTURE_THRESHOLD)
					{
						if (ev.button.y > lasty)
						{
							printf("mouse: slide to down\n");
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDEDOWN, ydiff, ev.button.x, ev.button.y);
						}
						else
						{
							printf("mouse: slide to up\n");
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDEUP, ydiff, ev.button.x, ev.button.y);
						}
					}
				}
				result |= ituSceneUpdate(&theScene, ITU_EVENT_MOUSEUP, ev.button.button, ev.button.x, ev.button.y);
				mouseDownTick = 0;
				break;

			case SDL_FINGERMOTION:
				ScreenSaverRefresh();
				printf("touch: move %d, %d\n", ev.tfinger.x, ev.tfinger.y);
				result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEMOVE, 1, ev.tfinger.x, ev.tfinger.y);
				break;

			case SDL_FINGERDOWN:
				ScreenSaverRefresh();
				printf("touch: down %d, %d\n", ev.tfinger.x, ev.tfinger.y);
				{
					mouseDownTick = SDL_GetTicks();
#ifdef DOUBLE_KEY_ENABLE
#ifdef CFG_POWER_WAKEUP_DOUBLE_CLICK_INTERVAL
					if (mouseDownTick - dblclk <= CFG_POWER_WAKEUP_DOUBLE_CLICK_INTERVAL)
#else
					if (mouseDownTick - dblclk <= 200)
#endif
					{
						printf("double touch!\n");
						if (sleepModeDoubleClick)
						{
							ScreenSetDoubleClick();
							ScreenSaverRefresh();
							sleepModeDoubleClick = false;
						}
						result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEDOUBLECLICK, 1, ev.tfinger.x, ev.tfinger.y);
						dblclk = mouseDownTick = 0;
					}
					else
#endif             // DOUBLE_KEY_ENABLE
					{
						result = ituSceneUpdate(&theScene, ITU_EVENT_MOUSEDOWN, 1, ev.tfinger.x, ev.tfinger.y);
						dblclk = mouseDownTick;
						lastx = ev.tfinger.x;
						lasty = ev.tfinger.y;
					}
					if (result && !ScreenIsOff() && !StorageIsInUsbDeviceMode())
						AudioPlayKeySound();

#ifdef CFG_SCREENSHOT_ENABLE
					if (ev.tfinger.x < 50 && ev.tfinger.y > CFG_LCD_HEIGHT - 50)
						Screenshot(screenSurf);
#endif             // CFG_SCREENSHOT_ENABLE
					//if (ev.tfinger.x < 50 && ev.tfinger.y > CFG_LCD_HEIGHT - 50)
					//    SceneQuit(QUIT_UPGRADE_WEB);
				}
				break;

			case SDL_FINGERUP:
				printf("touch: up %d, %d\n", ev.tfinger.x, ev.tfinger.y);
				if (SDL_GetTicks() - dblclk <= 300)
				{
					int xdiff = abs(ev.tfinger.x - lastx);
					int ydiff = abs(ev.tfinger.y - lasty);

					if (xdiff >= GESTURE_THRESHOLD && xdiff > ydiff)
					{
						if (ev.tfinger.x > lastx)
						{
							printf("touch: slide to right %d %d\n", ev.tfinger.x, ev.tfinger.y);
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDERIGHT, xdiff, ev.tfinger.x, ev.tfinger.y);
						}
						else
						{
							printf("touch: slide to left %d %d\n", ev.tfinger.x, ev.tfinger.y);
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDELEFT, xdiff, ev.tfinger.x, ev.tfinger.y);
						}
					}
					else if (ydiff >= GESTURE_THRESHOLD)
					{
						if (ev.tfinger.y > lasty)
						{
							printf("touch: slide to down %d %d\n", ev.tfinger.x, ev.tfinger.y);
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDEDOWN, ydiff, ev.tfinger.x, ev.tfinger.y);
						}
						else
						{
							printf("touch: slide to up %d %d\n", ev.tfinger.x, ev.tfinger.y);
							result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHSLIDEUP, ydiff, ev.tfinger.x, ev.tfinger.y);
						}
					}
				}
				result |= ituSceneUpdate(&theScene, ITU_EVENT_MOUSEUP, 1, ev.tfinger.x, ev.tfinger.y);
				mouseDownTick = 0;
				break;

			case SDL_MULTIGESTURE:
				printf("touch: multi %d, %d\n", ev.mgesture.x, ev.mgesture.y);
				if (ev.mgesture.dDist > 0.0f)
				{
					int dist = (int)(screenDistance * ev.mgesture.dDist);
					int x = (int)(screenWidth * ev.mgesture.x);
					int y = (int)(screenHeight * ev.mgesture.y);
					result |= ituSceneUpdate(&theScene, ITU_EVENT_TOUCHPINCH, dist, x, y);
				}
				break;
			}
		}
		if (!ScreenIsOff())
		{
			if (mouseDownTick > 0 && (SDL_GetTicks() - mouseDownTick >= MOUSEDOWN_LONGPRESS_DELAY))
			{
				printf("long press: %d %d\n", lastx, lasty);
				result |= ituSceneUpdate(&theScene, ITU_EVENT_MOUSELONGPRESS, 1, lastx, lasty);
				mouseDownTick = 0;
			}
			result |= ituSceneUpdate(&theScene, ITU_EVENT_TIMER, 0, 0, 0);
			//printf("%d\n", result);
			if (result)
			{
				ituSceneDraw(&theScene, screenSurf);
				ituFlip(screenSurf);
				if (first_screenSurf)
				{
					ScreenSetBrightness(theConfig.brightness);
					first_screenSurf = false;
				}
			}

			if (theConfig.screensaver_type != SCREENSAVER_NONE &&
				ScreenSaverCheck())
			{
				ituSceneSendEvent(&theScene, EVENT_CUSTOM_SCREENSAVER, "0");

				if (theConfig.screensaver_type == SCREENSAVER_BLANK)
				{
					// have a change to flush action commands
					ituSceneUpdate(&theScene, ITU_EVENT_TIMER, 0, 0, 0);

					// draw black screen
					ituSceneDraw(&theScene, screenSurf);
					ituFlip(screenSurf);

					ScreenOff();

#if defined(CFG_POWER_WAKEUP_IR)
					sleepModeIR = true;
#endif
#if defined(CFG_POWER_WAKEUP_TOUCH_DOUBLE_CLICK)
					sleepModeDoubleClick = true;
#endif
				}
			}
		}

#if defined(CFG_POWER_WAKEUP_IR)
		if (ScreenIsOff() && sleepModeIR)
		{
			printf("Wake up by remote IR!\n");
			ScreenSaverRefresh();
			ituSceneUpdate(&theScene, ITU_EVENT_MOUSEDOWN, 1, 0, 0);
			ituSceneDraw(&theScene, screenSurf);
			ituFlip(screenSurf);
			sleepModeIR = false;
		}
#endif

		if (sleepModeDoubleClick)
		{
			if (theConfig.screensaver_type != SCREENSAVER_NONE &&
				ScreenSaverCheckForDoubleClick())
			{
				if (theConfig.screensaver_type == SCREENSAVER_BLANK)
					ScreenOffContinue();
			}
		}
#endif
		delay = periodPerFrame - (SDL_GetTicks() - tick);
		//printf("scene loop delay=%d\n", delay);
		if (delay > 0)
		{
			SDL_Delay(delay);
		}
		else
			sched_yield();
	}

	return quitValue;
}

void SceneQuit(QuitValue value)
{
	quitValue = value;
}

QuitValue SceneGetQuitValue(void)
{
	return quitValue;
}