#include <Wire.h>
#include "Adafruit_TCS34725.h"

#define Key_pin_1 pin_1
#define Key_pin_2 pin_2
#define LED_pin_1 pin_3
#define LED_pin_2 pin_4

#define Serial_Output_ShowScreen false

volatile bool flag = false;
volatile bool replay = false;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_4X);

typedef struct RGB
{
  float R;
  float G;
  float B;

} xStruct;

QueueHandle_t QHandle_1 = xQueueCreate(5, sizeof(RGB));

enum class LEDcolor
{
  Yellow,
  Red,
  Green,
  Blue,
  Purple,
  Cyan,
  White,
  Black,
  NOTDEF
};

// 存放顏色資料(RGB與顏色種類)的class
class comVal
{
private:
  float red, green, blue;
  LEDcolor color;

public:
  comVal(float R, float G, float B, LEDcolor COLOR) : red(R), green(G), blue(B), color(COLOR) {}
  float get_r() { return red; }
  float get_g() { return green; }
  float get_b() { return blue; }
  LEDcolor get_c() { return color; }
  void setValue(float r, float g, float b, LEDcolor c)
  {
    red = r;
    green = g;
    blue = b;
    color = c;
  }
  void Serial_Print_RGB();
  void RGB_compare(bool i);
  float *get_r_ptr() { return &red; }
  float *get_g_ptr() { return &green; }
  float *get_b_ptr() { return &blue; }
  String enumclassTOstring(void);
};

class GrayWorld
{
private:
  float R_gain_realtime = 0;
  float G_gain_realtime = 0;
  float B_gain_realtime = 0;
  int cnt = 0;
  float R_mean = 0;
  float G_mean = 0;
  float B_mean = 0;
  float add_num_r = 0;
  float add_num_g = 0;
  float add_num_b = 0;
  float K = 0;
  float R_change = 0;
  float G_change = 0;
  float B_change = 0;

public:
  bool RGB_gain(int data_num, xStruct *RGB_ptr, float *r_gain, float *g_gain, float *b_gain);
  void clear_arr(int arr_num, float *where_arr_ptr);

  float get_r_gain(void) { return R_gain_realtime; }
  float get_g_gain(void) { return G_gain_realtime; }
  float get_b_gain(void) { return B_gain_realtime; }
};

// class object declare
comVal raw_rgb(0, 0, 0, LEDcolor::NOTDEF);
comVal cal_rgb(0, 0, 0, LEDcolor::NOTDEF);
GrayWorld gray;

void taskone(void *parameter)
{
  QueueHandle_t QHandle;
  QHandle = (QueueHandle_t)parameter;
  BaseType_t xStatus;

  xStruct RGB_send = {0, 0, 0};

  xStruct RGB_gain_default = {1, 1, 1};
  xStruct RGB_gain = {1, 1, 1};
  xStruct RGB_cal = {0, 0, 0};

  bool comp_new = false;
  while (1)
  {
    TickType_t tStart = xTaskGetTickCount();

    tcs.getRGB(
        raw_rgb.get_r_ptr(),
        raw_rgb.get_g_ptr(),
        raw_rgb.get_b_ptr());

    RGB_send.R = raw_rgb.get_r();
    RGB_send.G = raw_rgb.get_g();
    RGB_send.B = raw_rgb.get_b();

    xStatus = xQueueSend(QHandle, &RGB_send, 0);

    if (uxQueueMessagesWaiting(QHandle_1) != 0)
    {
      xQueueReceive(QHandle_1, &RGB_gain, 0);
      comp_new = true;
    }

    if (replay)
    {
      RGB_gain.R = RGB_gain_default.R;
      RGB_gain.G = RGB_gain_default.G;
      RGB_gain.B = RGB_gain_default.B;
      replay = false;
      comp_new = false;
    }

    RGB_cal.R = raw_rgb.get_r() * RGB_gain.R;
    RGB_cal.G = raw_rgb.get_g() * RGB_gain.G;
    RGB_cal.B = raw_rgb.get_b() * RGB_gain.B;

    if (RGB_cal.R >= 255)
      RGB_cal.R = 255;
    if (RGB_cal.G >= 255)
      RGB_cal.G = 255;
    if (RGB_cal.B >= 255)
      RGB_cal.B = 255;

    cal_rgb.setValue(RGB_cal.R, RGB_cal.G, RGB_cal.B, raw_rgb.get_c());
    cal_rgb.RGB_compare(comp_new);
    cal_rgb.Serial_Print_RGB();

    Serial.println(comp_new);

    vTaskDelayUntil(&tStart, 60);
  }
}

