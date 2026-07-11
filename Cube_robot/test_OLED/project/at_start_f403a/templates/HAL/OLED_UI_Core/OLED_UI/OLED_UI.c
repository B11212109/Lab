#include "OLED_UI.h"
#include "..\OLED_UI_Launcher.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#ifdef OLED_UI
/**
 * @brief ����Ŀ��Դ��ַ��
 * @param "https://github.com/bdth-7777777/OLED_UI"
 */
/*OLED_UIȫ�ֱ������� */
OLED_UI_Counter OLED_FPS = {0,0,0};									//���ڴ洢֡�ʵĽṹ��
OLED_Key OLED_UI_Key = {1,1,1,1};   								//���ڴ洢����״̬�Ľṹ��,Ĭ��û�а���?����Ϊ1
OLED_Key OLED_UI_LastKey = {1,1,1,1};								//���ڴ洢��һ�ְ���״̬�Ľṹ��,Ĭ��û�а���?����Ϊ1
MenuPage*  CurrentMenuPage = NULL;									//ȫ�ֽṹ��ָ�룬��ǰҳ���ָ��?
MenuWindow *CurrentWindow = NULL;									//ȫ�ֽṹ��ָ�룬��ǰ���ڵ�ָ��
MutexFlag KeyEnterFlag = FLAGEND;									//ȫ��enter�����Ļ�������������ΪFLAGSTARTʱ��ʾ����ִ�лص�����
MutexFlag FadeOutFlag = FLAGEND;									//����Ч���Ļ�������������ΪFLAGSTARTʱ��ʾ����ִ�н���Ч��
bool ColorMode = DARKMODE;											//ȫ�ֲ���������?���洢��ǰ��ʾģʽ��trueΪ��ɫģʽ��falseΪǳɫģʽ
bool OLED_UI_ShowFps = false;										//ȫ�ֲ���������?�����ڿ����Ƿ���ʾ֡��
int16_t OLED_UI_Brightness = 100;									//ȫ�ֱ������洢��ǰ��Ļ����
OLED_UI_WindowSustainCounter OLED_SustainCounter = {0,false};		//���ڴ洢���ڳ���ʱ��Ľṹ��?
int tmpi=0;                                                       	//Gif�������ٲ���tempֵ

static const uint8_t OLED_UI_BootLogo[48 * 6] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x80, 0xC0, 0xC0, 0xE0, 0xE0, 0xF0, 0xF0, 0xF8, 0xF8, 0xFC,
    0xFC, 0xFC, 0xF8, 0xF8, 0xF0, 0xF0, 0xE0, 0xE0, 0xC0, 0xC0, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x38, 0x78, 0x7C, 0xFC, 0xFE, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFE, 0xFE, 0xFC, 0x7C, 0x78, 0x38, 0x10, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFE, 0xFE, 0xFC, 0xFC, 0xF8, 0xF0, 0xF0, 0xE0, 0xC0, 0xC1,
    0x83, 0x83, 0x07, 0x0F, 0x0F, 0x1F, 0x3F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x3F, 0x1F, 0x0F, 0x0F, 0x07, 0x83,
    0x83, 0xC1, 0xC0, 0xE0, 0xF0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFE, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF8, 0xF0, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x00, 0xF0, 0xF0, 0xF8, 0xF8, 0xFC, 0xFE, 0xFE, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0x0F, 0x1F, 0x1F, 0x3F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x3F, 0x1F, 0x1F, 0x0F, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x03, 0x03, 0x07, 0x0F, 0x0F, 0x1F, 0x1F, 0x3F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3F, 0x3F, 0x1F, 0x1F, 0x0F, 0x0F, 0x07, 0x03, 0x03,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void OLED_UI_ShowBootLogo(void)
{
    OLED_Clear();
    OLED_ShowImage(40, 0, 48, 48, OLED_UI_BootLogo);
    OLED_ShowString(10, 56, "Rubik's Cube Robot", OLED_6X8_HALF);
    OLED_Update();
    Delay_ms(3000U);
}

/***********************************************************************************************/
/***************************��Щ�������ڴ洢��Ҫ�󶨶����Ŀؼ��Ĳ���*******************************/

// OLED_UI_Cursor�����ڴ洢���Ľṹ�壬���ƹ����ƶ�
OLED_ChangeArea OLED_UI_Cursor;
// OLED_UI_MenuFrame�����ڴ洢�˵��߿�Ľṹ�壬���Ʋ˵��߿����ڲ�������ƶ�
OLED_ChangeArea OLED_UI_MenuFrame;
// OLED_UI_Window�����ڴ洢���ڵĽṹ�壬���ƴ������ڲ�������ƶ�?
OLED_ChangeArea OLED_UI_Window;
// OLED_UI_ScrollBarHeight�����ڴ洢�������߶ȵĽṹ�壬���ƹ������߶ȵı仯
OLED_ChangeDistance OLED_UI_ScrollBarHeight;
// OLED_UI_ProbWidth�����ڴ洢���������ȵĽṹ�壬���ƽ��������ȵı仯
OLED_ChangeDistance OLED_UI_ProbWidth;
// OLED_UI_PageStartPoint�����ڴ洢ҳ����ʼ��Ľṹ�壬����ҳ��������ƶ�
OLED_ChangePoint OLED_UI_PageStartPoint ;
// OLED_UI_LineStep�����ڴ洢�м��Ľṹ�壬�����м��ı仯
OLED_ChangeDistance OLED_UI_LineStep;



/**
 * @brief ��ȡ��ǰ��Ļˢ����?������洢��ȫ�ֱ���OLED_FPS.value��
 * @param ��
 * @note �ú�����Ҫ����20ms�����ڵ���?������ᵼ�¼�������?
 * @return ��
 */
void GetFPS(void){
	if(OLED_FPS.step < (OLED_UI_TICK_HZ - 1)){
		OLED_FPS.step++;
	}else{
		OLED_FPS.step=0;
		OLED_FPS.value = OLED_FPS.count;
		OLED_FPS.count=0;
	}
}

/**
 * @brief ��ʾ��ǰ��Ļˢ����
 * @param ��
 * @note �轫�˺���������ѭ������?��ÿѭ��һ�μ�Ϊһ��ˢ�¡�
 * @return ��
 */
void OLED_UI_ShowFPS(void){
    OLED_FPS.count ++;
	if (OLED_UI_ShowFps){
		OLED_Printf(110,0,OLED_6X8_HALF,"%3d",OLED_FPS.value);
	}
}
/**
 * @brief ��ȡ��ǰҳ����������
 * @param style CHINESE�����ġ��� ASCII��ASCII��
 * @return ��ǰҳ����������
 */
OLED_Font GetOLED_Font(OLED_Font fontsize,bool style){
	//���ݵ�ǰҳ��������С��������
	OLED_Font ChineseFont,ASCIIFont;
	switch(fontsize){
		case OLED_UI_FONT_8:
			ChineseFont = OLED_8X8_FULL,ASCIIFont = OLED_6X8_HALF;break;
		case OLED_UI_FONT_12:
			ChineseFont = OLED_12X12_FULL,ASCIIFont = OLED_7X12_HALF;break;
		case OLED_UI_FONT_16:
			ChineseFont = OLED_16X16_FULL,ASCIIFont = OLED_8X16_HALF;break;
		case OLED_UI_FONT_20:
			ChineseFont = OLED_20X20_FULL,ASCIIFont = OLED_10X20_HALF;break;
		default:
			ChineseFont = OLED_12X12_FULL,ASCIIFont = OLED_7X12_HALF;break;
	}
	if(style == CHINESE){
		return ChineseFont;
	}else{
		return ASCIIFont;
	}

}
/**
 * @brief ��ת��ʾ���?
 * @param X ���X����
 * @param Y ���Y����
 * @param Width ������
 * @param Height ���߶�
 * @param Style ������?
 * @note �ú������ڷ�ת��ʾ��꣬ʹ����ʾΪ���?
 * @return ��
 */
void ReverseCoordinate(int16_t X, int16_t Y, int16_t Width, int16_t Height,uint8_t Style){
	switch(Style){
		
		case REVERSE_RECTANGLE://���η�ɫ
			OLED_ReverseArea(X, Y, Width, Height);
			break;
		case REVERSE_ROUNDRECTANGLE://Բ�Ǿ��η�ɫ
			if(Width >= 3 && Height >= 3){
				OLED_ReverseArea(X, Y, 1, 1);
				OLED_ReverseArea(X+Width-1, Y, 1, 1);
				OLED_ReverseArea(X, Y+Height-1, 1, 1);
				OLED_ReverseArea(X+Width-1, Y+Height-1, 1, 1);
			}
			OLED_ReverseArea(X, Y, Width, Height);
			break;

		case HOLLOW_RECTANGLE://���ľ���
			OLED_ReverseArea(X, Y, Width, Height);
			OLED_ReverseArea(X+1, Y+1, Width-2, Height-2);
			break;
		case HOLLOW_ROUNDRECTANGLE://���ľ���
			OLED_ReverseArea(X, Y, Width, Height);
			OLED_ReverseArea(X+1, Y+1, Width-2, Height-2);
			if(Width >= 3){
				OLED_ReverseArea(X, Y, 1, 1);
				OLED_ReverseArea(X+Width-1, Y, 1, 1);
			}
			if(Height >= 3){
				OLED_ReverseArea(X, Y+Height-1, 1, 1);
				OLED_ReverseArea(X+Width-1, Y+Height-1, 1, 1);
			}
			break;
		case REVERSE_BLOCK://С����
			
			OLED_ReverseArea(X, Y + 1, GetOLED_Font(CurrentMenuPage->General_FontSize,ASCII), Height);
			
			break;
		case NOT_SHOW:
			break;
		default:
			OLED_ReverseArea(X, Y, Width, Height);
		break;
	}
}

/** 
 * @brief����ָ������Ӧ��ģʽ������Ч��(�ɰ������?
 * @param x0 ������ʼX����
 * @param y0 ������ʼY����
 * @param width �������?
 * @param height ����߶�?
 * @param fadeLevel ������λ��1��5֮���ֵ����ʾ��ͬ������Ϩ���??
 * @note �ú���������ָ������Ӧ��ģʽ������Ч����ʹ�ø�����������𽥱䰵��Ч�������ɰ��������
*/
void OLED_UI_FadeOut_Masking(int16_t x0, int16_t y0, int16_t width, int16_t height, int8_t fadeLevel) {
    // ��鲢���������?
    if (x0 < 0) {
        width += x0;
        x0 = 0;
    }
    if (y0 < 0) {
        height += y0;
        y0 = 0;
    }
    if (x0 + width > OLED_WIDTH) {
        width = OLED_WIDTH - x0;
    }
    if (y0 + height > OLED_HEIGHT) {
        height = OLED_HEIGHT - y0;
    }
    if (width <= 0 || height <= 0) {
        return;
    }

    // ȷ��������λ����Ч��Χ��
    if (fadeLevel < 1 || fadeLevel > 5) {
        return;
    }

    // ����2x2����Ľ����??
    // ÿ�������ʾһ��?x2��������Щ������ҪϨ��
    // 0��ʾ��������1��ʾϨ��
    const uint8_t patterns[5][2][2] = {
        {{0, 0},  // Level 1: ȫ��
         {0, 0}},
        
        {{1, 0},  // Level 2: ���Ͻ�Ϩ��
         {0, 0}},
        
        {{1, 0},  // Level 3: ���ϽǺ����½�Ϩ��
         {0, 1}},
        
        {{1, 0},  // Level 4: ֻ�������Ͻ�
         {1, 1}},
        
        {{1, 1},  // Level 5: ȫ��
         {1, 1}}
    };

    // ����߽�?
    int16_t xEnd = x0 + width;
    int16_t yEnd = y0 + height;

    // Ӧ�ý���Ч��
    for (int16_t y = y0; y < yEnd; y++) {
        int page = y / 8;
        int bit_pos = y % 8;
        uint8_t pixel_mask = 1 << bit_pos;
        
        for (int16_t x = x0; x < xEnd; x++) {
            // ������2x2�����е����λ��?
            int grid_x = (x - x0) % 2;
            int grid_y = (y - y0) % 2;
            
            // ���ݽ������������λ�þ����Ƿ�Ϩ������?
            if (patterns[fadeLevel - 1][grid_y][grid_x]) {
                OLED_DisplayBuf[page][x] &= ~pixel_mask;
            }
        }
    }
}


