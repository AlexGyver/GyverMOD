/*
  Created 2017  by AlexGyver  AlexGyver Home Labs Inc.

  ВНИМАНИЕ! ПУТЬ К ПАПКЕ СО СКЕТЧЕМ НЕ ДОЛЖЕН СОДЕРЖАТЬ РУССКИХ СИМВОЛОВ
  ВО ИЗБЕЖАНИЕ ПРОБЛЕМ ПОЛОЖИТЕ ПАПКУ В КОРЕНЬ ДИСКА С
  
  Внимание! При первом запуске initial_calibration должен быть равен 1 (строка №16)
  При подключении и открытии монитора порта будет запущен процесс калибровки.
  Вам нужно при помощи вольтметра измерить напряжение на пинах 5V и GND,
  затем отправить его в монитор В МИЛЛИВОЛЬТАХ, т.е. если на вольтметре 4.56
  то отправить примерно 4560. После этого изменить initial_calibration на 0
  и заново прошить Arduino.
  Если хотите пропустить процесс калибровки, то введите то же самое напряжение,
  что было показано вам при калибровке (real VCC). И снова прошейте код.
*/
#define initial_calibration 1 // калибровка вольтметра 1 - включить, 0 - выключить
#define welcome 1 // приветствие (слова GYVER VAPE при включении), 1 - включить, 0 - выключить
#define battery_info 1 // отображение напряжения аккумулятора при запуске, 1 - включить, 0 - выключить

#include <EEPROMex.h>   // библиотека для работы со внутренней памятью ардуино
//-----------кнопки-----------
#define butt_up 5      // кнопка вверх
#define butt_down 4    // кнпока вниз
#define butt_set 3     // кнопка выбора
#define butt_vape 2    // кнопка "парить"
//-----------кнопки-----------

//-----------флажки-----------
boolean up_state, down_state, set_state, vape_state;
boolean up_flag, down_flag, set_flag, vape_flag, set_flag_hold, set_hold;
boolean change_v_flag;
boolean vape_mode, vape_mode_flag, vape_mode_flag2, vape_mode_flag3;
byte mode, mode_flag = 1;
boolean flag;          // флаг, разрешающий подать ток на койл (защита от КЗ, обрыва, разрядки)
//-----------флажки-----------

//-----------пины-----------
#define mosfet 10      // пин мосфета (нагрев спирали)
#define battery 5      // пин измерения напряжения акума
//-----------пины-----------

//-----------дисплей-----------
#include <TimerOne.h>
#include <TM74HC595Display.h>
#define SCLK 6
#define RCLK 7
#define DIO 8
TM74HC595Display disp(SCLK, RCLK, DIO);
unsigned char SYM[47];
//--------дисплей-------

int bat_vol, bat_volt_f;   // хранит напряжение на акуме
int PWM, PWM_f;           // хранит PWM сигнал

//----переменные и коэффициенты для фильтра----
int bat_old, PWM_old = 800;
float filter_k = 0.04;
float PWM_filter_k = 0.1;
//----переменные и коэффициенты для фильтра----

unsigned long last_time, vape_press, set_press, last_vape; // таймеры
int volts;    // храним вольты
float my_vcc_const;  // константа вольтметра

//-----------надписи----------
byte VVOL[4] = {41, 36, 32, 30};

byte GYVE[4] = {26, 37, 36, 24};
byte YVEA[4] = {37, 36, 24, 20};
byte VAPE[4] = {36, 20, 33, 24};
byte BVOL[4] = {44, 36, 32, 30};
byte vape1[4] = {46, 45, 46, 45};
byte vape2[4] = {45, 46, 45, 46};
byte LOWB[4] = {45, 45, 45, 0};
byte CHG1[4] = {22, 27, 26, 39};
byte CHG2[4] = {22, 27, 26, 45};
byte CHG3[4] = {22, 27, 26, 38};
//-----------надписи----------

