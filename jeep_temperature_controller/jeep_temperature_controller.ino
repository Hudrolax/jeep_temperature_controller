#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// PINS
const int pin_buzzer = 2;        // пин пищалки
const int pin_switch_on = 3;        // пин переключателя (состояние ВКЛ)
const int pin_switch_off = 4;        // пин переключателя (состояние ВЫКЛ)
const int pin_engine_temp_sensor = 14;        // пин датчика температуры двигателя
const int pin_transmission_temp_sensor = 15;        // пин датчика температуры АКПП
const int pin_voltmeter_12v = 16;        // пин вольтметра (делитель 30/10 кОм)
const int pin_FAN = 9;        // пин ШИМ вентилятора


// VARS
const int target_engine_temp = 950;  // целевая температура двигателя
const int target_at_temp = 950;  // целевая температура АКПП
const int FAN_frequency = 10;       // частота ШИМ в гц
const int display_update_frquency = 2; // частота обновления дисплея в гц

int switch_mode = 0; // 0 - auto, 1 - fan_off, 2 - fan_always_on
int engine_temp = 0;
int trans_temp = 0;
int duty_cycle = 0;
float voltage = 0;

bool overheat = false;

unsigned long last_display_update_time = 0;
unsigned long last_buzzer_on_time = 0; 
unsigned long last_pwm_cycle = 0; 


void setup(void)
{ 
  Serial.begin(9600);
  u8g2.begin();

  tone(pin_buzzer, 1046, 50);
  delay(100);
  tone(pin_buzzer, 1175, 50);
  delay(100);
  tone(pin_buzzer, 1319, 50);

  pinMode(pin_switch_on, INPUT_PULLUP);
  pinMode(pin_switch_off, INPUT_PULLUP);
  pinMode(pin_FAN, OUTPUT);
}

void loop(void)
{ 
  unsigned long now = millis();
  if (now - last_display_update_time > 1000/display_update_frquency)
  {
    read_analog_inputs(); // чтение датчиков
    read_switch_state();  // читаем состояние переключателя
    check_overheat();
    FAN_control();        // отработка алгоритма расчета скважности (управление вентилятором)
    buzzer();             // пищим пищалкой, если нужно
    display_func();       // отрисовываем дисплей
    last_display_update_time = now;
  }
  pwm_cycle();          // отработка ШИМ 
}

void check_overheat()
{
    overheat = engine_temp > 1100 || trans_temp > 1200;
}

void FAN_control()
{
  if (switch_mode == 0)
  {
    duty_cycle = map(engine_temp, target_engine_temp-50, target_engine_temp+50, 10, 90);
    int at_duty_cycle = map(trans_temp, target_at_temp-50, target_at_temp+50, 10, 90);
    if (at_duty_cycle > duty_cycle) duty_cycle = at_duty_cycle;
    if (duty_cycle > 90) duty_cycle = 90;
    if (duty_cycle < 10) duty_cycle = 0;
  }else
    if (switch_mode == 1) duty_cycle = 0;
      else if (switch_mode == 2) duty_cycle = 90;
}

void pwm_cycle()
{
  unsigned long now = millis();
  int cycle_time_ms = 1000/FAN_frequency;
  if (now - last_pwm_cycle > cycle_time_ms){
    digitalWrite(pin_FAN, HIGH);
    last_pwm_cycle = now;
  }

  int on_time_ms = 1000/FAN_frequency*duty_cycle/100;
  if (now - last_pwm_cycle > on_time_ms){
    digitalWrite(pin_FAN, LOW);
  }
}

