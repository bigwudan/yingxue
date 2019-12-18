#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"
#include "ctrlboard.h"

#define RIPPLE_MAX_COUNT 2
#define RIPPLE_2ND_DELAY 10
#define MENU_TOTAL_APP 12
#define MENU_ROW_DIST 233
#define MENU_COLUMN_DIST 306
#define MENU_FIRST_X 81
#define MENU_FIRST_Y 86
#define MENU_ROW_NUM 2
#define MENU_ROW_RANGE 3

static int moveTableX[2][10] = {
	{ 33, 32, 31, 30, 30, 30, 30, 30, 30, 30 },
	{ 66, 64, 62, 60, 60, 60, 60, 60, 60, 60 }
};

static int moveTableY[10] = {
	26, 24, 23, 23, 23, 23, 23, 23, 23, 23
};

static char widgetName[50];
static bool longPressState = false, arranging = false, moving = false;
static int arrange_start, arrange_end;
static int hold_cnt = 0, arrange_cnt = 0, margin_hold_cnt = 0, page_index = 0;
static int press_index, lastX, lastY, lastPos, offsetX, offsetY;

static ITUBackground* mainMenuBackground;
static ITUBackground* mainMenuRippleBackground;
static ITUCoverFlow* mainMenuCoverFlow;
static ITUShadow *mainMenuSubShadow[2][12];
static ITUContainer *mainMenuSubContainer[2][12];
static ITUContainer *mainMenuPFSubContainer[2][12];
static ITUContainer *mainMenuTmpContainer[12];
static ITUButton *mainMenuTmpButton[12];
static ITUAnimation *mainMenuTmpAnimation[12];
static ITUAnimation *mainMenuSubAnimation[2][12];
static ITUPopupButton *mainMenuSubPopupButton[2][12];
static ITUPageFlow* mainMenuPageFlow;
static ITUAnimation* mainMenu1Animation0;
static ITUAnimation* mainMenu2Animation0;
static ITUAnimation* mainMenu1Animation1;
static ITUAnimation* mainMenu2Animation1;

static ITUAnimation* mainMenu1Animations[RIPPLE_MAX_COUNT];
static ITUAnimation* mainMenu2Animations[RIPPLE_MAX_COUNT];

void MainMenuShadowSetVisibility(bool);
void MainMenuSetDisplayType(void);
int MainMenuGetCurrentPosition(int, int);
void MainMenuAnimationSetPlay(bool);

static void MainMenuPlay2ndRipple0(int arg)
{
    ITUAnimation* mainMenu2Animation = mainMenu2Animations[0];

    ituWidgetSetVisible(mainMenu2Animation, true);
    ituAnimationPlay(mainMenu2Animation, 0);
}

static void MainMenuPlay2ndRipple1(int arg)
{
    ITUAnimation* mainMenu2Animation = mainMenu2Animations[1];

    ituWidgetSetVisible(mainMenu2Animation, true);
    ituAnimationPlay(mainMenu2Animation, 0);
}

static bool MainMenuBackgroundUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = ituIconUpdate(widget, ev, arg1, arg2, arg3);

    if (ev == ITU_EVENT_MOUSEDOWN)
    {
        if (theConfig.mainmenu_type == MAINMENU_COVERFLOW_RIPPLE && ituWidgetIsEnabled(widget))
        {
            int x = arg2 - widget->rect.x;
            int y = arg3 - widget->rect.y;

            if (ituWidgetIsInside(widget, x, y))
            {
                int xx, yy, i;

                for (i = 0; i < RIPPLE_MAX_COUNT; i++)
                {
                    ITUAnimation *mainMenu1Animation, *mainMenu2Animation;
                        
                    mainMenu1Animation = mainMenu1Animations[i];
                    if (mainMenu1Animation->playing)
                        continue;

                    mainMenu2Animation = mainMenu2Animations[i];

                    xx = x - ituWidgetGetWidth(mainMenu1Animation) / 2;
                    yy = y - ituWidgetGetHeight(mainMenu1Animation) / 2;
                    ituWidgetSetVisible(mainMenu1Animation, true);
                    ituWidgetSetPosition(mainMenu1Animation, xx, yy);
                    ituAnimationPlay(mainMenu1Animation, 0);

                    if (i == 0)
                        ituSceneExecuteCommand(&theScene, RIPPLE_2ND_DELAY, MainMenuPlay2ndRipple0, 0);
                    else if (i == 1)
                        ituSceneExecuteCommand(&theScene, RIPPLE_2ND_DELAY, MainMenuPlay2ndRipple1, 0);

                    xx = x - ituWidgetGetWidth(mainMenu2Animation) / 2;
                    yy = y - ituWidgetGetHeight(mainMenu2Animation) / 2;
                    ituWidgetSetPosition(mainMenu2Animation, xx, yy);

                    result = true;
                    break;
                }
            }
        }
    }
    return result;
}