void tasktwo(void *parameter)
{
  QueueHandle_t QHandle;
  QHandle = (QueueHandle_t)parameter;
  BaseType_t xStatus;
  xStruct RGB_send = {0, 0, 0};

  xStruct RGB_send_gain = {0, 0, 0};

  float r_gain;
  float g_gain;
  float b_gain;

  bool status = false;

  while (1)
  {
    if (uxQueueMessagesWaiting(QHandle) != 0)
    {
      xStatus = xQueueReceive(QHandle, &RGB_send, 0);
      if (xStatus != pdPASS)
      {
        // Serial.println("rec fail");
      }
      else
      {
        if (flag)
        {
          // Serial.println("flag is true");
          status = gray.RGB_gain(20, &RGB_send, &r_gain, &g_gain, &b_gain);
          if (status == false)
          {
            RGB_send_gain.R = r_gain;
            RGB_send_gain.G = g_gain;
            RGB_send_gain.B = b_gain;

            // Serial.print("RGB_send_gain is ");
            Serial.println(r_gain);

            xQueueSend(QHandle_1, &RGB_send_gain, 0);
          }
          flag = status;
          // gray.Change_RGB_gain(true);
        }
      }
    }

    // Serial.println("task 2 doing");
    vTaskDelay(60 / portTICK_PERIOD_MS);
  }
}