/**
 * @brief ��ȡ�˵���ṹ����������һ��Ԫ�ص�ID
 * @param items �ṹ������MenuItem��ָ��
 * @return �˵���ṹ�������Ԫ������
 */
MenuID GetMenuItemNum(MenuItem * items){
	MenuID num = 0;
	while(items[num].General_item_text != NULL){
		num+=1;
	}
	return num;
}


/**
 * @brief ���ݵ�ǰ��ѡ�Ķ�����ʽ���ı両��������
 * @param CurrentNum ��ǰֵ��ָ��
 * @param TargetNum Ŀ��ֵָ��
 * @param StartNum ��ʼֵָ��
 * @param CurrentStepNum ��ǰ����ָ��
 * @return ��
 */
void ChangeFloatNum(float *CurrentNum, float *TargetNum, float *ErrorNum, float *LastErrorNum, float * IntegralNum, float *DerivativeNum)  {
	if(CurrentMenuPage->General_MoveStyle ==  UNLINEAR){
		if(*CurrentNum == *TargetNum){
			return;
		}
		//����û����ٶ������?����ô��ǰֱֵ�ӵ���Ŀ��ֵ
		if(CurrentMenuPage->General_MovingSpeed <= 0){

			*ErrorNum = 0;
			*LastErrorNum = 0;
		    *CurrentNum = *TargetNum;
			return;
		}
		*LastErrorNum = *ErrorNum;
		//���㱾������?
		*ErrorNum = *TargetNum - *CurrentNum; 
		//���㵱ǰֵ
		*CurrentNum += 0.02f*CurrentMenuPage->General_MovingSpeed * (*ErrorNum);
		//��Ŀ��ֵ�뵱ǰֵ���С���ٶ�ֵ��?/20ʱ����Ϊ�Ѿ�����Ŀ��ֵ
		if(fabs(*CurrentNum - *TargetNum) < CurrentMenuPage->General_MovingSpeed/20.0f){

			*ErrorNum = 0;
			*LastErrorNum = 0;
		    *CurrentNum = *TargetNum;
			return;
		}
	}
	//�����ǰ�Ķ�����ʽ��PID_CURVE��ʽ
	if(CurrentMenuPage->General_MoveStyle ==  PID_CURVE){
		/*����һ�����صķ�������Ϊ����ǰֵ����Ŀ��ֵ��ʱ��������������?�����ǻ����û�б����㡣����ʵ������������Ч������õġ�?*/
		//����û����ٶ������?����ô��ǰֱֵ�ӵ���Ŀ��ֵ�����������м�ֵ����
		if(CurrentMenuPage->General_MovingSpeed <= 0){

			*ErrorNum = 0;
			*LastErrorNum = 0;
			*DerivativeNum = 0;
			*IntegralNum = 0;
		    *CurrentNum = *TargetNum;
			return;
		}
		// ����PID����
		//����PID����
		float Kp = 0.02f * CurrentMenuPage->General_MovingSpeed;
		float Ki = 0.005f * CurrentMenuPage->General_MovingSpeed;
		float Kd = 0.002f ;

		//��¼��һ������?
		*LastErrorNum = *ErrorNum;
		//���㱾������?
		*ErrorNum = *TargetNum - *CurrentNum; 
		//��������?
		*IntegralNum += *ErrorNum;
		//����΢��ֵ��������ʱ����Ϊ0.1s
		*DerivativeNum = (*ErrorNum - *LastErrorNum) / 0.1f;
		//���㵱ǰֵ
		*CurrentNum += Kp * (*ErrorNum) + Ki *  (*IntegralNum) + Kd *(*DerivativeNum);
		//��Ŀ��ֵ�뵱ǰֵ���С��?.5ʱ����Ŀ��ֵǿ�Ƶ��ڵ�ǰֵ�����˻�������㣬���������м�ֵ����
		if(fabs(*TargetNum - *CurrentNum) < 0.5f){

			*ErrorNum = 0;
			*LastErrorNum = 0;
			*DerivativeNum = 0;
		    *CurrentNum = *TargetNum;
			return;
		}	
	}
	
}

/**
 * @brief �����Ըı�������
 * @param CurrentNum ��ǰֵ��ָ��
 * @param TargetNum Ŀ��ֵָ��
 * @param StepNum ����ָ��
 */
void ChangeDistance(OLED_ChangeDistance *distance){
	ChangeFloatNum(&distance->CurrentDistance,&distance->TargetDistance,&distance->Error,&distance->LastError,&distance->Integral,&distance->Derivative);
}

/**
 * @brief �����Ըı���������
 * @param OLED_MovingArea �ṹ�������ָ��?
 * @return ��
 */
void ChangePoint(OLED_ChangePoint *point){
	ChangeFloatNum(&point->CurrentPoint.X,&point->TargetPoint.X,&point->Error.X,&point->LastError.X,&point->Integral.X,&point->Derivative.X);
	ChangeFloatNum(&point->CurrentPoint.Y,&point->TargetPoint.Y,&point->Error.Y,&point->LastError.Y,&point->Integral.Y,&point->Derivative.Y);
}
/**
 * @brief �����Ըı��������?
 * @param OLED_MovingArea �ṹ�������ָ��?
 * @return ��
 */
void ChangeArea(OLED_ChangeArea *area)	{
	ChangeFloatNum(&area->CurrentArea.X,&area->TargetArea.X,&area->Error.X,&area->LastError.X,&area->Integral.X,&area->Derivative.X);
	ChangeFloatNum(&area->CurrentArea.Y,&area->TargetArea.Y,&area->Error.Y,&area->LastError.Y,&area->Integral.Y,&area->Derivative.Y);
	ChangeFloatNum(&area->CurrentArea.Width,&area->TargetArea.Width,&area->Error.Width,&area->LastError.Width,&area->Integral.Width,&area->Derivative.Width);
	ChangeFloatNum(&area->CurrentArea.Height,&area->TargetArea.Height,&area->Error.Height,&area->LastError.Height,&area->Integral.Height,&area->Derivative.Height);
}




/**
 * @brief �Ե�ǰ�Ĳ˵�ҳ��Ĳ������м�����ʼ��?
 * @param ��
 * @note ����ȷ���¼��½����Ӳ˵�ʱ�Ĳ���
 * @return ��
 */
void CurrentMenuPageInit(void){
	//�����ǰ�Ĳ˵�����ΪLIST
	if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
		//����ȫ��ҳ�浱ǰ��ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?���λ����ʼ��?
		OLED_UI_PageStartPoint.CurrentPoint.X = CurrentMenuPage->List_MenuArea.X+CurrentMenuPage->List_StartPointX;
		OLED_UI_PageStartPoint.CurrentPoint.Y = OLED_UI_MenuFrame.CurrentArea.Y+CurrentMenuPage->List_StartPointY;
		//����ȫ��ҳ��Ŀ����ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?���λ����ʼ��?
		OLED_UI_PageStartPoint.TargetPoint.X = CurrentMenuPage->List_MenuArea.X+CurrentMenuPage->List_StartPointX;
		OLED_UI_PageStartPoint.TargetPoint.Y = CurrentMenuPage->List_MenuArea.Y+CurrentMenuPage->List_StartPointY;
		//����ȫ��ҳ��Ŀ������Ϊ��ǰ�˵��ṹ�������?
		OLED_UI_MenuFrame.TargetArea.X = CurrentMenuPage->List_MenuArea.X;
		OLED_UI_MenuFrame.TargetArea.Y = CurrentMenuPage->List_MenuArea.Y;
		OLED_UI_MenuFrame.TargetArea.Width = CurrentMenuPage->List_MenuArea.Width;
		OLED_UI_MenuFrame.TargetArea.Height = CurrentMenuPage->List_MenuArea.Height;

		//���õ�ǰ�м���?�����С������˵���չ���Ķ���Ч��
		OLED_UI_LineStep.CurrentDistance = CurrentMenuPage->General_LineSpace;
		//����Ŀ���м��Ϊ�û����õ��о�?
		OLED_UI_LineStep.TargetDistance = CurrentMenuPage->General_LineSpace;
		//���õ�ǰ���Ϊ��ǰ�˵���ĵ�һ��
		CurrentMenuPage->_ActiveMenuID = 0;
		//���õ�ǰ��λΪ0
		CurrentMenuPage->_Slot = 0;
	}
	//�����ǰ�Ĳ˵�����ΪTILES
	if (CurrentMenuPage->General_MenuType == MENU_TYPE_TILES)
	{
		//����ȫ��ҳ��Ŀ����ʼ��Ϊ��Ļ��������ƫ�ư���������ȣ�ʹ�õ�ǰ�˵������
		OLED_UI_PageStartPoint.TargetPoint.X = CurrentMenuPage->Tiles_ScreenWidth/2-CurrentMenuPage->Tiles_TileWidth/2;
		//����ȫ��ҳ��Ŀ����ʼ��ΪԤ�����ʼ��?
		OLED_UI_PageStartPoint.TargetPoint.Y = TILES_STARTPOINT_Y;
		//����ȫ��ҳ�浱ǰ��ʼ��Ϊһ������λ��?��ʹ�ÿ�ʼʱ�ж���
		OLED_UI_PageStartPoint.CurrentPoint.X = -50;
		OLED_UI_PageStartPoint.CurrentPoint.Y = -CurrentMenuPage->Tiles_TileWidth;
		//���õ�ǰ��������?�Ա��������Ч��?
		OLED_UI_LineStep.CurrentDistance = 1;
		//����Ŀ��������Ϊ�û����õ��о�
		OLED_UI_LineStep.TargetDistance = CurrentMenuPage->General_LineSpace;
		//���õ�ǰ���Ϊ��ǰ�˵���ĵ�һ��
		CurrentMenuPage->_ActiveMenuID = 0;
	}
	//�������Ŀ�ʼ����㣬ȷ����ȷ�ϻ��Ƿ��ز���ʱ�������Ĳ˵�����¿��?
	SetLineSplitZero();
}
/**
 * @brief ��ԭ�˵���������һ�ε�״̬
 * @param ��
 * @note ���ڷ�����һ���˵�ʱ�Ĳ���
 * @return ��
 */
