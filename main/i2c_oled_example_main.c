/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "misc/lv_color.h"
#include "driver/uart.h"







#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

static const char *TAG = "example";

#define I2C_BUS_PORT  0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ    (400 * 1000)              //时钟频率
#define EXAMPLE_PIN_NUM_SDA           3                         //IIC数据线
#define EXAMPLE_PIN_NUM_SCL           4                         //IIC时钟线
#define EXAMPLE_PIN_NUM_RST           -1                        //IIC复位引脚
#define EXAMPLE_I2C_HW_ADDR           0x3C                      //IIC地址

// The pixel number in horizontal and vertical
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306                       //检查是否有宏定义
#define EXAMPLE_LCD_H_RES              128                      //设置水平分辨率
#define EXAMPLE_LCD_V_RES              CONFIG_EXAMPLE_SSD1306_HEIGHT    //设置垂直分辨率
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
#define EXAMPLE_LCD_H_RES              64
#define EXAMPLE_LCD_V_RES              128
#endif
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8







#include "driver/gpio.h"
#include "led_strip.h"


static led_strip_handle_t led_strip;





static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = 4,
        .max_leds = 9, // at least one LED on board，一个引脚控制九个灯，是因为ESP32一次性发一整串数据，每个灯先拿走属于自己的RGB数据然后接力传输数据
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));          //读取配置到led_strip操作句柄，往后点灯等操作都能直接调用其相关函数，例如led_strip_set_pixel(led_strip, ...)等（来源于官方库函数）
    led_strip_clear(led_strip);         //熄灭，防止乱亮灯
}




extern void example_lvgl_demo_ui(lv_disp_t *disp);          //计算三个角并显示
void DisplayAframe();
 lv_disp_t *disp;                           //lv_disp_t是lvgl里一个显示设备的结构体
 static lv_style_t style_line;              //static表示该变量不会因函数结束而被杀死
void oled_init(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = EXAMPLE_PIN_NUM_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = EXAMPLE_I2C_HW_ADDR,
        .scl_speed_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = EXAMPLE_LCD_CMD_BITS, // According to SSD1306 datasheet
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
        .dc_bit_offset = 0,                     // According to SH1107 datasheet,offset=0表示dc引脚不进行位偏移
        .flags =
        {
            .disable_control_phase = 1,         //关闭SPI或并口的额外控制相位（额外时钟周期）
        }
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;     //定义一个屏幕驱动的句柄，后面再赋值
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,                        //每个像素用1位表示（单色，如果是彩色则应该为8、16或24）
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,      //复位引脚
    };
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SSD1306
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = EXAMPLE_LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = true,
        }
    };
     disp = lvgl_port_add_disp(&disp_cfg);

    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);

    ESP_LOGI(TAG, "Display LVGL Scroll Text");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    
 
    lv_style_init(&style_line);
 
    lv_style_set_line_width(&style_line, 1);
 
    lv_style_set_line_color(&style_line, lv_color_black());
 
    //lv_style_set_line_rounded(&style_line, true);
DisplayAframe();
}

void DisplayAframe(){
        if (lvgl_port_lock(0)) {            //给lvgl上锁，供自己用，如果无法上锁说明其他进程在调用，直接返回
        example_lvgl_demo_ui(disp);         //更新
        // Release the mutex
        lvgl_port_unlock();                 //用完解锁给其他进程调用
    }
}
#include <imu_data_decode.h>
#include <packet.h>
    static const char *RX_TASK_AAATAG = "IMU_TASK";