static void MainMenuAnimationOnStop(ITUAnimation* animation)
{
    ituWidgetSetVisible(animation, false);
}

bool MainMenuOnEnter(ITUWidget* widget, char* param)
{
    int i, j;

    if (!mainMenuBackground)
    {
        mainMenuBackground = ituSceneFindWidget(&theScene, "mainMenuBackground");
        assert(mainMenuBackground);
        ituWidgetSetUpdate(mainMenuBackground, MainMenuBackgroundUpdate);

        mainMenuRippleBackground = ituSceneFindWidget(&theScene, "mainMenuRippleBackground");
        assert(mainMenuRippleBackground);

        mainMenuCoverFlow = ituSceneFindWidget(&theScene, "mainMenuCoverFlow");
        assert(mainMenuCoverFlow);

		for (i = 0; i < 2; i++){
			for (j = 0; j < MENU_TOTAL_APP; j++){
				sprintf(widgetName, "mainMenuSub%dShadow%d", i, j);
				mainMenuSubShadow[i][j] = ituSceneFindWidget(&theScene, widgetName);
				assert(mainMenuSubShadow[i][j]);

				sprintf(widgetName, "mainMenuSub%dContainer%d", i, j);
				mainMenuSubContainer[i][j] = ituSceneFindWidget(&theScene, widgetName);
				assert(mainMenuSubContainer[i][j]);

				sprintf(widgetName, "mainMenuPFSub%dContainer%d", i, j);
				mainMenuPFSubContainer[i][j] = ituSceneFindWidget(&theScene, widgetName);
				assert(mainMenuPFSubContainer[i][j]);

				sprintf(widgetName, "mainMenuSub%dAnimation%d", i, j);
				mainMenuSubAnimation[i][j] = ituSceneFindWidget(&theScene, widgetName);
				assert(mainMenuSubAnimation[i][j]);

				sprintf(widgetName, "mainMenuSub%dPopupButton%d", i, j);
				mainMenuSubPopupButton[i][j] = ituSceneFindWidget(&theScene, widgetName);
				assert(mainMenuSubPopupButton[i][j]);
			}
		}
		for (i = 0; i < MENU_TOTAL_APP; i++){
			sprintf(widgetName, "mainMenuTmpContainer%d", i);
			mainMenuTmpContainer[i] = ituSceneFindWidget(&theScene, widgetName);
			assert(mainMenuTmpContainer[i]);

			sprintf(widgetName, "mainMenuTmpButton%d", i);
			mainMenuTmpButton[i] = ituSceneFindWidget(&theScene, widgetName);
			assert(mainMenuTmpButton[i]);

			sprintf(widgetName, "mainMenuTmpAnimation%d", i);
			mainMenuTmpAnimation[i] = ituSceneFindWidget(&theScene, widgetName);
			assert(mainMenuTmpAnimation[i]);
		}

        mainMenuPageFlow = ituSceneFindWidget(&theScene, "mainMenuPageFlow");
        assert(mainMenuPageFlow);

        i = 0;
        mainMenu1Animation0 = ituSceneFindWidget(&theScene, "mainMenu1Animation0");
        ituAnimationSetOnStop(mainMenu1Animation0, MainMenuAnimationOnStop);
        mainMenu1Animations[i] = mainMenu1Animation0;

        mainMenu2Animation0 = ituSceneFindWidget(&theScene, "mainMenu2Animation0");
        assert(mainMenu2Animation0);
        ituAnimationSetOnStop(mainMenu2Animation0, MainMenuAnimationOnStop);
        mainMenu2Animations[i] = mainMenu2Animation0;
        i++;

        mainMenu1Animation1 = ituSceneFindWidget(&theScene, "mainMenu1Animation1");
        assert(mainMenu1Animation1);
        ituAnimationSetOnStop(mainMenu1Animation1, MainMenuAnimationOnStop);
        mainMenu1Animations[i] = mainMenu1Animation1;

        mainMenu2Animation1 = ituSceneFindWidget(&theScene, "mainMenu2Animation1");
        assert(mainMenu2Animation1);
        ituAnimationSetOnStop(mainMenu2Animation1, MainMenuAnimationOnStop);
        mainMenu2Animations[i] = mainMenu2Animation1;
        i++;

    #if !defined(CFG_PLAY_VIDEO_ON_BOOTING) && !defined(CFG_PLAY_MJPEG_ON_BOOTING)        
        ituSceneExecuteCommand(&theScene, 3, ScenePredraw, 0);
    #endif

		for (i = 0; i < 6; i++){
			theConfig.order[0][i] = i;
			theConfig.order[1][i] = i + 6;
		}
		for (i = 0; i < 2; i++){
			for (j = 0; j < 6; j++){
				ituWidgetSetVisible(mainMenuSubContainer[i][theConfig.order[i][j]], true);
				ituWidgetSetVisible(mainMenuPFSubContainer[i][theConfig.order[i][j]], true);
			}
		}
    }

	MainMenuShadowSetVisibility(false);

	MainMenuSetDisplayType();

    for (i = 0; i < RIPPLE_MAX_COUNT; i++)
    {
        ituWidgetSetVisible(mainMenu1Animations[i], false);
        ituWidgetSetVisible(mainMenu2Animations[i], false);
    }

    return true;
}