void CurrentMenuPageBackUp(void){
	//�����ǰ�Ĳ˵�����ΪLIST
	if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
		//����ȫ��ҳ�浱ǰ��ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?2
		OLED_UI_PageStartPoint.CurrentPoint.X = CurrentMenuPage->_StartPoint.X;
		OLED_UI_PageStartPoint.CurrentPoint.Y = CurrentMenuPage->_StartPoint.Y;
		//����ȫ��ҳ��Ŀ����ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?
		OLED_UI_PageStartPoint.TargetPoint.X = CurrentMenuPage->_StartPoint.X;
		OLED_UI_PageStartPoint.TargetPoint.Y = CurrentMenuPage->_StartPoint.Y;
		//���õ�ǰ�м���?�����С�����춯��Ч��?
		OLED_UI_LineStep.CurrentDistance = CurrentMenuPage->General_LineSpace;
		//����Ŀ���м��?
		OLED_UI_LineStep.TargetDistance = CurrentMenuPage->General_LineSpace;
	}
	//�����ǰ�Ĳ˵�����ΪTILES
	if (CurrentMenuPage->General_MenuType == MENU_TYPE_TILES)
	{
		//����ȫ��ҳ�浱ǰ��ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?2
		OLED_UI_PageStartPoint.CurrentPoint.X = CurrentMenuPage->_StartPoint.X + CurrentMenuPage->Tiles_TileWidth;
		OLED_UI_PageStartPoint.CurrentPoint.Y = -CurrentMenuPage->Tiles_TileHeight-1;
		//����ȫ��ҳ��Ŀ����ʼ��Ϊ�˵��ṹ��Ŀ�ʼ��?
		OLED_UI_PageStartPoint.TargetPoint.X = CurrentMenuPage->_StartPoint.X;
		OLED_UI_PageStartPoint.TargetPoint.Y = CurrentMenuPage->_StartPoint.Y;
		//���õ�ǰ�м���?�����С�����춯��Ч��?
		OLED_UI_LineStep.CurrentDistance = CurrentMenuPage->General_LineSpace;
		//����Ŀ���м��?
		OLED_UI_LineStep.TargetDistance = CurrentMenuPage->General_LineSpace;
	}
	//�������Ŀ�ʼ����㣬ȷ����ȷ�ϻ��Ƿ��ز���ʱ�������Ĳ˵�����¿��?
	SetLineSplitZero();
}

/**
 * @brief ��ʼ��OLED_UI�����õ�ǰҳ��Ľṹ��ָ�룬����ʼ��OLED��ʾ��
 * @param Page �˵�ҳ��ṹ��?
 * @return ��
 */
void OLED_UI_Init(MenuPage* Page){
	//��ʼ��OLED��ʾ��
	OLED_Init();
	OLED_UI_ShowBootLogo();

	//������ʱ��
	HAL_TIM_Base_Start_IT(&htim1);
	Timer_Init();
	Key_Init();
	Encoder_Init();

	//���õ�ǰҳ��Ľṹ��ָ��?
	CurrentMenuPage = Page;	//���õ�ǰҳ��Ľṹ��ָ��?
	//��ʼ���˵�ҳ�����?
	CurrentMenuPageInit();
	
}



/**
 * @brief ��ȡenter�¼�״̬�������ж��Ƿ�����ִ�лص�����
 * @param void
 * @return ��
 */
bool GetEnterFlag(void){
	if(KeyEnterFlag == FLAGEND){
		return true;
	}else{
		return false;
	}
}

/**
 * @brief ��ȡenter�¼�״̬�������ж��Ƿ�����ִ�лص�����
 * @param void
 * @return ��
 */
bool GetFadeoutFlag(void){
	if(FadeOutFlag == FLAGEND){
		return true;
	}else{
		return false;
	}
}

/**
 * @brief �����ַ����Ŀ���?����������Ӣ���ַ�����
 * @param String �ַ���ָ��
 * @param ChineseFont �����������?
 * @param ASCIIFont ASCII�������?
 * @return �ַ����Ŀ���
 */
int16_t CalcStringWidth(int16_t ChineseFont, int16_t ASCIIFont, const char *format, ...) {
    int16_t StringLength = 0;
    char String[MAX_STRING_LENGTH];

    va_list args;
    va_start(args, format);
    vsnprintf(String, sizeof(String), format, args); // ʹ��vsnprintf
    va_end(args);

    char *ptr = String;
    while (*ptr != '\0') {
        if ((unsigned char)*ptr & 0x80) { // ���������ַ�
            StringLength += ChineseFont;
            ptr += 2;
        } else {
            StringLength += ASCIIFont;
            ptr++;
        }
    }

    return StringLength;
}
/**
 * @brief ��ȡ�����������?
 * @param *int16_tdata ָ�룬���ڴ洢int16_t����
 * @param *float_tdata ָ�룬���ڴ洢float����
 * @return ����ֵΪ���� WINDOW_DATA_STYLE_FLOAT ��int16_tֵΪ���� WINDOW_DATA_STYLE_INT ����ָ�뷵�� WINDOW_DATA_STYLE_NONE
 */
int8_t GetWindowDataStyle(int16_t *int16_tdata,float *float_tdata){
	//����������ʷǷ��ڴ�?
	if(CurrentWindow == NULL){
		return -1;
	}
	if(int16_tdata != NULL){
		return WINDOW_DATA_STYLE_INT;
	}else if(float_tdata != NULL){
		return WINDOW_DATA_STYLE_FLOAT;
	}else{
		return WINDOW_DATA_STYLE_NONE;
	}
}

/**
 * @brief ���ƴ���
 * @param ��
 * @return ��
 */
void OLED_DrawWindow(void){
	//������ڱ�־λû�б���λ���򲻽��д���Ч���������ڵ�Ŀ��λ����λ��Ļ��?
	if(OLED_SustainCounter.SustainFlag == false){
		OLED_UI_Window.TargetArea.Height = 30;
		OLED_UI_Window.TargetArea.Width = 60;
		OLED_UI_Window.TargetArea.X = (OLED_WIDTH - 60)/2;	
		OLED_UI_Window.TargetArea.Y = -40;
	}
	if(CurrentWindow == NULL){
		return;
	}
	OLED_Font ChineseFont = GetOLED_Font(CurrentWindow->Text_FontSize,CHINESE),ASCIIFont = GetOLED_Font(CurrentWindow->Text_FontSize,ASCII);

	

	//������ڶ�����û���˳���Ļ������ƴ���
	if(OLED_SustainCounter.SustainFlag == true || (OLED_UI_Window.CurrentArea.Height != 30 && OLED_UI_Window.CurrentArea.Width != 60 && OLED_UI_Window.CurrentArea.Y != -40)){
		//ͨ�����ж��Ƿ���Ҫ����Բ�Ǿ���
		if(CurrentWindow->General_WindowType == WINDOW_RECTANGLE){
			OLED_DrawRectangle(OLED_UI_Window.CurrentArea.X - 1,OLED_UI_Window.CurrentArea.Y - 1,OLED_UI_Window.CurrentArea.Width + 2,OLED_UI_Window.CurrentArea.Height + 2,OLED_UNFILLED);
			OLED_ClearArea(OLED_UI_Window.CurrentArea.X,OLED_UI_Window.CurrentArea.Y,OLED_UI_Window.CurrentArea.Width,OLED_UI_Window.CurrentArea.Height);
		}else{
			OLED_DrawRoundedRectangle(OLED_UI_Window.CurrentArea.X - 1,OLED_UI_Window.CurrentArea.Y - 1,OLED_UI_Window.CurrentArea.Width + 2,OLED_UI_Window.CurrentArea.Height + 2,2,OLED_UNFILLED);
			OLED_ClearArea(OLED_UI_Window.CurrentArea.X,OLED_UI_Window.CurrentArea.Y,OLED_UI_Window.CurrentArea.Width,OLED_UI_Window.CurrentArea.Height);
		}
		//��ʾ����
		/**
		 * ������ʾ������
		 * X����ǰ�����������ʼ��X + �˴������õ����������?
		 * Y�� ��ǰ�����������ʼ��Y + ���嶥�����?
		 * Width����ǰ���ڿ��ȼ�ȥ 2*���ֱ߾� ��ȥ ���ݵĿ��� ��ȥ���ݺ����ֵľ���WINDOW_DATA_TEXT_DISTANCE
		 * Height���ַ����߶�
		 * ��ʾ���ֵ���ʼ����?��
		 * X�� ��ǰ�����������ʼ��X + �˴������õ����������?
		 * Y�� ��ǰ�����������ʼ��Y + ���嶥�����?
		 * Width���ַ�������
		 * Height���ַ����߶�
		 * */
        // �ȼ��㲻�������ݿ��ȵ��ַ���������ΪĬ�Ͽ���
		int16_t MaxLength  = OLED_UI_Window.CurrentArea.Width - 2*CurrentWindow->Text_FontSideDistance - WINDOW_DATA_TEXT_DISTANCE;
		int16_t DataLength = 0;
		//������������
		int8_t DataStyle = GetWindowDataStyle(CurrentWindow->Prob_Data_Int,CurrentWindow->Prob_Data_Float);
		//������������ݲ�Ϊ�գ���˵���û������˽�����������ʾ������?
		//�������ָ�벻Ϊ��?
		if(DataStyle != WINDOW_DATA_STYLE_NONE ){
			//�����������Ϊint16_t
			if (DataStyle == WINDOW_DATA_STYLE_INT)
			{
				//�����ַ��������Ƶ�������
				MaxLength = OLED_UI_Window.CurrentArea.Width - 2*CurrentWindow->Text_FontSideDistance - CalcStringWidth(ChineseFont,ASCIIFont,"%3d",*CurrentWindow->Prob_Data_Int)- WINDOW_DATA_TEXT_DISTANCE;
				//����������ռ�Ŀ���
				DataLength = CalcStringWidth(ChineseFont,ASCIIFont,"%3d",*CurrentWindow->Prob_Data_Int);
				//��ʾ����
				OLED_PrintfMixArea(
					//��������
					OLED_UI_Window.CurrentArea.X,
					OLED_UI_Window.CurrentArea.Y,
					OLED_UI_Window.CurrentArea.Width,
					OLED_UI_Window.CurrentArea.Height,
					OLED_UI_Window.CurrentArea.X + CurrentWindow->General_Width - 1 - CurrentWindow->Text_FontSideDistance - DataLength,
					OLED_UI_Window.CurrentArea.Y + CurrentWindow->Text_FontTopDistance,
					ChineseFont,ASCIIFont,
					"%3d",*CurrentWindow->Prob_Data_Int);

			}else{//����Ĭ����Ϊ�Ǹ���������
			
				//�����ַ��������Ƶ�������
				MaxLength = OLED_UI_Window.CurrentArea.Width - 2*CurrentWindow->Text_FontSideDistance - CalcStringWidth(ChineseFont,	ASCIIFont,	"%5.2f",*CurrentWindow->Prob_Data_Float) - WINDOW_DATA_TEXT_DISTANCE;
				//����������ռ�Ŀ���
				DataLength = CalcStringWidth(ChineseFont,ASCIIFont,"%5.2f",*CurrentWindow->Prob_Data_Float);
				//��ʾ����
				OLED_PrintfMixArea(
					//��������
					OLED_UI_Window.CurrentArea.X,
					OLED_UI_Window.CurrentArea.Y,
					OLED_UI_Window.CurrentArea.Width,
					OLED_UI_Window.CurrentArea.Height,
					OLED_UI_Window.CurrentArea.X + CurrentWindow->General_Width - 1 - CurrentWindow->Text_FontSideDistance - DataLength,
					OLED_UI_Window.CurrentArea.Y + CurrentWindow->Text_FontTopDistance,
					ChineseFont,ASCIIFont,
					"%5.2f",*CurrentWindow->Prob_Data_Float);
			}
			

			//��ʾ�����������?
			OLED_DrawRoundedRectangle(OLED_UI_Window.CurrentArea.X +CurrentWindow->Prob_SideDistance,
			OLED_UI_Window.CurrentArea.Y + OLED_UI_Window.CurrentArea.Height  - CurrentWindow->Prob_LineHeight - CurrentWindow->Prob_BottomDistance,
			OLED_UI_Window.CurrentArea.Width- 2*CurrentWindow->Prob_SideDistance,CurrentWindow->Prob_LineHeight,2,OLED_UNFILLED);
			//��ʾ���������ڲ���
			OLED_DrawRectangle(OLED_UI_Window.CurrentArea.X +CurrentWindow->Prob_SideDistance + 2,
							OLED_UI_Window.CurrentArea.Y + OLED_UI_Window.CurrentArea.Height  - CurrentWindow->Prob_LineHeight - CurrentWindow->Prob_BottomDistance + 2,
							OLED_UI_ProbWidth.CurrentDistance>=OLED_UI_Window.CurrentArea.Width- 2*CurrentWindow->Prob_SideDistance - 4  ?OLED_UI_Window.CurrentArea.Width- 2*CurrentWindow->Prob_SideDistance - 4: OLED_UI_ProbWidth.CurrentDistance  ,CurrentWindow->Prob_LineHeight-4,OLED_FILLED);
			if(DataStyle == WINDOW_DATA_STYLE_INT && CurrentWindow->Prob_MinData == 0 && CurrentWindow->Prob_MaxData == 100){
				int16_t PercentTextX = OLED_UI_Window.CurrentArea.X + (OLED_UI_Window.CurrentArea.Width - 24) / 2;
				int16_t PercentTextY = OLED_UI_Window.CurrentArea.Y + OLED_UI_Window.CurrentArea.Height - CurrentWindow->Prob_LineHeight - CurrentWindow->Prob_BottomDistance;
				OLED_ClearArea(PercentTextX - 1, PercentTextY, 26, CurrentWindow->Prob_LineHeight);
				OLED_Printf(PercentTextX, PercentTextY, OLED_6X8_HALF, "%3d%%", *CurrentWindow->Prob_Data_Int);
			}
		}
		if(CurrentWindow->Text_String != NULL){
			int16_t WindowTextStringLength = CalcStringWidth(ChineseFont,ASCIIFont,CurrentWindow->Text_String);
			//����ַ����Ŀ��ȳ���������޶�����
			if(WindowTextStringLength > MaxLength){
#if IF_WAIT_ANIMATION_FINISH
                if( OLED_UI_Window.CurrentArea.X == OLED_UI_Window.TargetArea.X &&
                        OLED_UI_Window.CurrentArea.Y == OLED_UI_Window.TargetArea.Y &&
                        OLED_UI_Window.CurrentArea.Width == OLED_UI_Window.TargetArea.Width &&
                        OLED_UI_Window.CurrentArea.Height == OLED_UI_Window.TargetArea.Height){
#endif
		    	CurrentWindow->_LineSlip-= (float)LINE_SLIP_SPEED;
#if IF_WAIT_ANIMATION_FINISH
                }
#endif
		}
			if(CurrentWindow->_LineSlip < -WindowTextStringLength){
		   	 	CurrentWindow->_LineSlip =  MaxLength + 1;
			}

			OLED_PrintfMixArea(
			        OLED_UI_Window.CurrentArea.X + CurrentWindow->Text_FontSideDistance,
			        OLED_UI_Window.CurrentArea.Y + CurrentWindow->Text_FontTopDistance,
			        MaxLength,
			        OLED_UI_Window.CurrentArea.Height,
					OLED_UI_Window.CurrentArea.X + CurrentWindow->Text_FontSideDistance + CurrentWindow->_LineSlip,
					OLED_UI_Window.CurrentArea.Y + CurrentWindow->Text_FontTopDistance,
					ChineseFont,ASCIIFont,CurrentWindow->Text_String);
			}

	}else{
		// ������ڶ����Ѿ��˳���Ļ���򽫵�ǰ����ָ��ָ��NULL
		CurrentWindow = NULL;
	}
	
}