void display_func()
{ 
  u8g2.clearBuffer();
 
  // engine
  u8g2.setFont(u8g2_font_inb16_mf);
  u8g2.setCursor(0,16);
  u8g2.print("E ");
  u8g2.setCursor(20,16);
  u8g2.print(engine_temp/10, 1);
//    u8g2.setCursor(65,16);
//    u8g2.print("C");

  // trans
  u8g2.setFont(u8g2_font_inb16_mf);
  u8g2.setCursor(0,37);
  u8g2.print("T ");
  u8g2.setCursor(20,37);
  u8g2.print(trans_temp/10, 1);
//    u8g2.setCursor(65,37);
//    u8g2.print("C");

  // fan
  u8g2.setFont(u8g2_font_crox4h_tf );
  u8g2.setCursor(0,63);
  u8g2.print("FAN ");
  u8g2.setCursor(45,63);
  u8g2.print(map(duty_cycle, 0, 90, 0, 100));

  // switch mode
  u8g2.setFont(u8g2_font_helvR12_tf);
  u8g2.setCursor(95,13);
  if (switch_mode == 0)
  {
    u8g2.print("auto");
  }else if (switch_mode == 1)
    {
      u8g2.print("off");
    }else if (switch_mode == 2)
      {
        u8g2.print("on");
      }

  // voltage
  u8g2.setFont(u8g2_font_7x14B_mf);
  u8g2.setCursor(87,63);
  u8g2.print(voltage, 2);
  u8g2.setCursor(115,63);
  u8g2.print(" V");

  // overheat
  if (overheat)
  {
    u8g2.setFont(u8g2_font_helvR12_tf);
    u8g2.setCursor(95,37);
    u8g2.print("!!!!!");
  }
    
  u8g2.sendBuffer();
}

void read_analog_inputs()
{
  int engine_vol1 = analogRead(pin_engine_temp_sensor);
  int engine_vol2 = analogRead(pin_engine_temp_sensor);
  int engine_vol3 = analogRead(pin_engine_temp_sensor);
  int engine_vol = engine_vol1 + engine_vol2 + engine_vol3;
  engine_vol = engine_vol / 3;
  
  int trans_vol1 = analogRead(pin_transmission_temp_sensor);
  int trans_vol2 = analogRead(pin_transmission_temp_sensor);
  int trans_vol3 = analogRead(pin_transmission_temp_sensor);
  int trans_vol = trans_vol1 + trans_vol2 + trans_vol3;
  trans_vol = trans_vol / 3;
  
  int digit_voltage1 = analogRead(pin_voltmeter_12v);
  int digit_voltage2 = analogRead(pin_voltmeter_12v);
  int digit_voltage3 = analogRead(pin_voltmeter_12v);
  int digit_voltage = digit_voltage1 + digit_voltage2 + digit_voltage3;
  digit_voltage = digit_voltage / 3;

  engine_temp = map(engine_vol, 543, 180, 530, 1000);
  trans_temp = map(trans_vol, 527, 645, 210, 700);
  voltage = map(digit_voltage, 0, 658, 0, 1400);
  voltage = voltage/100;

  if (engine_temp < 0) engine_temp = 0;
  if (trans_temp < 0) trans_temp = 0;
  if (voltage < 0) voltage = 0;
}

void read_switch_state()
{
  int switch_on = digitalRead(pin_switch_on);
  int switch_off = digitalRead(pin_switch_off);
  if (!switch_on) {switch_mode = 1;}
    else if (!switch_off) {switch_mode = 2;}
      else {switch_mode = 0;}
}

void buzzer()
{
  unsigned long now = millis();
  if (overheat)
  {
    if (now - last_buzzer_on_time > 3000)
      {
        last_buzzer_on_time = now;
        tone(pin_buzzer, 220, 500); // перегрев
      }
  } else if (switch_mode > 0){
      if(switch_mode == 1){
        if (now - last_buzzer_on_time > 5000)
        {
          last_buzzer_on_time = now;
          tone(pin_buzzer, 440, 500); // принудительное отключение вентилятора
        }
      }else{
        if (now - last_buzzer_on_time > 20000)
        {
          last_buzzer_on_time = now;
          tone(pin_buzzer, 880, 250); // принудительное включение
        }
      }
    }
}