bool MainMenuOnLeave(ITUWidget* widget, char* param)
{
    int i;

    for (i = 0; i < RIPPLE_MAX_COUNT; i++)
    {
        ituAnimationStop(mainMenu1Animations[i]);
        ituAnimationStop(mainMenu2Animations[i]);
    }
    return true;
}

bool MainMenuOnTimer(ITUWidget* widget, char* param)
{		
	bool updated = false;

	// arrange menu
	int i, curX, curY, curPos, tmp;
	if (longPressState == true) {
		widget = (ITUWidget*)mainMenuCoverFlow;
		widget->flags &= ~ITU_DRAGGING;
		curX = theScene.lastMouseX;
		curY = theScene.lastMouseY;
		curPos = MainMenuGetCurrentPosition(curX, curY);
		ituWidgetSetX(mainMenuTmpContainer[press_index], curX - offsetX);
		ituWidgetSetY(mainMenuTmpContainer[press_index], curY - offsetY);

		if (abs(curX - lastX) < 5 && abs(curY - lastY)<5 && curPos >= 0)
			hold_cnt++;
		else {
			hold_cnt = 0;
			lastX = curX;
			lastY = curY;
		}

		if (ituWidgetGetX(mainMenuTmpContainer[press_index]) > 793
			|| ituWidgetGetX(mainMenuTmpContainer[press_index]) < -20){
			margin_hold_cnt++;
			curPos = 5;
		}
		else
			margin_hold_cnt = 0;

		if (margin_hold_cnt > 20 && !moving){
			if (ituWidgetGetX(mainMenuTmpContainer[press_index]) > 793 && page_index < 1){
				ituWidgetSetVisible(mainMenuSubContainer[1][theConfig.order[1][5]], false);
				ituWidgetSetVisible(mainMenuPFSubContainer[1][theConfig.order[1][5]], false);
				ituCoverFlowNext(mainMenuCoverFlow);
				moving = true;
				margin_hold_cnt = 0;
			}
			else if (ituWidgetGetX(mainMenuTmpContainer[press_index]) < -20 && page_index > 0){
				ituWidgetSetVisible(mainMenuSubContainer[0][theConfig.order[0][5]], false);
				ituWidgetSetVisible(mainMenuPFSubContainer[0][theConfig.order[0][5]], false);
				ituCoverFlowPrev(mainMenuCoverFlow);
				moving = true;
				margin_hold_cnt = 0;
			}
		}

		if (hold_cnt > 1 && curPos != lastPos && arranging == false){
			printf("arrange.... start from %d to %d\n", lastPos, curPos);
			arranging = true;
			arrange_cnt = 0;
			arrange_start = lastPos;
			arrange_end = curPos;
			tmp = theConfig.order[page_index][arrange_start];
			if (arrange_start < arrange_end) {
				for (i = arrange_start; i < arrange_end; i++)
					theConfig.order[page_index][i] = theConfig.order[page_index][i + 1];
			}
			else {
				for (i = arrange_start; i > arrange_end; i--)
					theConfig.order[page_index][i] = theConfig.order[page_index][i - 1];
			}
			theConfig.order[page_index][arrange_end] = tmp;

			lastPos = curPos;
		}

		if (arranging == true && arrange_cnt < 10)
		{
			if (arrange_start < arrange_end) {
				for (i = arrange_start; i < arrange_end; i++) {
					if (i == 2) {
						ituWidgetSetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) + moveTableX[1][arrange_cnt]);
						ituWidgetSetY(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetY(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) - moveTableY[arrange_cnt]);
					}
					else
						ituWidgetSetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) - moveTableX[0][arrange_cnt]);
				}
			}
			else {
				for (i = arrange_start; i > arrange_end; i--) {
					if (i == 3) {
						ituWidgetSetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) - moveTableX[1][arrange_cnt]);
						ituWidgetSetY(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetY(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) + moveTableY[arrange_cnt]);
					}
					else
						ituWidgetSetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]], ituWidgetGetX(mainMenuSubContainer[page_index][theConfig.order[page_index][i]]) + moveTableX[0][arrange_cnt]);
				}
			}
			arrange_cnt++;
		}
		if (arrange_cnt >= 10) {
			arranging = false;
		}
		updated = true;
	}
	return updated;
}