/**
 * @brief ���˵����������ƶ�һ��
 * @param ��
 * @return ��
 */
void MenuItemsMoveUp(void){
	OLED_UI_PageStartPoint.TargetPoint.Y -= (OLED_UI_LineStep.TargetDistance + CurrentMenuPage->General_FontSize);
}
/**
 * @brief ���˵����������ƶ�һ��
 * @param ��
 * @return ��
 */
void MenuItemsMoveDown(void){
	OLED_UI_PageStartPoint.TargetPoint.Y += (OLED_UI_LineStep.TargetDistance + CurrentMenuPage->General_FontSize);
}
/**
 * @brief ���˵����������ƶ�һ��
 * @param ��
 * @return ��
 */
void MenuItemsMoveLeft(void){
	OLED_UI_PageStartPoint.TargetPoint.X -= (OLED_UI_LineStep.TargetDistance + CurrentMenuPage->Tiles_TileWidth);
}
/**
 * @brief ���˵����������ƶ�һ��
 * @param ��
 * @return ��
 */
void MenuItemsMoveRight(void){
	OLED_UI_PageStartPoint.TargetPoint.X += (OLED_UI_LineStep.TargetDistance + CurrentMenuPage->Tiles_TileWidth);
}
/**
 * @brief ��ȡ��ǰ�˵�ҳ��Ĳ�λ����?
 * @param ��
 * @return ��ǰ�˵�ҳ��Ĳ�λ����?
 */
int16_t GetCurrentMenuPageMaxSlotNum(void){
	return (int16_t)(CurrentMenuPage->List_MenuArea.Height - CurrentMenuPage->List_StartPointY+OLED_UI_LineStep.TargetDistance-1) / (OLED_UI_LineStep.TargetDistance + CurrentMenuPage->General_FontSize );
}



/**
 * @brief ����Ŀ��������
 * @param ��
 * @return ��
 */
void SetTargetCursor(void){
	//�����ǰҳ�������ΪList��
	if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
		//Ŀ����x�������?��ʼ��ӡҳ����ʼ���x�����?����ȥ1��Ϊ��ȷ����긲�ǵ��˵�������֡�
		OLED_UI_Cursor.TargetArea.X = OLED_UI_PageStartPoint.TargetPoint.X -1;

		//Ŀ����y�������?��ʼ��ӡҳ����ʼ���y����  ����  (����߶�?�� �о�)�� ��ǰҳ���ID�� ��1����ȥ1��Ϊ��ȷ����긲�ǵ��˵�������֡�
		OLED_UI_Cursor.TargetArea.Y = OLED_UI_PageStartPoint.TargetPoint.Y + 
		CurrentMenuPage->_ActiveMenuID * ( CurrentMenuPage->General_LineSpace + CurrentMenuPage->General_FontSize) -1 ;
		//Ŀ����߶ȵ���?����߶�?��2����2��Ϊ��ȷ����긲�ǵ��˵�������֡�
		OLED_UI_Cursor.TargetArea.Height = CurrentMenuPage->General_FontSize + 2;
		//Ŀ������ȵ���?��ǰ�ַ����Ŀ���	����2��Ϊ��ȷ����긲�ǵ��˵�������֡�
		//�������ǰ�?
		int8_t LinePerfixWidth = 0;
		if(CurrentMenuPage->List_IfDrawLinePerfix == true){
			LinePerfixWidth = GetOLED_Font(CurrentMenuPage->General_FontSize,ASCII) + LINEPERFIX_DISTANCE;
		}
		////��ѡ�����?
		int8_t RadioCompensationWidth = 0;
		//�����Ҫ���Ƶ�ѡ��?��BoolRadioBox��Ϊ��)
		if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_BoolRadioBox != NULL){
				RadioCompensationWidth = (GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE) + 2);
		}
		else if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_IntBox != NULL){
				char IntBoxValue[5]; // ���ڴ洢IntBox��ֵת������ַ���?
				// ��IntBox��ֵת��Ϊ�ַ���
				sprintf(IntBoxValue, "%d", *CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_IntBox);
				RadioCompensationWidth = (GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE) + (strlen(IntBoxValue) * 4));
		}
		else if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_FloatBox != NULL){
				char FloatBoxValue[6]; // ���ڴ洢IntBox��ֵת������ַ���?
				// ��IntBox��ֵת��Ϊ�ַ���
				sprintf(FloatBoxValue, "%.2f", *CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_FloatBox);
				RadioCompensationWidth = (GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE) + (strlen(FloatBoxValue) * 4));
		}
		else{
			RadioCompensationWidth = 0;
		}
		OLED_UI_Cursor.TargetArea.Width = 
		fmin((float)CalcStringWidth(
			//�ַ�������
			GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE),GetOLED_Font(CurrentMenuPage->General_FontSize,ASCII),CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_item_text) + 2 + LinePerfixWidth ,
			//��ǰҳ��Ŀ��ȼӵ�ǰҳ�����ʼ�����ȥ��ʼ��ӡҳ����ʼ���������?���ǹ��������ȼ�һ��������ǰ׺�Ŀ���
			OLED_UI_MenuFrame.CurrentArea.Width + OLED_UI_MenuFrame.CurrentArea.X - OLED_UI_PageStartPoint.CurrentPoint.X - 6 - LinePerfixWidth + LinePerfixWidth - RadioCompensationWidth) ;
	}
	//�����ǰҳ�������ΪTiles��
	if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
		//�����಻��Ҫ������ʾ����������Ϊ0.
		// SetCursorZero();
		OLED_UI_Cursor.TargetArea.X = CurrentMenuPage->Tiles_ScreenWidth/2 - CalcStringWidth(GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE),GetOLED_Font(CurrentMenuPage->General_FontSize,ASCII),CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_item_text)/2 - 1;
		OLED_UI_Cursor.TargetArea.Y = CurrentMenuPage->Tiles_ScreenHeight - CurrentMenuPage->General_FontSize - TILES_BOTTOM_DISTANCE - 1;
		OLED_UI_Cursor.TargetArea.Height = CurrentMenuPage->General_FontSize + 2;
		OLED_UI_Cursor.TargetArea.Width = CalcStringWidth(GetOLED_Font(CurrentMenuPage->General_FontSize,CHINESE),GetOLED_Font(CurrentMenuPage->General_FontSize,ASCII),CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_item_text) + 2;

	}
}
/**
 * @brief ����Ŀ��˵��߿�?
 * @param ��
 * @return ��
 */
void SetTargetMenuFrame(void){
	OLED_UI_MenuFrame.TargetArea.X = CurrentMenuPage->List_MenuArea.X;
	OLED_UI_MenuFrame.TargetArea.Y = CurrentMenuPage->List_MenuArea.Y;
	OLED_UI_MenuFrame.TargetArea.Width = CurrentMenuPage->List_MenuArea.Width;
	OLED_UI_MenuFrame.TargetArea.Height = CurrentMenuPage->List_MenuArea.Height;

}
/**
 * @brief ����ǰҳ���LineSlip ����Ϊ0
 * @param ��
 * @return ��
 */
void SetLineSplitZero(void){
	MenuPage* page = CurrentMenuPage;
	for(MenuID i = 0; i<GetMenuItemNum(page->General_MenuItems);i++){
		page->General_MenuItems[i]._LineSlip = 0;
	}
}

/**
 * @brief ����Ŀ��������߶�?
 * @param ��
 * @return ��
 */
void SetTargetScrollBarHeight(void){
	if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
		OLED_UI_ScrollBarHeight.TargetDistance = (float)CurrentMenuPage->List_MenuArea.Height*(CurrentMenuPage->_ActiveMenuID + 1)/GetMenuItemNum(CurrentMenuPage->General_MenuItems);
	}else if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
		OLED_UI_ScrollBarHeight.TargetDistance = (float)(CurrentMenuPage->Tiles_ScreenWidth *(CurrentMenuPage->_ActiveMenuID + 1)/GetMenuItemNum(CurrentMenuPage->General_MenuItems));
	}
}
/**
 * @brief ����Ŀ�����������?
 * @param ��
 * @return ��
 */