void setup() {
  Serial.begin(9600);
  if (initial_calibration) calibration();  // калибровка, если разрешена

  //----читаем из памяти-----
  volts = EEPROM.readInt(0);
  my_vcc_const = EEPROM.readFloat(8);
  //----читаем из памяти-----

  symbols(); // инициализировать символы для дисплея
  Timer1.initialize(1500);          // таймер дисплея
  Timer1.attachInterrupt(timerIsr);

  //---настройка кнопок и выходов-----
  pinMode(butt_up , INPUT_PULLUP);
  pinMode(butt_down , INPUT_PULLUP);
  pinMode(butt_set , INPUT_PULLUP);
  pinMode(butt_vape , INPUT_PULLUP);
  pinMode(mosfet , OUTPUT);
  //---настройка кнопок и выходов-----

  //------приветствие-----
  if (welcome) {
    disp_send(GYVE);
    delay(400);
    disp_send(YVEA);
    delay(400);
    disp_send(VAPE);
    delay(400);
  }
  //------приветствие-----

  // измерить напряжение аккумулятора
  bat_vol = analogRead(battery) * readVcc() / 1023;
  bat_old = bat_vol;

  // проверка заряда акума, если разряжен то прекратить работу
  if (bat_vol < 2800) {
    flag = 0;
    disp_send(LOWB);
    while (1); // перейти в бесконечный цикл (поможет только перезагрузка)
  } else {
    flag = 1;
  }

  if (battery_info) {
    disp_send(BVOL);
    delay(500);
    disp.float_dot((float)bat_vol / 1000, 2);
    delay(1000);
    disp.clear();
  }
}