bool MainMenuPopupButtonOnLongPress(ITUWidget* widget, char* param)
{
	int i, j;
	ituUnPressWidget(widget);
	ituWidgetSetVisible(mainMenuCoverFlow, true);
	ituWidgetSetVisible(mainMenuPageFlow, false);
	page_index = mainMenuCoverFlow->focusIndex;
	press_index = atoi(param);
	mainMenuTmpButton[press_index]->pressed = true;
	ituWidgetSetVisible(mainMenuSubContainer[page_index][press_index], false);
	ituWidgetSetVisible(mainMenuPFSubContainer[page_index][press_index], false);
	ituWidgetSetVisible(mainMenuTmpContainer[press_index], true);
	ituWidgetToBottom(mainMenuTmpContainer[press_index]);
	ituAnimationPlay(mainMenuTmpAnimation[press_index], 0);
	lastX = ituWidgetGetX(mainMenuTmpContainer[press_index]);
	lastY = ituWidgetGetY(mainMenuTmpContainer[press_index]);
	for (i = 0; i < 2; i++)
		for (j = 0; j < MENU_TOTAL_APP; j++)
			ituWidgetSetVisible(mainMenuSubPopupButton[i][j], false);
	MainMenuAnimationSetPlay(true);
	offsetX = theScene.lastMouseX - lastX;
	offsetY = theScene.lastMouseY - lastY;
	lastPos = MainMenuGetCurrentPosition(theScene.lastMouseX, theScene.lastMouseY);
	longPressState = true;
	hold_cnt = 0;
	return true;
}

bool MainMenuPopupButtonOnMouseUp(ITUWidget* widget, char* param)
{
	int i, j, x, y;
	if (longPressState == true) {
		MainMenuAnimationSetPlay(false);
		ituAnimationStop(mainMenuTmpAnimation[press_index]);
		ituWidgetSetVisible(mainMenuTmpContainer[press_index], false);
		for (i = 0; i < 2; i++){
			for (j = 0; j < 6; j++){
				x = MENU_FIRST_X + MENU_COLUMN_DIST * (j % MENU_ROW_RANGE);
				y = MENU_FIRST_Y + MENU_ROW_DIST * (j / MENU_ROW_RANGE) - ituWidgetGetY(mainMenuCoverFlow);
				ituWidgetSetPosition(mainMenuSubContainer[i][theConfig.order[i][j]], x, y);
				ituWidgetSetPosition(mainMenuPFSubContainer[i][theConfig.order[i][j]], x, y);
				ituWidgetSetPosition(mainMenuTmpContainer[theConfig.order[i][j]], x, y);
				ituWidgetSetVisible(mainMenuSubContainer[i][theConfig.order[i][j]], true);
				ituWidgetSetVisible(mainMenuPFSubContainer[i][theConfig.order[i][j]], true);
				ituWidgetSetVisible(mainMenuSubPopupButton[i][theConfig.order[i][j]], true);
			}
		}
		MainMenuSetDisplayType();
		longPressState = false;
		arranging = false;
		arrange_cnt = 0;
		press_index = -1;
		moving = false;
	}
	return true;
}