void SetTargetProbWidth(void){
	
	//ȷ����������
	int8_t DataStyle = GetWindowDataStyle(CurrentWindow->Prob_Data_Int,CurrentWindow->Prob_Data_Float);
	if(DataStyle != WINDOW_DATA_STYLE_NONE){
		if(DataStyle == WINDOW_DATA_STYLE_INT){
			OLED_UI_ProbWidth.TargetDistance = ((*CurrentWindow->Prob_Data_Int - CurrentWindow->Prob_MinData)/((float)CurrentWindow->Prob_MaxData - CurrentWindow->Prob_MinData ))*	(OLED_UI_Window.CurrentArea.Width- 2*CurrentWindow->Prob_SideDistance - 4);
		}else{
			OLED_UI_ProbWidth.TargetDistance = ((*CurrentWindow->Prob_Data_Float - CurrentWindow->Prob_MinData)/((float)CurrentWindow->Prob_MaxData - CurrentWindow->Prob_MinData ))*	(OLED_UI_Window.CurrentArea.Width- 2*CurrentWindow->Prob_SideDistance - 4);
		}
	}
	
}

/**
 * @brief ���ݵ�ǰҳ����������Ƿ������ǰ׺
 * @param page �˵�ҳ��ṹ��ָ��?
 * @param id �˵���ID��
 * @param CursorPoint ��������
 * @return ��
 */
void DrawLinePermix(MenuPage* page,MenuID i,OLED_Point* CursorPoint,OLED_Font ChineseFont,OLED_Font ASCIIFont){
	//�����Ҫ��ӡ��ǰ�?
	if(page->List_IfDrawLinePerfix == true){
		char* LinePerfixSymb = "";
		//�����ǰ�Ĳ˵������Ӳ˵���û�лص���������ô��ӡ��Ӧ�ķ���?
		if(CurrentMenuPage->General_MenuItems[i].General_SubMenuPage != NULL && page->General_MenuItems[i].General_callback == NULL && page->General_MenuItems[i].List_BoolRadioBox == NULL){
			LinePerfixSymb = SUBMENU_PREFIX;
		}else//����лص�����?
		if(CurrentMenuPage->General_MenuItems[i].General_SubMenuPage == NULL && page->General_MenuItems[i].General_callback != NULL && page->General_MenuItems[i].List_BoolRadioBox == NULL){
			LinePerfixSymb = FUNCTION_PREFIX;
		}else
		if(CurrentMenuPage->General_MenuItems[i].General_SubMenuPage == NULL && page->General_MenuItems[i].General_callback == NULL && page->General_MenuItems[i].List_BoolRadioBox != NULL)
		{
			LinePerfixSymb = RADIO_PREFIX;
		}else{
			LinePerfixSymb = NONE_PREFIX;
		}

		OLED_PrintfMixArea(//�����Ƶ������ڴ�ӡ����
					   //������ʼx����
				       OLED_UI_MenuFrame.CurrentArea.X ,
					   OLED_UI_MenuFrame.CurrentArea.Y,
					   OLED_UI_MenuFrame.CurrentArea.Width - 6,
					   OLED_UI_MenuFrame.CurrentArea.Height,
					   //��ӡ���ֵ�����
					   CursorPoint->X,
					   CursorPoint->Y,
					   //��ӡ���ֵĴ�С
					   ChineseFont,ASCIIFont,
					   //��ӡ���ֵ�����
					   LinePerfixSymb);
		
	}

}

/**
 * @brief ���ݴ���Ľṹ������ָ���ӡ�˵�Ԫ��
 * @param ��
 * @return ��
 */
void PrintMenuElements(void){
	//���ȿӡ�����ǰҳ��Ľṹ��ָ��ȡ�����������������������forѭ�����иı�ṹ��ָ�룬��ᵼ��forѭ�����������ʵ�NULLָ�룬����Ӳ��������ɿ���?
	MenuPage* page = CurrentMenuPage;
	OLED_ChangePoint TempTargetPoint = OLED_UI_PageStartPoint;
	OLED_ChangeArea TempTargetArea = OLED_UI_MenuFrame;
	
	//��ȡ��ǰҳ��Ĳ˵�������?
	MenuID num = GetMenuItemNum(page->General_MenuItems);
	//��ȡ�ı���С���ӵ�ǰҳ���������������ASCII��������������������
	OLED_Font ChineseFont = GetOLED_Font(page->General_FontSize,CHINESE),ASCIIFont = GetOLED_Font(page->General_FontSize,ASCII);
	//���������������?��ʼ��Ϊ��ǰ�˵��ṹ��Ŀ�ʼ��?
	OLED_Point CursorPoint = TempTargetPoint.CurrentPoint;
    
	//�����ǰҳ�������Ϊ�б���
	if(page->General_MenuType == MENU_TYPE_LIST){
		//��ǰ׺����
		int16_t LinePerfixWidth = 0;
		////��ѡ�����?
		int16_t RadioCompensationWidth = 0;

		//���Ʊ߿������һ���ȼӶ���Ϊ��ʹ����򲻻ᵲס�˵���������ʵ����������ڲ˵�����?
		if(page->List_IfDrawFrame == true){
			OLED_DrawRectangle(TempTargetArea.CurrentArea.X-1,TempTargetArea.CurrentArea.Y-1,	TempTargetArea.CurrentArea.Width+2,TempTargetArea.CurrentArea.Height+2,OLED_UNFILLED);
		}
		//�����Ҫ������ǰ�?
		if(page->List_IfDrawLinePerfix == true){
			//����ǰ׺��������ΪASCII������ȼ�LINEPERFIX_DISTANCE������ַ����ľ���?
			LinePerfixWidth = ASCIIFont + LINEPERFIX_DISTANCE;
		}


		//��ӡ�˵���
		for(MenuID i = 0; i<num;i++){
			
			if(CursorPoint.Y + ChineseFont <0 || CursorPoint.Y > OLED_HEIGHT){
				CursorPoint.Y += (page->General_FontSize + OLED_UI_LineStep.CurrentDistance);
				continue;
			}

			//��¼����ѭ�����ַ�������
			int16_t StringLength = CalcStringWidth(ChineseFont,ASCIIFont,page->General_MenuItems[i].General_item_text);

			//�������������ǰ�?
			DrawLinePermix(page,i,&CursorPoint,ChineseFont,ASCIIFont);

			//�����Ҫ���Ƶ�ѡ��?��BoolRadioBox��Ϊ��)
			if(page->General_MenuItems[i].List_BoolRadioBox != NULL){
				RadioCompensationWidth = (ChineseFont + 2);
				char* RadioBoxSymb = "";
				if(*page->General_MenuItems[i].List_BoolRadioBox == true){
					RadioBoxSymb = "��";
				}else{
					RadioBoxSymb = "��";
				}


				OLED_PrintfMixArea(//�����Ƶ������ڴ�ӡ����
							   //������ʼx���������ǰ׺���ȣ����������Զ�������ӡ��ǰ׺�Ŀռ�?
						       TempTargetArea.CurrentArea.X ,
							   TempTargetArea.CurrentArea.Y,
							   TempTargetArea.CurrentArea.Width - 6,
							   TempTargetArea.CurrentArea.Height,

							   //��ӡ���ֵ�����
							   CursorPoint.X + TempTargetArea.CurrentArea.Width - RadioCompensationWidth -9 ,
							   CursorPoint.Y,
							   //��ӡ���ֵĴ�С
							   ChineseFont,ASCIIFont,
							   //��ӡ���ֵ�����
							   RadioBoxSymb);
			}	
			// �����Ҫ��ʾIntBox��ֵ(��IntBox��Ϊ��)
			else if(page->General_MenuItems[i].List_IntBox != NULL){
				char IntBoxValue[5]; // ���ڴ洢IntBox��ֵת������ַ���?
				// ��IntBox��ֵת��Ϊ�ַ���
				sprintf(IntBoxValue, "%d", *page->General_MenuItems[i].List_IntBox);
				
				RadioCompensationWidth = (ChineseFont + (strlen(IntBoxValue) * 4));
							
					// �����Ƶ������ڴ�ӡIntBox��ֵ
				OLED_PrintfMixArea(
						TempTargetArea.CurrentArea.X, // ������ʼx���������ǰ׺����?
						TempTargetArea.CurrentArea.Y,
						TempTargetArea.CurrentArea.Width - 6,
						TempTargetArea.CurrentArea.Height,
						CursorPoint.X + TempTargetArea.CurrentArea.Width - RadioCompensationWidth -9, // ��ӡ���ֵ�����
						CursorPoint.Y,
						ChineseFont, ASCIIFont, // ��ӡ���ֵĴ�С
						IntBoxValue // ��ӡIntBox��ֵ
				);
			}
			// �����Ҫ��ʾFloatBox��ֵ(��FloatBox��Ϊ��)
			else if (page->General_MenuItems[i].List_FloatBox != NULL) {
					char FloatBoxValue[5]; // ���ڴ洢FloatBox��ֵת������ַ���?
					// ��FloatBox��ֵת��Ϊ�ַ�����������λС��
					sprintf(FloatBoxValue, "%.2f", *page->General_MenuItems[i].List_FloatBox);

					// ���㲹������
					RadioCompensationWidth = (ChineseFont + (strlen(FloatBoxValue) * 4));

					// �����Ƶ������ڴ�ӡFloatBox��ֵ
					OLED_PrintfMixArea(
							TempTargetArea.CurrentArea.X, // ������ʼx���������ǰ׺����?
							TempTargetArea.CurrentArea.Y,
							TempTargetArea.CurrentArea.Width - 6,
							TempTargetArea.CurrentArea.Height,
							CursorPoint.X + TempTargetArea.CurrentArea.Width - RadioCompensationWidth - 9, // ��ӡ���ֵ�����
							CursorPoint.Y,
							ChineseFont, ASCIIFont, // ��ӡ���ֵĴ�С
							FloatBoxValue // ��ӡFloatBox��ֵ
					);
			}
			else{
					RadioCompensationWidth = 0;
			}
			

			//����ַ����Ŀ��ȳ���������޶�����
			if(StringLength > (TempTargetArea.TargetArea.X + TempTargetArea.TargetArea.Width - 6 - 2) - TempTargetPoint.TargetPoint.X - LinePerfixWidth - RadioCompensationWidth ){
#if IF_WAIT_ANIMATION_FINISH
			    if(TempTargetArea.TargetArea.Width == TempTargetArea.CurrentArea.Width &&
					TempTargetArea.TargetArea.X == TempTargetArea.CurrentArea.X &&
					TempTargetArea.TargetArea.Height == TempTargetArea.CurrentArea.Height &&
					TempTargetArea.TargetArea.Y == TempTargetArea.CurrentArea.Y){
#endif
						page->General_MenuItems[i]._LineSlip -= (float)LINE_SLIP_SPEED;
#if IF_WAIT_ANIMATION_FINISH
					}
#endif
				
				
				//���page->MenuItems[i]._LineSlipС�ڸ����ַ�������?��˵���ַ����Ѿ����Ƶ�������
				if(page->General_MenuItems[i]._LineSlip < -StringLength){
					//��λ����������޶����ȵ��Ҳ�ոպÿ������ĵط�
					page->General_MenuItems[i]._LineSlip = (TempTargetArea.TargetArea.X + TempTargetArea.TargetArea.Width - 6) - CursorPoint.X - LinePerfixWidth;
				}


			}

			OLED_PrintfMixArea(TempTargetArea.CurrentArea.X + LinePerfixWidth + CurrentMenuPage->List_StartPointX ,
			 					TempTargetArea.CurrentArea.Y,
			 					TempTargetArea.CurrentArea.Width - 6 - LinePerfixWidth - CurrentMenuPage->List_StartPointX  -2 - RadioCompensationWidth,
			 					TempTargetArea.CurrentArea.Height,
								//�������LinePerfixWidth��Ϊ�˸���ǰ׺���¿ռ�
							   	CursorPoint.X + LinePerfixWidth + page->General_MenuItems[i]._LineSlip,
							   	CursorPoint.Y,
							   	ChineseFont,ASCIIFont,page->General_MenuItems[i].General_item_text);

			// ��ӡ�������?
			CursorPoint.Y += (page->General_FontSize + OLED_UI_LineStep.CurrentDistance);

		}

		//���ƹ���������������
		if(page->List_IfDrawFrame == true){
			OLED_DrawRectangle(TempTargetArea.CurrentArea.X + TempTargetArea.CurrentArea.Width - 5,TempTargetArea.CurrentArea.Y,5,(OLED_UI_ScrollBarHeight.CurrentDistance > page->List_MenuArea.Height) ?page->List_MenuArea.Height:OLED_UI_ScrollBarHeight.CurrentDistance,OLED_FILLED);
			OLED_DrawLine(TempTargetArea.CurrentArea.X + TempTargetArea.CurrentArea.Width - 3,TempTargetArea.CurrentArea.Y,TempTargetArea.CurrentArea.X + TempTargetArea.CurrentArea.Width-3,TempTargetArea.CurrentArea.Height + TempTargetArea.CurrentArea.Y-1);
		}
	}
	//�����ǰ��ҳ��Ϊ��������?
	if(page->General_MenuType == MENU_TYPE_TILES){
		
		//��ӡ������
		for(MenuID i = 0; i<num;i++){
			if(CursorPoint.X + page->Tiles_TileWidth < 0 || CursorPoint.X > OLED_WIDTH){
				CursorPoint.X += (page->Tiles_TileWidth + OLED_UI_LineStep.CurrentDistance);
				continue;
			}
			//������̬icon
			if (page->_ActiveMenuID == i && page->General_MenuItems[i].Tiles_GifIcon != NULL) {
        OLED_ShowImageArea(ceil(CursorPoint.X),CursorPoint.Y,page->Tiles_TileWidth,page->Tiles_TileHeight,0,0,page->Tiles_ScreenWidth,page->Tiles_ScreenHeight,page->General_MenuItems[i].Tiles_GifIcon == NULL?UnKnown:page->General_MenuItems[i].Tiles_GifIcon[page->General_MenuItems[i].Gif_index]);
				if(tmpi==GIFICON_SLIP_SPEED) page->General_MenuItems[i].Gif_index++,tmpi=0;//�����ٶ�
				else tmpi++;
				if(page->General_MenuItems[i].Gif_index>31) page->General_MenuItems[i].Gif_index=0;
      }else{
				//��ʾ����ͼ��
				OLED_ShowImageArea(ceil(CursorPoint.X),CursorPoint.Y,page->Tiles_TileWidth,page->Tiles_TileHeight,0,0,page->Tiles_ScreenWidth,page->Tiles_ScreenHeight,page->General_MenuItems[i].Tiles_Icon == NULL?UnKnown:page->General_MenuItems[i].Tiles_Icon);
			}
			// ��ӡ�������?
			CursorPoint.X += (page->Tiles_TileWidth + OLED_UI_LineStep.CurrentDistance);
			//��ʾ�˵�������
			
		}
		//����ָʾ��Arrow1
		OLED_ShowImageArea(page->Tiles_ScreenWidth/2 -16-2,4,36,36,0,0,page->Tiles_ScreenWidth,page->Tiles_ScreenHeight,Arrow1);
		//����ָʾ��ͷArrow
		//OLED_ShowImageArea(page->Tiles_ScreenWidth/2 - 3,0,6,5,0,0,page->Tiles_ScreenWidth,page->Tiles_ScreenHeight,Arrow);


		//��¼����ѭ�����ַ�������
		int16_t StringLength = CalcStringWidth(ChineseFont,ASCIIFont,page->General_MenuItems[page->_ActiveMenuID].General_item_text);
		//����ַ����Ŀ��ȴ����û������õ���Ļ����?
		if(StringLength > page->Tiles_ScreenWidth){
#if IF_WAIT_ANIMATION_FINISH
		    if(OLED_UI_PageStartPoint.CurrentPoint.X == OLED_UI_PageStartPoint.TargetPoint.X &&
		        OLED_UI_PageStartPoint.CurrentPoint.Y == OLED_UI_PageStartPoint.TargetPoint.Y ){
#endif
		        page->General_MenuItems[page->_ActiveMenuID]._LineSlip -= (float)LINE_SLIP_SPEED;
#if IF_WAIT_ANIMATION_FINISH
		    }
#endif
		}else{
		    SetLineSplitZero();
		}
		//���page->MenuItems[i]._LineSlipС�ڸ����ַ�������?��˵���ַ����Ѿ����Ƶ�������
		if(page->General_MenuItems[page->_ActiveMenuID]._LineSlip < -StringLength){
		     //��λ����������޶����ȵ��Ҳ�ոպÿ������ĵط�
		     page->General_MenuItems[page->_ActiveMenuID]._LineSlip = page->Tiles_ScreenWidth + 1;
		}

		OLED_PrintfMixArea(0,0,page->Tiles_ScreenWidth,page->Tiles_ScreenHeight,
		        StringLength > page->Tiles_ScreenWidth? 0 + page->General_MenuItems[page->_ActiveMenuID]._LineSlip : page->Tiles_ScreenWidth/2 - CalcStringWidth(ChineseFont,ASCIIFont,page->General_MenuItems[page->_ActiveMenuID].General_item_text)/2 + page->General_MenuItems[page->_ActiveMenuID]._LineSlip,
							   page->Tiles_ScreenHeight - page->General_FontSize - TILES_BOTTOM_DISTANCE,
							   ChineseFont,ASCIIFont,
							   page->General_MenuItems[page->_ActiveMenuID].General_item_text);
		//���ƹ���������������
		int16_t ScrollBarHeight = (page->Tiles_ScreenHeight >= 128? 5:3);
		OLED_DrawRectangle(0,TILES_STARTPOINT_Y + page->Tiles_TileHeight + TILES_SCROLLBAR_Y,
						OLED_UI_ScrollBarHeight.CurrentDistance,ScrollBarHeight,OLED_FILLED);
		OLED_DrawLine(0,TILES_STARTPOINT_Y + page->Tiles_TileHeight + TILES_SCROLLBAR_Y + ScrollBarHeight/2,page->Tiles_ScreenWidth-1,TILES_STARTPOINT_Y + page->Tiles_TileHeight + TILES_SCROLLBAR_Y + ScrollBarHeight/2);

		
		
	}
	if(page->General_ShowAuxiliaryFunction != NULL){
		//���Ƹ�������
		page->General_ShowAuxiliaryFunction();
	}
	
}