void DisplayThread(){
    while(1){
        DisplayAframe();
        //ESP_LOGI(RX_TASK_AAATAG, "p:%2.2f,y:%2.2f,r:%2.2f",id0x91.eul[0],id0x91.eul[1],id0x91.eul[2]);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}





#include "lvgl.h"
lv_obj_t *scr=NULL;
lv_obj_t *label=NULL;
lv_obj_t* lines =NULL;
lv_obj_t * arc =NULL;
lv_obj_t *label_roll=NULL;
lv_obj_t *label_yaw=NULL;
lv_obj_t * label_n =NULL;
lv_obj_t * label_e =NULL;
lv_obj_t * label_s =NULL;
lv_obj_t * label_w =NULL;

#include <math.h>



struct {
    lv_obj_t* lines;                    //线
    lv_point_t line_points[2];          //线的起终点坐标
    lv_obj_t *label;                    //文字
}linespitch[4];                         //一次性创建四组




#define lv_font_montserrat_8 lv_font_montserrat_14





void example_lvgl_demo_ui(lv_disp_t *disp)  //lv_disp_t是lvgl里表示一个显示设备的结构体
{
    if(!scr){
        scr = lv_disp_get_scr_act(disp);
        label_n = lv_label_create(scr);
    lv_label_set_text(label_n, "0");
        label_e = lv_label_create(scr);
    lv_label_set_text(label_e, "-90"); 
       label_s = lv_label_create(scr);
    lv_label_set_text(label_s, "180");
       label_w = lv_label_create(scr);
    lv_label_set_text(label_w, "90");
    lv_obj_align(label_n, LV_ALIGN_CENTER, 48+0, -12+10);
    

    lv_obj_align(label_e, LV_ALIGN_CENTER, 48-10, -12+0);
    

    lv_obj_align(label_s, LV_ALIGN_CENTER, 48+0, -12-10);
    

    lv_obj_align(label_w, LV_ALIGN_CENTER, 48+10, -12+0);





        label = lv_label_create(scr);    
        lv_obj_set_style_text_font(label, &lv_font_montserrat_8, LV_STATE_DEFAULT);    
        lv_obj_set_width(label, disp->driver->hor_res);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
        label_roll = lv_label_create(scr);
        lv_obj_set_style_text_font(label_roll, &lv_font_montserrat_8, LV_STATE_DEFAULT);
        lv_obj_align(label_roll, LV_ALIGN_CENTER,0, 0);
        label_yaw = lv_label_create(scr);
        lv_obj_set_style_text_font(label_yaw, &lv_font_montserrat_8, LV_STATE_DEFAULT);
        lv_obj_align(label_yaw, LV_ALIGN_CENTER,0, -28);

        lv_obj_t * yawlabline = lv_line_create(scr);
        lv_obj_add_style(yawlabline, &style_line, 0);
        static lv_point_t line_points[] = {{75,3},{105,3},{110,5}};     //坐标点数组
        lv_line_set_points(yawlabline, line_points, sizeof(line_points) / sizeof(lv_point_t));      //将三个点的坐标送入绘制线的函数


        lines = lv_line_create(scr);
        lv_obj_add_style(lines, &style_line, 0);

        arc = lv_arc_create(scr);
        lv_obj_set_size(arc, 28, 28);
        lv_arc_set_angles(arc, 0, 360);
        lv_obj_align(arc, LV_ALIGN_CENTER,48, -12);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x000000), LV_PART_INDICATOR|LV_STATE_DEFAULT);
        lv_obj_set_style_arc_width(arc, 1, LV_PART_INDICATOR|LV_STATE_DEFAULT);
        
        for (size_t i = 0; i < 4; i++)
        {
            linespitch[i].lines = lv_line_create(scr);
            lv_obj_add_style(linespitch[i].lines, &style_line, 0);
            linespitch[i].line_points[0].x=20;
            linespitch[i].line_points[0].y=32+10*i;
            linespitch[i].line_points[1].x=30;
            linespitch[i].line_points[1].y=32+10*i;
            lv_line_set_points(linespitch[i].lines, linespitch[i].line_points, 2);
            linespitch[i].label = lv_label_create(scr);
            lv_obj_set_style_text_font(linespitch[i].label, &lv_font_montserrat_8, LV_STATE_DEFAULT);
            lv_obj_align(linespitch[i].label, LV_ALIGN_CENTER,-64+40,-32+32+10*i);
            lv_label_set_text(label, "22.0");
        }



        
    }

    static char sndsd[50];
    snprintf(sndsd,50,"%2.1f\n%2.1f\n%2.1f",id0x91.eul[0],id0x91.eul[1],id0x91.eul[2]);  
    lv_label_set_text(label, sndsd);

    float trueroll=id0x91.eul[1];


    if(id0x91.eul[0]>90){
        trueroll=id0x91.eul[0];
    }
    if(id0x91.eul[0]<-90){
        trueroll=id0x91.eul[0];
    }

    int ax=cos((trueroll/180)*3.1415926)*32;
    int ay=sin((trueroll/180)*3.1415926)*32;

    static char snroll[10];
    snprintf(snroll,10,"%2.1f",id0x91.eul[1]);
    lv_label_set_text(label_roll, snroll);
    lv_obj_align(label_roll, LV_ALIGN_CENTER,-ay/2, -ax/2);
    static char snyaw[10];
    snprintf(snyaw,10,"%2.1f",id0x91.eul[2]);
    lv_label_set_text(label_yaw, snyaw);
   


    static lv_point_t line_points[] = {{},{},{},{},  {}};
    line_points[0].x=64+ax;line_points[0].y=32-ay;
    line_points[1].x=64+ax/8;line_points[1].y=32-ay/8;
    line_points[2].x=64+ay/8;line_points[2].y=32+ax/8;
    line_points[3].x=64-ax/8;line_points[3].y=32+ay/8;
    line_points[4].x=64-ax;line_points[4].y=32+ay;
    lv_line_set_points(lines, line_points, sizeof(line_points) / sizeof(lv_point_t));


    
    ax=sin((id0x91.eul[2]/180)*3.1415926)*10;
    ay=-cos((id0x91.eul[2]/180)*3.1415926)*10;

    lv_obj_align(label_n, LV_ALIGN_CENTER, 48+ax, -12+ay);
    lv_obj_align(label_e, LV_ALIGN_CENTER, 48-ay, -12+ax);
    lv_obj_align(label_s, LV_ALIGN_CENTER, 48-ax, -12-ay);
    lv_obj_align(label_w, LV_ALIGN_CENTER, 48+ay, -12-ax);
    



    

    
}