void loop() {
  if (millis() - last_time > 50) {                       // 20 раз в секунду измеряем напряжение
    last_time = millis();
    bat_vol = analogRead(battery) * readVcc() / 1023;    // измерить напряжение аккумулятора в миллиВольтах
    bat_volt_f = filter_k * bat_vol + (1 - filter_k) * bat_old;  // фильтруем
    bat_old = bat_volt_f;                                // фильтруем
    if (bat_volt_f < 2800) {                             // если напряжение меньше минимального
      flag = 0;                                          // прекратить работу
      disp_send(LOWB);
      while (1); // перейти в бесконечный цикл (поможет только перезагрузка)
    }
  }

  //-------опрос кнопок-------
  up_state = !digitalRead(butt_up);
  down_state = !digitalRead(butt_down);
  set_state = !digitalRead(butt_set);
  vape_state = !digitalRead(butt_vape);
  //-------опрос кнопок-------

  // service_mode();  // раскомментировать для отладки кнопок

  //---отработка нажатия SET и изменение режимов---
  if (set_state && !set_hold) {          // если кнпока нажата
    set_hold = 1;
    set_press = millis();                // начинаем отсчёт
    while (millis() - set_press < 300) {
      if (digitalRead(butt_set)) {       // если кнопка отпущена до 300 мс
        set_hold = 0;
        set_flag = 1;
        break;
      }
    }
  }
  if (set_hold && set_state) {           // если кнопка всё ещё удерживается
    if (!set_flag_hold) {
      disp.clear();
      set_flag_hold = 1;
    }
    if (round(millis() / 150) % 2 == 0) disp.float_dot((float)bat_volt_f / 1000, 2);  // показать заряд акума
  }
  if (set_hold && !set_state && set_flag_hold) {  // если удерживалась и была отпущена
    set_hold = 0;
    set_flag_hold = 0;
    mode_flag = 1;
  }

  if (!set_state && set_flag) {  // если нажали-отпустили
    set_hold = 0;
    set_flag = 0;
    mode = 0;
    mode_flag = 1;
  }
  // ---отработка нажатия SET и изменение режимов---

  // -------------режим ВАРИВОЛЬТ--------------
  if (mode == 0 && !vape_state && !set_hold) {
    if (mode_flag) {                     // приветствие
      mode_flag = 0;
      disp_send(VVOL);
      delay(400);
      disp.clear();
    }
    //---------кнопка ВВЕРХ--------
    if (up_state && !up_flag) {
      volts += 100;
      volts = min(volts, bat_volt_f);
      up_flag = 1;
      disp.clear();
    }
    if (!up_state && up_flag) {
      up_flag = 0;
      change_v_flag = 1;
    }
    //---------кнопка ВВЕРХ--------

    //---------кнопка ВНИЗ--------
    if (down_state && !down_flag) {
      volts -= 100;
      volts = max(volts, 0);
      down_flag = 1;
      disp.clear();
    }
    if (!down_state && down_flag) {
      down_flag = 0;
      change_v_flag = 1;
    }
    //---------кнопка ВНИЗ--------
    disp.float_dot((float)volts / 1000, 2); // отобразить на дисплее
  }
  // -------------режим ВАРИВОЛЬТ--------------

  //----отработка нажатия кнопки парения------
  if (vape_state && flag) {

    if (!vape_mode_flag && !vape_flag) {
      vape_mode_flag = 1;     // первичное нажатие
      vape_press = millis();
    }

    // ------------отработка двойного нажатия------------
    if (millis() - vape_press > 200 && vape_mode_flag) {
      vape_mode_flag = 0;
    }
    if (vape_mode_flag3 && vape_mode_flag) {
      vape_mode_flag2 = 1;
    }
    if (vape_mode_flag && vape_mode_flag2) {
      vape_mode = 1;
    }
    // ------------отработка двойного нажатия------------

    if (!vape_mode) {
      if (round(millis() / 150) % 2 == 0) disp_send(vape1); else disp_send(vape2); // мигать медленно
      if (mode == 0) {                                              // если ВАРИВОЛЬТ
        PWM = (float)volts / bat_volt_f * 1024;                     // считаем значение для ШИМ сигнала
        if (PWM > 1023) PWM = 1023;                                 // ограничил PWM "по тупому", потому что constrain сука не работает!
        PWM_f = PWM_filter_k * PWM + (1 - PWM_filter_k) * PWM_old;  // фильтруем
        PWM_old = PWM_f;                                            // фильтруем
      }
      Timer1.pwm(mosfet, PWM_f);  // управление мосфетом
    } else {
      vape_mode_flag = 0;
      vape_mode_flag2 = 0;
      vape_mode_flag3 = 0;
      if (round(millis() / 50) % 2 == 0) disp_send(vape1); else disp_send(vape2); // мигать быстро
      digitalWrite(mosfet, 1);   // херачить на полную мощность
    }
    vape_flag = 1;
  }
  if (vape_state && !flag) { // если акум сел, а мы хотим подымить
    disp.clear();
    disp_send(LOWB);
    delay(1000);
    vape_flag = 1;
  }
  if (!vape_state && vape_flag) {  // если кнопка ПАРИТЬ отпущена
    digitalWrite(mosfet, 0);
    disp.clear();
    if (vape_mode_flag) vape_mode_flag3 = 1;
    vape_flag = 0;
    mode_flag = 0;
    vape_mode = 0;

    // если есть изменения в настройках, записать в память
    if (change_v_flag) {
      EEPROM.writeInt(0, volts);
      change_v_flag = 0;
    }
    // если есть изменения в настройках, записать в память
  }
  //----отработка нажатия кнопки парения------

} // конец loop