/**
 * @brief �������������¼����?
 * @param ��
 * @return ��һ������һ�ֵ�_ActiveMenuID��ֵ
 * @note ʵ����_ActiveMenuID��ֵ���䡣
  */
MenuID_Type OLED_KeyAndEncoderRecord(void){
    static bool IfUpTapAndHold = false;
    static bool IfDownTapAndHold = false;
	//��¼��һ�ְ���״̬
    OLED_UI_LastKey.Up=OLED_UI_Key.Up;
	OLED_UI_LastKey.Down=OLED_UI_Key.Down;
	OLED_UI_LastKey.Enter=OLED_UI_Key.Enter;
	OLED_UI_LastKey.Back=OLED_UI_Key.Back;
    
    //��¼��ǰ����״̬
	OLED_UI_Key.Enter = Key_GetEnterStatus();
	OLED_UI_Key.Back = Key_GetBackStatus();
	OLED_UI_Key.Up = Key_GetUpStatus();
	OLED_UI_Key.Down = Key_GetDownStatus();
#if IF_START_UP_AND_DOWN_LONG_PRESS

	static int16_t UpPressCounter = 0;
	static int16_t UpPressedCounter  = 0;
	static int16_t DownPressCounter = 0;
	static int16_t DownPressedCounter = 0;
	//�����һ�κ���һ�ζ����µ�״̬����?
	if(OLED_UI_LastKey.Up == 0 && OLED_UI_Key.Up == 0){
	    UpPressCounter++;
	    if(UpPressCounter >= (float)PRESS_TIME * OLED_UI_TICK_HZ){
	        UpPressedCounter ++;
	        if(UpPressedCounter >= (UpPressCounter >= (PRESS_TIME + CONTINUE_PRESS_TIME) * OLED_UI_TICK_HZ? CONTINUE_PRESS_MOVE_SPEED : PRESS_MOVE_SPEED)){
	            IfUpTapAndHold = true;
	            UpPressedCounter = 0;
	            IfUpTapAndHold = true;
	        }else{
	            IfUpTapAndHold = false;
	        }
	    }

	}else{
	    UpPressCounter = 0;
	    UpPressedCounter = 0;
	    IfUpTapAndHold = false;
	}
	//�����һ�κ���һ�ζ����µ�״̬����?
	    if(OLED_UI_LastKey.Down == 0 && OLED_UI_Key.Down == 0){
	        DownPressCounter++;

	        if(DownPressCounter >= (float)PRESS_TIME * OLED_UI_TICK_HZ){
	            DownPressedCounter ++;
	            if(DownPressedCounter >= (DownPressCounter >= (PRESS_TIME + CONTINUE_PRESS_TIME) * OLED_UI_TICK_HZ? CONTINUE_PRESS_MOVE_SPEED : PRESS_MOVE_SPEED)){
	                IfDownTapAndHold = true;
	                DownPressedCounter = 0;
	                IfDownTapAndHold = true;
	            }else{
	                IfDownTapAndHold = false;
	            }
	        }

	    }else{
	        DownPressCounter = 0;
	        DownPressedCounter = 0;
	        IfDownTapAndHold = false;
	    }

#endif
	if(OLED_UI_Key.Up == OLED_UI_Key.Down){
	    IfUpTapAndHold = false;
	    IfDownTapAndHold = false;

	}

	//��ȡ��ǰҳ��Ĳ˵�������?
	int16_t MaxID = GetMenuItemNum(CurrentMenuPage->General_MenuItems);
	MenuID_Type IncreaseID = {0,0};
	int16_t ActiveMenuID = CurrentMenuPage->_ActiveMenuID;
	int16_t LastActiveID = ActiveMenuID;
	//��ȡ������״̬
	ActiveMenuID += Encoder_Get();

	//�����⵽���ϡ���������һ״̬����ε�״̬��ͬ������һ״̬��̧��״̬��˵���û������ˡ��ϡ����������Ҹող�̧��?
	if((OLED_UI_Key.Up != OLED_UI_LastKey.Up && OLED_UI_Key.Up == 1) || IfUpTapAndHold){
		ActiveMenuID--;
	}

	//�����⵽���¡���������һ״̬����ε�״̬��ͬ������һ״̬��̧��״̬��˵���û������ˡ��¡����������Ҹող�̧��?
	if((OLED_UI_Key.Down != OLED_UI_LastKey.Down && OLED_UI_Key.Down == 1) || IfDownTapAndHold){
		ActiveMenuID++;
	}
	
	IncreaseID.Unsafe = ActiveMenuID - LastActiveID;
	//�����ǰ�˵���ID��Խ�磬����������0~MaxID-1֮��
	if(ActiveMenuID > MaxID-1){
		//ActiveMenuID = MaxID-1;
		ActiveMenuID = 0;
	}
	if(ActiveMenuID < 0){
		//ActiveMenuID = 0;
		ActiveMenuID = MaxID-1;
	}

	IncreaseID.Safe = ActiveMenuID - LastActiveID;
	return IncreaseID;
}
/**
 * @brief ��KeyEnterFlag��FLAGSTART�����ڱ�ǻص���������ִ��?
 * @param ��
 * @return ��
 */