static const int RX_BUF_SIZE = 512;

#define TXD_PIN (GPIO_NUM_39)
#define RXD_PIN (GPIO_NUM_40)

#define TX2D_PIN (GPIO_NUM_15)
#define RX2D_PIN (GPIO_NUM_16)


void init(void)                                     //初始化两个串口UART1和UART2
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,                        //波特率
        .data_bits = UART_DATA_8_BITS,              //八个数据位
        .parity = UART_PARITY_DISABLE,              //无校验位
        .stop_bits = UART_STOP_BITS_1,              //一位停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      //无硬件流控
        .source_clk = UART_SCLK_DEFAULT,            //默认时钟
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);        //安装UART1驱动，两倍接收缓冲区，无发送缓冲
    uart_param_config(UART_NUM_1, &uart_config);                            //将上面配置传给UART1
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);


    const uart_config_t uart_config2 = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config2);
    uart_set_pin(UART_NUM_2, GPIO_NUM_15, GPIO_NUM_16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);







}



static void rx_task(void *arg)              //FreeRTOS任务
{


    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);       //设置日志标签和级别，方便调试打印

    imu_data_decode_init();                             //初始化imu数据解码器
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE);     //申请内存
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);      //从UART1读取数据，放到data，最后一个参数是超时时间
        if (rxBytes > 0) {
            for(int i=0;i<rxBytes;i++){
                packet_decode(data[i]);                 //读到数据就给packet_decode
                
            }
            
            //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

void imu216_main(void)
{
    init();                                             //第381行初始化函数直接调用
    xTaskCreate(rx_task,                                //任务函数
                "uart_rx_task",                         //任务名称
                1024 * 2,                               //任务栈大小2KB
                NULL,                                   //不需要传参
                configMAX_PRIORITIES - 1,               //任务优先级
                NULL);                                  //不用任务句柄
}





int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_2, data, len);            //从UART2发送数据，来源于data，函数返回长度而非data的值
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)                              //串口发送测试任务，验证UART2是否能正常发数据
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, "Hello world");               //发送字符串（sendData就是上面的函数，从UART2发送数据）
        vTaskDelay(20000 / portTICK_PERIOD_MS);             //每20秒发送一次
    }
}

#include <math.h>

// HSV到RGB转换函数
void hsv_to_rgb(int h, int s, int v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float hp = h / 60.0;
    float c = (s / 255.0) * (v / 255.0);
    float x = c * (1 - fabs(fmod(hp, 2) - 1));
    
    float r1, g1, b1;
    if (hp >= 0 && hp < 1) { r1 = c; g1 = x; b1 = 0; }
    else if (hp >= 1 && hp < 2) { r1 = x; g1 = c; b1 = 0; }
    else if (hp >= 2 && hp < 3) { r1 = 0; g1 = c; b1 = x; }
    else if (hp >= 3 && hp < 4) { r1 = 0; g1 = x; b1 = c; }
    else if (hp >= 4 && hp < 5) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    
    float m = v / 255.0 - c;
    *r = (uint8_t)((r1 + m) * 255);
    *g = (uint8_t)((g1 + m) * 255);
    *b = (uint8_t)((b1 + m) * 255);
}

void rainbow_effect(led_strip_handle_t led_strip) {
    static int hue_offset = 0;
    
    for (size_t i = 0; i < 9; i++) {
        // 计算每个LED的HSV颜色（色相在0-359之间，饱和度和亮度为最大值）
        int hue = (hue_offset + i * 45) % 360;  // 每个LED间隔45度，彩虹色
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, 20, &r, &g, &b);
        
        // 设置LED颜色
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
    
    // 更新LED显示
    led_strip_refresh(led_strip);
    
    // 增加偏移量以实现动态效果
    hue_offset = (hue_offset + 1) % 360;  // 每次增加5度，可以调整这个值改变速度
    
    // 添加适当延迟（例如50ms）
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

// 在主循环中调用

void app_main(void){
        configure_led();
    //oled_init();

    //imu216_main();
    //xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
    //xTaskCreate(DisplayThread, "DisplayThread", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    while (1) {
    rainbow_effect(led_strip);
    }
        for (size_t i = 0; i < 8; i++)
    {
        led_strip_set_pixel(led_strip, i, 1, 1, 1);
    }
     led_strip_refresh(led_strip);
}