bool MainMenuCoverFlowOnChange(ITUWidget* widget, char* param)
{
	int prev, next, tmp;
	if (longPressState == true) {
		prev = page_index;
		next = atoi(param);
		mainMenuTmpButton[press_index]->pressed = true;
		page_index = next;
		tmp = theConfig.order[next][5];
		theConfig.order[next][5] = theConfig.order[prev][5];
		theConfig.order[prev][5] = tmp;
		ituWidgetSetVisible(mainMenuSubContainer[prev][theConfig.order[prev][5]], true);
		ituWidgetSetVisible(mainMenuPFSubContainer[prev][theConfig.order[prev][5]], true);
		lastPos = 5;
		moving = false;
	}
	return true;
}

void MainMenuReset(void)
{
    mainMenuBackground = NULL;
}

void MainMenuShadowSetVisibility(bool isVisible)
{
	int i, j;
	for (i = 0; i < 2; i++){
		for (j = 0; j < 12; j++){
			ituWidgetSetVisible(mainMenuSubShadow[i][j], isVisible);
		}
	}
}

void MainMenuSetDisplayType()
{
	switch (theConfig.mainmenu_type)
	{
	case MAINMENU_COVERFLOW:
		ituWidgetSetVisible(mainMenuRippleBackground, false);
		ituWidgetSetVisible(mainMenuCoverFlow, true);
		ituWidgetSetVisible(mainMenuPageFlow, false);
		break;

	case MAINMENU_COVERFLOW_REFLECTION:
		ituWidgetSetVisible(mainMenuRippleBackground, false);
		ituWidgetSetVisible(mainMenuCoverFlow, true);
		ituWidgetSetVisible(mainMenuPageFlow, false);
		MainMenuShadowSetVisibility(true);
		break;

	case MAINMENU_PAGEFLOW_FLIP:
		ituWidgetSetVisible(mainMenuRippleBackground, false);
		ituWidgetSetVisible(mainMenuCoverFlow, false);
		ituWidgetSetVisible(mainMenuPageFlow, true);
		mainMenuPageFlow->type = ITU_PAGEFLOW_FLIP;
		break;

	case MAINMENU_PAGEFLOW_FLIP2:
		ituWidgetSetVisible(mainMenuRippleBackground, false);
		ituWidgetSetVisible(mainMenuCoverFlow, false);
		ituWidgetSetVisible(mainMenuPageFlow, true);
		mainMenuPageFlow->type = ITU_PAGEFLOW_FLIP2;
		break;

	case MAINMENU_PAGEFLOW_FOLD:
		ituWidgetSetVisible(mainMenuRippleBackground, false);
		ituWidgetSetVisible(mainMenuCoverFlow, false);
		ituWidgetSetVisible(mainMenuPageFlow, true);
		mainMenuPageFlow->type = ITU_PAGEFLOW_FOLD;
		break;

	case MAINMENU_COVERFLOW_RIPPLE:
		ituWidgetSetVisible(mainMenuRippleBackground, true);
		ituWidgetSetVisible(mainMenuCoverFlow, true);
		ituWidgetSetVisible(mainMenuPageFlow, false);
		break;
	}
}

int MainMenuGetCurrentPosition(int x, int y)
{
	int pos = 5, i, j;
	for (i = 0; i < MENU_ROW_NUM; i++){
		if ((y >= (MENU_FIRST_Y + MENU_ROW_DIST * i))
			&& (y < (MENU_FIRST_Y + MENU_ROW_DIST * (i + 1)))){
			for (j = 0; j < MENU_ROW_RANGE; j++){
				if ((x >= (MENU_FIRST_X + MENU_COLUMN_DIST * j))
					&& (x < (MENU_FIRST_X + MENU_COLUMN_DIST * (j + 1))))
					pos = i * MENU_ROW_RANGE + j;
			}
		}
	}
	return pos;
}