void SetEnterFlag(void){
	KeyEnterFlag = FLAGSTART;
	Encoder_Disable();  // ʧ�ܱ�����
}

/**
 * @brief ��KeyEnterFlag��FLAGEND�����ڱ�ʾ�ص�����û������ִ��
 * @param void
 * @return ��
 */
void ResetEnterFlag(void){
	KeyEnterFlag = FLAGEND;
}


/**
 * @brief ��FadeOutFlag��FLAGSTART�����ڱ�ǽ���Ч������ִ��?
 * @param ��
 * @return ��
 */
void SetFadeOutFlag(MutexFlag action){
	FadeOutFlag = action;
	Encoder_Disable();  // ʧ�ܱ�����
}

/**
 * @brief ��FadeOutFlag��FLAGEND�����ڱ�ʾ����Ч��û������ִ��
 * @param void
 * @return ��
 */
void ResetFadeOutFlag(void){
	FadeOutFlag = FLAGEND;
}

/**
 * @brief �˺�����������Enter��������������Ϊ���¼��������?
 * 		1. ��ǰ�˵����лص���������ô�ͽ�KeyEnterFlag��λ��
 * 		2. ��ǰ�˵���û�лص��������������Ӳ˵�����ô�ͽ����Ӳ˵�
 * 		3. ��ǰ�˵���û�лص�������Ҳû���Ӳ˵�����ô��ʲô������
 * 		4. ��ǰ�˵�����лص������������Ӳ˵�����ô��ʲôҲ����?
 * @param ��
 * @return ��
 */
void EnterEventMenuItem(void){
    //�����ǰѡ�в˵����лص���������û���Ӳ˵�����ô�ͽ�KeyEnterFlag��λ
    if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_callback != NULL && CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage == NULL){

        SetEnterFlag();
    }
	//�����ǰѡ�в˵���û�лص��������������Ӳ˵�����ô��ȷ�ϱ�־�?
    if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage != NULL 
	&& CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_callback == NULL){
		SetFadeOutFlag(ENTER_FLAGSTART);
    }

}
/**
 * @brief ���¡����ء�������Ĳ���?
 * @param ��
 * @return ��
 */
void BackEventMenuItem(void){
	//�����ǰ�˵��ĸ��˵���Ϊ��?
	if(CurrentMenuPage->General_ParentMenuPage != NULL){
		//�÷��ر���λ
        SetFadeOutFlag(BACK_FLAGSTART);
    }
}
/**
 * @brief ���غ���
 * @param ��
 * @note ģ�ⰴ�¡����ء�����
 * @return ��
 */
void OLED_UI_Back(void){
	BackEventMenuItem();
}

/**
 * @brief ���벢���лص�����
 * @param ��
 * @return ��
 * @note ��OLED_UI����ѭ�������е���
 */
void RunCurrentCallBackFunction(void){
	//KeyEnterFlag == FLAGSTART���������ʾenter������
	if(KeyEnterFlag == FLAGSTART){
		//���ص�����ָ���Ƿ�Ϊ��?�������Ϊ�գ���ִ�лص�����?
		if (CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_callback != NULL){
			//ִ�лص�����
			CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_callback();	
		}
		//�ڻص�����ִ�����֮�󣬽�KeyEnterFlag��λ��
		ResetEnterFlag();
		Encoder_Enable();  // ʹ�ܱ�����
	}
}
/**
 * @brief ����ǰ���������Ŀ�������������?
 * @param ��
 * @return ��
 */
void SetCursorZero(void){
	OLED_UI_Cursor.TargetArea.X = 0;
	OLED_UI_Cursor.TargetArea.Y = 0;
	OLED_UI_Cursor.TargetArea.Width = 0;
	OLED_UI_Cursor.TargetArea.Height = 0;
	OLED_UI_Cursor.CurrentArea.X = 0;
	OLED_UI_Cursor.CurrentArea.Y = 0;
	OLED_UI_Cursor.CurrentArea.Width = 0;
	OLED_UI_Cursor.CurrentArea.Height = 0;
}
/**
 * @brief ���ô��ڴ�С
 * @param Width ���ڿ���
 * @param Height ���ڸ߶�
 * @return ��
 */
void OLED_UI_CreateWindow(MenuWindow* window){
	//�����ڱ�־λ����Ϊtrue����ʾ��ʼ����Ч��
	OLED_SustainCounter.SustainFlag = true;
	//����ǰ��������������
	OLED_UI_ProbWidth.CurrentDistance = 0;
	//���ô���Ч��ʱ�������?
	OLED_SustainCounter.count = 0;
	//������ڱ�־λ����λ��������Ŀ�괰�ڵ�λ�õ���Ļ����?
	
	//����Ŀ�괰�ڵ�λ��Ϊ��Ļ����������?
	OLED_UI_Window.TargetArea.Width = window->General_Width;
	OLED_UI_Window.TargetArea.Height = window->General_Height;
	OLED_UI_Window.TargetArea.X = (OLED_WIDTH - window->General_Width)/2;
	OLED_UI_Window.TargetArea.Y = (OLED_HEIGHT - window->General_Height)/2;
	window->_LineSlip = 0;
	//����ǰ����ָ��ָ��window
	CurrentWindow = window;
	
}




/**
 * @brief ���н���Ч��
 * @param ��
 * @return ��
 */
void RunFadeOut(void){

	static uint8_t FadeOut_Seq;
	static uint32_t FadeOut_Seq_StartTick;
	static int16_t FadeOut_x0, FadeOut_y0, FadeOut_width, FadeOut_height;

	/*�����ǰ��FadeOutFlag�Ѿ�����λ����˵���������н���Ч����
	��ǰ�����н���Ч����ǰ��������2����
	1.���ڰ���ȷ�ϼ�������¡��������ǰѡ�в˵���û�лص��������������Ӳ˵�������ʱ FadeOutFlag == ENTER_FLAGSTART
	2.���ڰ��·��ؼ�������¡��������ǰ�˵��ĸ��˵���Ϊ�ա�����ʱ FadeOutFlag == BACK_FLAGSTART
	*/
	if(FadeOutFlag != FLAGEND){
		if (FadeOut_Seq != 0){	//�����ǰ���ǲ���?
			if ((FadeOut_Seq_StartTick + FADEOUT_TIME) < HAL_GetTick()){	//��ʱFADEOUT_TIME����
				FadeOut_Seq++;
				FadeOut_Seq_StartTick = HAL_GetTick();	//��¼ÿһ���Ŀ�ʼʱ��
			}
		}
		if (FadeOut_Seq == 0){	//����0������Ч������
			//�����ǰ�˵����б���?
			if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
				//��ǰ�˵����ҳ���������б��������£�������ȷ�ϰ���?
				if(FadeOutFlag == ENTER_FLAGSTART){
					//�������Ӳ˵�������£������ǰҳ��ĵ�ǰ�Ӳ˵����ҳ���������б���
					if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage->General_MenuType == MENU_TYPE_LIST){
						//ֻ�����ǰҳ������?
						//OLED_UI_FadeoutCurrentArea(CurrentMenuPage->List_MenuArea.X,CurrentMenuPage->List_MenuArea.Y,CurrentMenuPage->List_MenuArea.Width-5,CurrentMenuPage->List_MenuArea.Height-2);
						FadeOut_x0 = CurrentMenuPage->List_MenuArea.X;
						FadeOut_y0 = CurrentMenuPage->List_MenuArea.Y;
						FadeOut_width = CurrentMenuPage->List_MenuArea.Width-5;
						FadeOut_height = CurrentMenuPage->List_MenuArea.Height-2;
					}else{
						//���ȫ������?
						//OLED_UI_FadeoutAllArea();
						FadeOut_x0 = 0;
						FadeOut_y0 = 0;
						FadeOut_width = OLED_WIDTH;
						FadeOut_height = OLED_HEIGHT;
					}
				}
				//��ǰ�˵����ҳ���������б��������£�������ȡ������?
				if(FadeOutFlag == BACK_FLAGSTART) {
					//�����ǰҳ��ĸ��˵����ҳ���������б���?
					if(CurrentMenuPage->General_ParentMenuPage->General_MenuType == MENU_TYPE_LIST){
						//ֻ�����ǰҳ������?
						//OLED_UI_FadeoutCurrentArea(CurrentMenuPage->List_MenuArea.X,CurrentMenuPage->List_MenuArea.Y,CurrentMenuPage->List_MenuArea.Width-5,CurrentMenuPage->List_MenuArea.Height-2);
						FadeOut_x0 = CurrentMenuPage->List_MenuArea.X;
						FadeOut_y0 = CurrentMenuPage->List_MenuArea.Y;
						FadeOut_width = CurrentMenuPage->List_MenuArea.Width-5;
						FadeOut_height = CurrentMenuPage->List_MenuArea.Height-2;

					}else{
						//���ȫ������?
						//OLED_UI_FadeoutAllArea();
						FadeOut_x0 = 0;
						FadeOut_y0 = 0;
						FadeOut_width = OLED_WIDTH;
						FadeOut_height = OLED_HEIGHT;
					}
				}

			}else //�����ǰ�˵������Ǵ�����?
			if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
				//���ȫ������?
				//OLED_UI_FadeoutAllArea();
				FadeOut_x0 = 0;
				FadeOut_y0 = 0;
				FadeOut_width = OLED_WIDTH;
				FadeOut_height = OLED_HEIGHT;

			}
			FadeOut_Seq++;
		}else
		if(FadeOut_Seq == 6){	//����6��������ϣ���λ����?
			OLED_UI_FadeOut_Masking(FadeOut_x0, FadeOut_y0, FadeOut_width, FadeOut_height, 5);	//��һ֡Ӧ�벽��5һ����ʾȫ��
			FadeOut_Seq = 0;
			//�����ǰ�˵����б���?
			if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
				//��ǰ�˵����ҳ���������б��������£�������ȷ�ϰ���?
				if(FadeOutFlag == ENTER_FLAGSTART){
					//�������Ӳ˵�������£������ǰҳ��ĵ�ǰ�Ӳ˵����ҳ���������б���
					if(CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage->General_MenuType == MENU_TYPE_LIST){
					}else{
						//���������ĵ�ǰ�߶���Ϊ0
						OLED_UI_ScrollBarHeight.CurrentDistance = 0;
					}

					//����ǰ�˵���λ�ñ��棬�Ա㷵��ʱ�ָ�
					CurrentMenuPage->_StartPoint.X = OLED_UI_PageStartPoint.TargetPoint.X;
					CurrentMenuPage->_StartPoint.Y = OLED_UI_PageStartPoint.TargetPoint.Y;

					//����ǰ�˵���ָ��ָ���Ӳ˵�
					CurrentMenuPage = CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage;
					//�Ե�ǰ�Ӳ˵����г�ʼ��
					CurrentMenuPageInit();
				}
				//��ǰ�˵����ҳ���������б��������£�������ȡ������?
				if(FadeOutFlag == BACK_FLAGSTART) {
					//�����ǰҳ��ĸ��˵����ҳ���������б���?
					if(CurrentMenuPage->General_ParentMenuPage->General_MenuType == MENU_TYPE_LIST){
					}else{
						//���������ĵ�ǰ�߶���Ϊ0
						OLED_UI_ScrollBarHeight.CurrentDistance = 0;
					}
					//����ǰ�˵���ָ��ָ�򸸲˵�
					CurrentMenuPage = CurrentMenuPage->General_ParentMenuPage;
					//����ǰ�˵���λ�õȲ����ָ�
					CurrentMenuPageBackUp();
				}

				//��FadeOutFlag��λ
				ResetFadeOutFlag();
				//����ǰ���������Ŀ�������������?
				SetCursorZero();

			}else //�����ǰ�˵������Ǵ�����?
			if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
				//��ǰ�˵����ҳ�������Ǵ����������£�������ȷ�ϲ���?
				if(FadeOutFlag == ENTER_FLAGSTART){
					//����ǰ�˵���λ�ñ��棬�Ա㷵��ʱ�ָ�
					CurrentMenuPage->_StartPoint.X = OLED_UI_PageStartPoint.TargetPoint.X;
					CurrentMenuPage->_StartPoint.Y = OLED_UI_PageStartPoint.TargetPoint.Y;
					//����ǰ�˵���ָ��ָ���Ӳ˵�
					CurrentMenuPage = CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].General_SubMenuPage;
					//�Ե�ǰ�Ӳ˵����г�ʼ��
					CurrentMenuPageInit();

				}
				//��ǰ�˵����ҳ�������Ǵ����������£������˷��ز���?
				if(FadeOutFlag == BACK_FLAGSTART){
					//����ǰ�˵���ָ��ָ�򸸲˵�
					CurrentMenuPage = CurrentMenuPage->General_ParentMenuPage;
					//����ǰ�˵���λ�õȲ����ָ�
					CurrentMenuPageBackUp();
				}
				//���������ĵ�ǰ�߶���Ϊ0
				OLED_UI_ScrollBarHeight.CurrentDistance = 0;
				//����ǰ���������Ŀ�������������?
				SetCursorZero();
				//��FadeOutFlag��λ
				ResetFadeOutFlag();
			}
			//����ǰ���������Ŀ�������������?
			SetCursorZero();
			//��FadeOutFlag��λ
			ResetFadeOutFlag();
			// ʹ�ܱ�����
			Encoder_Enable();
		}
		else{					//����1-5��������
			OLED_UI_FadeOut_Masking(FadeOut_x0, FadeOut_y0, FadeOut_width, FadeOut_height, FadeOut_Seq);
		}
	}
}
/**
 * @brief ��ӡ�˵�Ԫ�ز�����Ŀ��ֵ�ı�Ԫ�صĲ���
 * @param ��
 * @return ��
 */