void service_mode() {
  if (set_state && !set_flag) {
    set_flag = 1;
    Serial.println("SET pressed");
  }
  if (!set_state && set_flag) {
    set_flag = 0;
    Serial.println("SET released");
  }
  if (up_state && !up_flag) {
    up_flag = 1;
    Serial.println("UP pressed");
  }
  if (!up_state && up_flag) {
    up_flag = 0;
    Serial.println("UP released");
  }
  if (down_state && !down_flag) {
    down_flag = 1;
    Serial.println("DOWN pressed");
  }
  if (!down_state && down_flag) {
    down_flag = 0;
    Serial.println("DOWN released");
  }
  if (vape_state && !vape_flag) {
    vape_flag = 1;
    Serial.println("VAPE pressed");
  }
  if (!vape_state && vape_flag) {
    vape_flag = 0;
    Serial.println("VAPE released");
  }
}


void disp_send(byte sym[]) {
  for (int i = 0; i < 4; i++) {
    disp.set(SYM[sym[i]], 3 - i);
  }
}

void timerIsr()  //нужно для дисплея
{
  disp.timerIsr();
}

void symbols() {

  SYM[0] = 0xC0; //0
  SYM[1] = 0xF9; //1
  SYM[2] = 0xA4; //2
  SYM[3] = 0xB0; //3
  SYM[4] = 0x99; //4
  SYM[5] = 0x92; //5
  SYM[6] = 0x82; //6
  SYM[7] = 0xF8; //7
  SYM[8] = 0x80; //8
  SYM[9] = 0x90; //9

  SYM[10] = 0b01000000; //.0
  SYM[11] = 0b01111001; //.1
  SYM[12] = 0b00100100; //.2
  SYM[13] = 0b00110000; //.3
  SYM[14] = 0b00011001; //.4
  SYM[15] = 0b00010010; //.5
  SYM[16] = 0b00000010; //.6
  SYM[17] = 0b01111000; //.7
  SYM[18] = 0b00000000; //.8
  SYM[19] = 0b00010000; //.9

  SYM[20] = 0x88; //A
  SYM[21] = 0x83; //b
  SYM[22] = 0xC6; //C
  SYM[23] = 0xA1; //d
  SYM[24] = 0x86; //E
  SYM[25] = 0x8E; //F
  SYM[26] = 0xC2; //G
  SYM[27] = 0x89; //H
  SYM[28] = 0xF9; //I
  SYM[29] = 0xF1; //J
  SYM[30] = 0xC3; //L
  SYM[31] = 0xA9; //n
  SYM[32] = 0xC0; //O
  SYM[33] = 0x8C; //P
  SYM[34] = 0x98; //q
  SYM[35] = 0x92; //S
  SYM[36] = 0xC1; //U
  SYM[37] = 0x91; //Y
  SYM[38] = 0xFE; //hight -
  SYM[39] = 0b11110111; //low -
  SYM[40] = 0b01000001; //U.
  SYM[41] = 0b01100011; //u.
  SYM[42] = 0b11111111; //
  SYM[43] = 0b11100011; //u
  SYM[44] = 0b00000011; //b.
  SYM[45] = 0b10111111; //mid -
  SYM[46] = 0b11110110; //mid&high -
}

void calibration() {
  //--------калибровка----------
  for (byte i = 0; i < 7; i++) EEPROM.writeInt(i, 0);          // чистим EEPROM для своих нужд
  my_vcc_const = 1.1;
  Serial.print("Real VCC is: "); Serial.println(readVcc());     // общаемся с пользователем
  Serial.println("Write your VCC (in millivolts)");
  while (Serial.available() == 0); int Vcc = Serial.parseInt(); // напряжение от пользователя
  float real_const = (float)1.1 * Vcc / readVcc();              // расчёт константы
  Serial.print("New voltage constant: "); Serial.println(real_const, 3);
  EEPROM.writeFloat(8, real_const);                             // запись в EEPROM
  while (1); // уйти в бесконечный цикл
  //------конец калибровки-------
}

long readVcc() { //функция чтения внутреннего опорного напряжения, универсальная (для всех ардуин)
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both
  long result = (high << 8) | low;

  result = my_vcc_const * 1023 * 1000 / result; // расчёт реального VCC
  return result; // возвращает VCC
}