void MainMenuAnimationSetPlay(bool toPlay)
{
	int i, j;
	for (i = 0; i < 2; i++){
		for (j = 0; j < MENU_TOTAL_APP; j++){
			ituWidgetSetVisible(mainMenuSubAnimation[i][j], toPlay);
			if (toPlay)
				ituAnimationPlay(mainMenuSubAnimation[i][j], 0);
			else
				ituAnimationStop(mainMenuSubAnimation[i][j]);
		}
	}
}

//樱雪每个页面初始化

bool YX_MenuOnEnter(ITUWidget* widget, char* param)
{
	ITUWidget *t_widget = NULL;
	//MainLayer 首页
	if (strcmp(widget->name, "MainLayer") == 0){
		//test
		ituLayerGoto(ituSceneFindWidget(&theScene, "moshiLayer"));
		return true;
		//全部隐藏
		t_widget = ituSceneFindWidget(&theScene, "Background100");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "Background102");
		ituWidgetSetVisible(t_widget, false);
		//第三个大框
		t_widget = ituSceneFindWidget(&theScene, "Background134");
		ituWidgetSetVisible(t_widget, false);
		//默认选中第一个
		curr_node_widget = &mainlayer_0;
		t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
		ituWidgetSetVisible(t_widget, true);
	}
	else if (strcmp(widget->name, "yureLayer") == 0){
		//全部隐藏
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton78");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "Background27");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "Background30");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "Background46");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "Background132");
		ituWidgetSetVisible(t_widget, false);
		//第三个大框中的小框
		t_widget = ituSceneFindWidget(&theScene, "Background94");
		ituWidgetSetVisible(t_widget, false);
		//默认选中第一个
		curr_node_widget = &yureLayer_0;
		t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
		ituWidgetSetVisible(t_widget, true);
		t_widget = ituSceneFindWidget(&theScene, curr_node_widget->name);
		ituWidgetSetVisible(t_widget, false);
	}
	else if (strcmp(widget->name, "yureshijianLayer") == 0 ){
		//默认选中第一个
		curr_node_widget = &yureshijian_widget_0;
		//显示选中
		t_widget = ituSceneFindWidget(&theScene, curr_node_widget->focus_back_name);
		ituWidgetSetVisible(t_widget, true);
		//屏蔽未选中
		t_widget = ituSceneFindWidget(&theScene, curr_node_widget->name);
		ituWidgetSetVisible(t_widget, false);
	}
	else if (strcmp(widget->name, "yureshezhiLayer") == 0){
		//默认选中第一个
		curr_node_widget = &yureshezhiLayer_0;

		//没有选中
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton60");
		ituWidgetSetVisible(t_widget, false);
		
		//焦点在
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton85");
		ituWidgetSetVisible(t_widget, true);



		//Background37
		//焦点选框
		t_widget = ituSceneFindWidget(&theScene, "Background37");
		ituWidgetSetVisible(t_widget, false);

		//选中背景
		t_widget = ituSceneFindWidget(&theScene, "Background45");
		ituWidgetSetVisible(t_widget, false);
		//Background33
		//焦点选框
		t_widget = ituSceneFindWidget(&theScene, "Background33");
		ituWidgetSetVisible(t_widget, false);

		//选中背景Background105
		t_widget = ituSceneFindWidget(&theScene, "Background105");
		ituWidgetSetVisible(t_widget, false);


		//Background4
		//焦点
		t_widget = ituSceneFindWidget(&theScene, "Background40");
		ituWidgetSetVisible(t_widget, false);

		//选中背景
		t_widget = ituSceneFindWidget(&theScene, "Background107");
		ituWidgetSetVisible(t_widget, false);

	}
	else if (strcmp(widget->name, "moshiLayer") == 0){
		//默认选中第一个
		curr_node_widget = &moshiLayer_0;

		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton68");
		ituWidgetSetVisible(t_widget, false);

		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton80");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton11");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton12");
		ituWidgetSetVisible(t_widget, false);
		t_widget = ituSceneFindWidget(&theScene, "BackgroundButton13");
		ituWidgetSetVisible(t_widget, false);
	}

}