void MoveMenuElements(void){

	//����Ŀ��������
	SetTargetCursor();
	//����Ŀ��˵��߿�?
	SetTargetMenuFrame();
	//����Ŀ�����������?
	SetTargetProbWidth();
	//����Ŀ��������߶�?
	SetTargetScrollBarHeight();


	// �ı�˵���ʼԪ�ص�����?
	ChangePoint(&OLED_UI_PageStartPoint);
	// �ı�˵�����м��?
	ChangeDistance(&OLED_UI_LineStep);
	// �ı�������߶�?
	ChangeDistance(&OLED_UI_ScrollBarHeight);
	
	

	// �ı�˵��߿����
	ChangeArea(&OLED_UI_MenuFrame);

	// ��ӡ�˵�ҳ���Ԫ��?
	PrintMenuElements();

	//�ı���Ĳ���?
	ChangeArea(&OLED_UI_Cursor);
	//��ʾ���?
	ReverseCoordinate(OLED_UI_Cursor.CurrentArea.X,OLED_UI_Cursor.CurrentArea.Y,OLED_UI_Cursor.CurrentArea.Width,OLED_UI_Cursor.CurrentArea.Height,CurrentMenuPage->General_CursorStyle);
	//������ɫģʽ
	OLED_SetColorMode(ColorMode);

	OLED_Brightness(OLED_UI_Brightness);

	ChangeDistance(&OLED_UI_ProbWidth);
	// �ı䴰�ڲ���
	ChangeArea(&OLED_UI_Window);
	// ���ƴ���
	OLED_DrawWindow();


	
	
}

/**
 * @brief OLED_UI����ѭ������
 * @param ��
 * @note �ú�����Ҫ������ѭ���е���?���Ա�ʵ��UI��ˢ��
 * @return ��
 */
void OLED_UI_MainLoop(void){

	

	//����
	OLED_Clear();

	

	//�ƶ��˵�Ԫ��
	MoveMenuElements();

	
	//������������λʱ�����е�ǰ�˵���Ļص�����?
	RunCurrentCallBackFunction();
	
	//����������������λʱ�����н���Ч��
	RunFadeOut();

	//��ʾFPS
	OLED_UI_ShowFPS();
	//ˢ��
	OLED_Update();
	
}




/**
 * @brief  OLED_UI���жϺ������ڲ����������ж��ڴ���������
 * @param  ��
 * @return ��
 */
void OLED_UI_InterruptHandler(void){
	// ��ȡ��ǰ��Ļˢ����
    GetFPS();
    
	// �����ǰ������ִ�еĻص��������򲻴����ж��ڵ�����?
    if(GetEnterFlag() && GetFadeoutFlag()){
    	

		//��ȡ_ActiveMenuID�ı仯ֵ��_ActiveMenuID��ֵ���䣬����¼�˰����ı仯
		MenuID_Type IncreaseID = OLED_KeyAndEncoderRecord();


		//��������?����ı�־λΪtrue��˵����ǰ�������д���
		if(OLED_SustainCounter.SustainFlag == true){
			//������������ǰ����ı仯ֵ����?
			if(IncreaseID.Unsafe != 0){
				//���ڼ���ֵ����
				OLED_SustainCounter.count = 0;
			}
			//�������������?
			int8_t DataStyle = GetWindowDataStyle(CurrentWindow->Prob_Data_Int,CurrentWindow->Prob_Data_Float);
			if(DataStyle != WINDOW_DATA_STYLE_NONE){
				if(DataStyle == WINDOW_DATA_STYLE_INT){
					*CurrentWindow->Prob_Data_Int += (IncreaseID.Unsafe * CurrentWindow->Prob_DataStep);
					if(*CurrentWindow->Prob_Data_Int < CurrentWindow->Prob_MinData) {*CurrentWindow->Prob_Data_Int = CurrentWindow->Prob_MinData;}
					if(*CurrentWindow->Prob_Data_Int > CurrentWindow->Prob_MaxData) {*CurrentWindow->Prob_Data_Int = CurrentWindow->Prob_MaxData;}
				}else{
					*CurrentWindow->Prob_Data_Float += (IncreaseID.Unsafe * CurrentWindow->Prob_DataStep);
					if(*CurrentWindow->Prob_Data_Float < CurrentWindow->Prob_MinData) {*CurrentWindow->Prob_Data_Float = CurrentWindow->Prob_MinData;}
					if(*CurrentWindow->Prob_Data_Float > CurrentWindow->Prob_MaxData) {*CurrentWindow->Prob_Data_Float = CurrentWindow->Prob_MaxData;}
				}
			}
			IncreaseID.Safe = 0;
		}
		 
		//����仯ֵС��?����ô�൱�ڰ���IncreaseID.Safe�Ρ��ϡ�����
		if(IncreaseID.Safe < 0 ){
			for(int i = 0; i < -IncreaseID.Safe; i++){
				//�����ǰ�˵��������б���?
				if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
					/*********************���¡��ϡ��������еĲ���*************************/
					//�����ǰ�˵�ҳ��Ĺ���Ѿ���������Ĳ��?���ҵ�ǰ�˵���ǵ�һ���˵����ô�������ƶ��˵����Ŀ��λ��?
           			if(CurrentMenuPage->_Slot == 0 && CurrentMenuPage->_ActiveMenuID !=0){
						MenuItemsMoveDown();

					}
					//�����껹û�е�������Ĳ�λ����ô�������ƶ���λ
					if(CurrentMenuPage->_Slot > 0){
						CurrentMenuPage->_Slot--;
					}
					CurrentMenuPage->_ActiveMenuID--;
				}
				//�����ǰ�˵��������б���?
				if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
					CurrentMenuPage->_ActiveMenuID--;
					MenuItemsMoveRight();
				}
			}
		}
		if(IncreaseID.Safe > 0){
			for(int i = 0; i < IncreaseID.Safe; i++){
				/*********************���¡��¡��������еĲ���*************************/
					if(CurrentMenuPage->General_MenuType == MENU_TYPE_LIST){
					//�����ǰ�˵�ҳ��Ĺ���Ѿ�������ײ��Ĳ�λ,���ҵ�ǰ�˵�������һ���˵����ô�������ƶ��˵����Ŀ��λ��
					if(CurrentMenuPage->_Slot == GetCurrentMenuPageMaxSlotNum()-1 && CurrentMenuPage->_ActiveMenuID != GetMenuItemNum(CurrentMenuPage->General_MenuItems)){
						MenuItemsMoveUp();

					}
					//�����껹û�е�����ײ��Ĳ�λ����ô�������ƶ����?
					if(CurrentMenuPage->_Slot < GetCurrentMenuPageMaxSlotNum()-1){
						CurrentMenuPage->_Slot++;
					}
					CurrentMenuPage->_ActiveMenuID++;
				}
				//�����ǰ�˵��������б���?
				if(CurrentMenuPage->General_MenuType == MENU_TYPE_TILES){
					CurrentMenuPage->_ActiveMenuID++;
					MenuItemsMoveLeft();
				}

			}
		}
		
	
    	
		//�����⵽�����ء���������һ״̬����ε�״̬��ͬ������һ״̬��̧��״̬��˵���û������ˡ����ء����������Ҹող�̧��?
		if(OLED_UI_Key.Back != OLED_UI_LastKey.Back && OLED_UI_Key.Back == 1){
			//�����ǰû�����д���?
			if(OLED_SustainCounter.SustainFlag == false){
				BackEventMenuItem();
			}else{
				OLED_SustainCounter.count = (int16_t)(CurrentWindow->General_ContinueTime * OLED_UI_TICK_HZ);
			}
			
		}
		//�����⵽��ȷ�ϡ���������һ״̬����ε�״̬��ͬ������һ״̬��̧��״̬��˵���û������ˡ�ȷ�ϡ����������Ҹող�̧��?
		if(OLED_UI_Key.Enter != OLED_UI_LastKey.Enter && OLED_UI_Key.Enter == 1){
			if(OLED_SustainCounter.SustainFlag == false){
				EnterEventMenuItem();
				if (CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_BoolRadioBox != NULL) {
				    *CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_BoolRadioBox = !(*CurrentMenuPage->General_MenuItems[CurrentMenuPage->_ActiveMenuID].List_BoolRadioBox);
				}
			}else{
				OLED_SustainCounter.count = 0;
			}
		}
		
	}

	//�����ǰ�������д��ڣ���ô����?
	if(OLED_SustainCounter.SustainFlag == true){
		OLED_SustainCounter.count++;
	}
	if(CurrentWindow != NULL){
		if(OLED_SustainCounter.count >= (int16_t)(CurrentWindow->General_ContinueTime * OLED_UI_TICK_HZ)){
			OLED_SustainCounter.SustainFlag = false;
			OLED_SustainCounter.count = 0;
		}
	}
}
#endif