void taskKey(void *parameter)
{
  int key_cnt = 0;
  uint8_t pin_3 = 2;
  uint8_t pin_4 = 15;
  pinMode(LED_pin_1, OUTPUT);
  pinMode(LED_pin_2, OUTPUT);

  uint8_t pin_1 = 25;
  uint8_t pin_2 = 39;
  pinMode(Key_pin_1, INPUT);
  pinMode(Key_pin_2, INPUT);

  int key_value_1 = 0;
  int key_value_2 = 0;

  // Serial.println("task key start");

  while (1)
  {
    if (digitalRead(Key_pin_1) == 1)
    {
      key_cnt++;
      // Serial.print("CCC: ");
      // Serial.println(key_cnt);
      if (key_cnt >= 10)
      {
        flag = true;
        digitalWrite(LED_pin_1, HIGH);
        // Serial.print("AAA: ");
        // Serial.println(key_cnt);
        key_cnt = 0;
      }
    }
    else if (key_cnt >= 1)
    {
      key_cnt = 0;
      // Serial.print("BBB: ");
      digitalWrite(LED_pin_1, LOW);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void taskKey_1(void *parameter)
{
  int key_cnt = 0;
  uint8_t pin_3 = 2;
  uint8_t pin_4 = 15;
  pinMode(LED_pin_1, OUTPUT);
  pinMode(LED_pin_2, OUTPUT);

  uint8_t pin_1 = 25;
  uint8_t pin_2 = 39;
  pinMode(Key_pin_1, INPUT);
  pinMode(Key_pin_2, INPUT);

  int key_value_1 = 0;
  int key_value_2 = 0;
  // Serial.println("task key start");
  while (1)
  {
    // Serial.print("OOO: ");
    // Serial.println(digitalRead(Key_pin_1));
    ////Serial.println("task key is running");
    if (digitalRead(Key_pin_2) == 1)
    {
      key_cnt++;
      // Serial.print("CCC: ");
      // Serial.println(key_cnt);
      if (key_cnt >= 10)
      {
        replay = true;
        digitalWrite(LED_pin_2, HIGH);
        // Serial.print("BBB: ");
        // Serial.println(key_cnt);
        key_cnt = 0;
      }
    }
    else if (key_cnt >= 1)
    {
      key_cnt = 0;
      // Serial.print("BBB: ");
      digitalWrite(LED_pin_2, LOW);
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL for ESP32

  if (tcs.begin())
  {
    Serial.println("TCS34725 初始化成功");
  }
  else
  {
    Serial.println("找不到 TCS34725，請檢查接線");
    while (1)
      ;
  }

  QueueHandle_t QHandle;
  QHandle = xQueueCreate(5, sizeof(RGB));

  TaskHandle_t pxtask1;
  TaskHandle_t pxtask2;

  if (QHandle != NULL)
  {
    Serial.println("Creat quene sucessful");
    xTaskCreatePinnedToCore(taskone, "TaskOne", 10000, (void *)QHandle, 1, &pxtask1, 0);
    xTaskCreatePinnedToCore(tasktwo, "Tasktwo", 10000, (void *)QHandle, 1, &pxtask2, 1);
  }
  else
  {
    Serial.println("Creat quene not sucessful");
  }

  xTaskCreatePinnedToCore(taskKey, "TaskKey", 1000, NULL, 0, NULL, 1);
  xTaskCreatePinnedToCore(taskKey_1, "TaskKey_1", 1000, NULL, 0, NULL, 1);
  // vTaskStartScheduler();
}

void loop()
{
  delay(1000);
}

bool GrayWorld::RGB_gain(int data_num, xStruct *RGB_ptr, float *r_gain, float *g_gain, float *b_gain)
{
  if (cnt < data_num)
  {
    Serial.print("cnt: ");
    Serial.println(cnt);
    add_num_r += RGB_ptr->R;
    add_num_g += RGB_ptr->G;
    add_num_b += RGB_ptr->B;

    Serial.print("r is ");
    Serial.println(RGB_ptr->R);
    Serial.print("add num is ");
    Serial.println(add_num_r);
    cnt++;

    return true;
  }
  else if (cnt == data_num)
  {
    cnt = 0;

    Serial.print("false");
    Serial.println(cnt);

    Serial.print("SUM ADD num is ");
    Serial.println(add_num_r);

    R_mean = add_num_r / data_num;
    G_mean = add_num_g / data_num;
    B_mean = add_num_b / data_num;

    add_num_r = 0.0;
    add_num_g = 0.0;
    add_num_b = 0.0;
    K = 255;

    R_gain_realtime = K / R_mean;
    G_gain_realtime = K / G_mean;
    B_gain_realtime = K / B_mean;

    *r_gain = K / R_mean;
    *g_gain = K / G_mean;
    *b_gain = K / B_mean;

    return false;
  }
}

void GrayWorld::clear_arr(int arr_num, float *where_arr_ptr)
{
  for (size_t i = 0; i < arr_num; i++)
  {
    *(where_arr_ptr + i) = 0;
  }
}

// true 表示可以用以python顯示
void comVal::Serial_Print_RGB()
{

#if Serial_Output_ShowScreen
  Serial.print("R:");
  Serial.print(red);
  Serial.print(" ");
  Serial.print("G:");
  Serial.print(green);
  Serial.print(" ");
  Serial.print("B:");
  Serial.print(blue);
  Serial.println(" ");
#else
  Serial.println("color is: ");
  Serial.println(enumclassTOstring());
  Serial.println("rgb value is: ");
  Serial.print("R:");
  Serial.print(red);
  Serial.print(" ");
  Serial.print("G:");
  Serial.print(green);
  Serial.print(" ");
  Serial.print("B:");
  Serial.print(blue);
  Serial.println(" ");
  Serial.println("------------------");

#endif
}

String comVal::enumclassTOstring(void)
{
  switch (color)
  {
  case LEDcolor::Yellow:
    return "Yellow";
  case LEDcolor::Red:
    return "Red  ";
  case LEDcolor::Green:
    return "Green";
  case LEDcolor::Blue:
    return "Blue ";
  case LEDcolor::Purple:
    return "Purple";
  case LEDcolor::Cyan:
    return "Cyan ";
  case LEDcolor::White:
    return "White";
  case LEDcolor::Black:
    return "Black";
  case LEDcolor::NOTDEF:
    return "NOTDEF";
  }
}

void comVal::RGB_compare(bool i)
{
  // 存入初始判斷顏色值
  comVal Yel(85, 100, 70, LEDcolor::Yellow);
  comVal Red(120, 70, 70, LEDcolor::Red);
  comVal Green(90, 130, 90, LEDcolor::Green);
  comVal Blue(100, 100, 125, LEDcolor::Blue);
  comVal Purple(80, 65, 110, LEDcolor::Purple);
  comVal Cyan(55, 100, 100, LEDcolor::Cyan);
  comVal White(90, 90, 90, LEDcolor::White);
  comVal Black(70, 70, 70, LEDcolor::Black);

  if (i)
  {
    Yel.setValue(255, 255, 200, LEDcolor::Yellow);
    Red.setValue(255, 200, 200, LEDcolor::Red);
    Green.setValue(210, 255, 210, LEDcolor::Green);
    Blue.setValue(200, 200, 255, LEDcolor::Blue);
    Purple.setValue(255, 200, 255, LEDcolor::Purple);
    Cyan.setValue(200, 255, 255, LEDcolor::Cyan);
    White.setValue(240, 240, 240, LEDcolor::White);
    Black.setValue(50, 50, 50, LEDcolor::Black);
  }

  if ((red >= Yel.get_r()) && (green >= Yel.get_g()) && (blue <= Yel.get_b()))
    color = Yel.get_c(); // YELLOW
  else if ((red >= Red.get_r()) && (green <= Red.get_g()) && (blue <= Red.get_b()))
    color = Red.get_c(); // RED
  else if ((red <= Green.get_r()) && (green >= Green.get_g()) && (blue <= Green.get_b()))
    color = Green.get_c(); // GREEN
  else if ((red <= Blue.get_r()) && (green <= Blue.get_g()) && (blue >= Blue.get_b()))
    color = Blue.get_c(); // Blue
  else if ((red >= Purple.get_r()) && (green <= Purple.get_g()) && (blue >= Purple.get_b()))
    color = Purple.get_c(); // PURPLE
  else if ((red <= Cyan.get_r()) && (green >= Cyan.get_g()) && (blue >= Cyan.get_b()))
    color = Cyan.get_c(); // CYAN
  else if ((red >= White.get_r()) && (green >= White.get_g()) && (blue >= White.get_b()))
    color = White.get_c(); // WHITE
  else if ((red <= Black.get_r()) && (green <= Black.get_g()) && (blue <= Black.get_b()))
    color = Black.get_c(); // BLACK
  else
    color = LEDcolor::NOTDEF;
}
