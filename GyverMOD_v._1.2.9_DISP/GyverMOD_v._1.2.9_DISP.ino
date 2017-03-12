/*
  Created 2017  by AlexGyver  AlexGyver Home Labs Inc.

  ВНИМАНИЕ! ПУТЬ К ПАПКЕ СО СКЕТЧЕМ НЕ ДОЛЖЕН СОДЕРЖАТЬ РУССКИХ СИМВОЛОВ
  ВО ИЗБЕЖАНИЕ ПРОБЛЕМ ПОЛОЖИТЕ ПАПКУ В КОРЕНЬ ДИСКА С

  Внимание! При первом запуске initial_calibration должен быть равен 1 (строка №17)
  При подключении и открытии монитора порта будет запущен процесс калибровки.
  Вам нужно при помощи вольтметра измерить напряжение на пинах 5V и GND,
  затем отправить его в монитор В МИЛЛИВОЛЬТАХ, т.е. если на вольтметре 4.56
  то отправить примерно 4560. После этого изменить initial_calibration на 0
  и заново прошить Arduino.
  Если хотите пропустить процесс калибровки, то введите то же самое напряжение,
  что было показано вам при калибровке (real VCC). И снова прошейте код.
*/
//-----------------------------------НАСТРОЙКИ------------------------------------
#define initial_calibration 0  // калибровка вольтметра 1 - включить, 0 - выключить
#define welcome 0              // приветствие (слова GYVER VAPE при включении), 1 - включить, 0 - выключить
#define battery_info 0         // отображение напряжения аккумулятора при запуске, 1 - включить, 0 - выключить
#define sleep_timer 10         // таймер сна в секундах
#define vape_threshold 4       // отсечка затяжки, в секундах
#define turbo_mode 0           // турбо режим 1 - включить, 0 - выключить
#define battery_percent 0      // отображать заряд в процентах, 1 - включить, 0 - выключить
#define battery_low 2.8        // нижний порог срабатывания защиты от переразрядки аккумулятора, в Вольтах!
//-----------------------------------НАСТРОЙКИ------------------------------------

#include <EEPROMex.h>   // библиотека для работы со внутренней памятью ардуино
#include <LowPower.h>   // библиотека сна

//-----------кнопки-----------
#define butt_up 5      // кнопка вверх
#define butt_down 4    // кнпока вниз
#define butt_set 3     // кнопка выбора
#define butt_vape 2    // кнопка "парить"
//-----------кнопки-----------

//-----------флажки-----------
boolean up_state, down_state, set_state, vape_state;
boolean up_flag, down_flag, set_flag, set_flag_hold, set_hold, vape_btt, vape_btt_f;
volatile boolean wake_up_flag, vape_flag;
boolean change_v_flag, change_w_flag, change_o_flag;
volatile byte mode, mode_flag = 1;
boolean flag;          // флаг, разрешающий подать ток на койл (защита от КЗ, обрыва, разрядки)
//-----------флажки-----------

//-----------пины-------------
#define mosfet 10      // пин мосфета (нагрев спирали)
#define battery 5      // пин измерения напряжения акума
//-----------пины-------------

//-----------дисплей-----------
#include <TimerOne.h>
#include <TM74HC595Display.h>
#define disp_vcc 13
#define SCLK 6
#define RCLK 7
#define DIO 8
TM74HC595Display disp(SCLK, RCLK, DIO);
unsigned char SYM[47];
//-----------дисплей-----------

int bat_vol, bat_volt_f;   // хранит напряжение на акуме
int PWM, PWM_f;           // хранит PWM сигнал

//-------переменные и коэффициенты для фильтра-------
int bat_old, PWM_old = 800;
float filter_k = 0.04;
float PWM_filter_k = 0.1;
//-------переменные и коэффициенты для фильтра-------

unsigned long last_time, vape_press, set_press, last_vape, wake_timer; // таймеры
int volts, watts;    // храним вольты и ватты
float ohms;          // храним омы
float my_vcc_const;  // константа вольтметра
volatile byte vape_mode, vape_release_count;

//---------------надписи---------------
byte VVOL[4] = {41, 36, 32, 30};
byte VAVA[4] = {41, 36, 20, 42};
byte COIL[4] = {22, 32, 28, 30};

byte GYVE[4] = {26, 37, 36, 24};
byte YVEA[4] = {37, 36, 24, 20};
byte VAPE[4] = {36, 20, 33, 24};
byte BVOL[4] = {44, 36, 32, 30};
byte vape1[4] = {46, 45, 46, 45};
byte vape2[4] = {45, 46, 45, 46};
byte LOWB[4] = {8, 45, 45, 0};
byte BYE[4] = {21, 37, 24, 42};
byte BLANK[4] = {42, 42, 42, 42};
byte V[4] = {36, 42, 42, 42};
byte A[4] = {36, 20, 42, 42};
byte P[4] = {36, 20, 33, 42};
byte E[4] = {36, 20, 33, 24};
//----------------надписи---------------

void setup() {
  Serial.begin(9600);
  if (initial_calibration) calibration();  // калибровка, если разрешена

  //----читаем из памяти-----
  volts = EEPROM.readInt(0);
  watts = EEPROM.readInt(2);
  ohms = EEPROM.readFloat(4);
  my_vcc_const = EEPROM.readFloat(8);
  //----читаем из памяти-----

  symbols(); // инициализировать символы для дисплея
  Timer1.initialize(1500);          // таймер
  Timer1.attachInterrupt(timerIsr);

  //---настройка кнопок и выходов-----
  pinMode(butt_up , INPUT_PULLUP);
  pinMode(butt_down , INPUT_PULLUP);
  pinMode(butt_set , INPUT_PULLUP);
  pinMode(butt_vape , INPUT_PULLUP);
  pinMode(mosfet , OUTPUT);
  pinMode(disp_vcc , OUTPUT);
  digitalWrite(disp_vcc, HIGH);
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
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
  bat_vol = readVcc();
  bat_old = bat_vol;

  // проверка заряда акума, если разряжен то прекратить работу
  if (bat_vol < battery_low * 1000) {
    flag = 0;
    disp.clear();
    disp_send(LOWB);
    Timer1.disablePwm(mosfet);    // принудительно отключить койл
    digitalWrite(mosfet, LOW);    // принудительно отключить койл
  } else {
    flag = 1;
  }

  if (battery_info) {  // отобразить заряд аккумулятора при первом включении
    disp.clear();
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
    bat_vol = readVcc();                                 // измерить напряжение аккумулятора в миллиВольтах
    bat_volt_f = filter_k * bat_vol + (1 - filter_k) * bat_old;  // фильтруем
    bat_old = bat_volt_f;                                // фильтруем
    if (bat_volt_f < battery_low * 1000) {               // если напряжение меньше минимального
      flag = 0;                                          // прекратить работу
      disp.clear();
      disp_send(LOWB);
      Timer1.disablePwm(mosfet);    // принудительно отключить койл
      digitalWrite(mosfet, LOW);    // принудительно отключить койл
    }
  }

  //-----------опрос кнопок-----------
  up_state = !digitalRead(butt_up);
  down_state = !digitalRead(butt_down);
  set_state = !digitalRead(butt_set);
  vape_state = !digitalRead(butt_vape);

  // если нажата любая кнопка, "продлить" таймер ухода в сон
  if (up_state || down_state || set_state || vape_state) wake_timer = millis();
  //-----------опрос кнопок-----------

  // service_mode();  // раскомментировать для отладки кнопок
  // показывает, какие кнопки нажаты или отпущены
  // использовать для проерки правильности подключения

  //---------------------отработка нажатия SET и изменение режимов---------------------
  if (flag) {                              // если акум заряжен
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
      if (round(millis() / 150) % 2 == 0) {
        if (!battery_percent) {
          disp.float_dot((float)bat_volt_f / 1000, 2); // показать заряд акума в вольтах
        } else {
          disp.digit4(map(bat_volt_f, battery_low * 1000, 4200, 0, 99)); // показать заряд акума в процентах
        }
      }
    }
    if (set_hold && !set_state && set_flag_hold) {  // если удерживалась и была отпущена
      set_hold = 0;
      set_flag_hold = 0;
      mode_flag = 1;
    }

    if (!set_state && set_flag) {  // если нажали-отпустили
      set_hold = 0;
      set_flag = 0;
      mode++;                      // сменить режим
      mode_flag = 1;
      if (mode > 2) mode = 0;      // ограничение на 3 режима
    }
    // ----------------------отработка нажатия SET и изменение режимов---------------------------

    // ------------------режим ВАРИВОЛЬТ-------------------
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
        volts = min(volts, bat_volt_f);  // ограничение сверху на текущий заряд акума
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
    // ------------------режим ВАРИВОЛЬТ-------------------


    // ------------------режим ВАРИВАТТ-------------------
    if (mode == 1 && !vape_state && !set_hold) {
      if (mode_flag) {                     // приветствие
        mode_flag = 0;
        disp_send(VAVA);
        delay(400);
        disp.clear();
      }
      //---------кнопка ВВЕРХ--------
      if (up_state && !up_flag) {
        watts += 1;
        byte maxW = (sq((float)bat_volt_f / 1000)) / ohms;
        watts = min(watts, maxW);               // ограничение сверху на текущий заряд акума и сопротивление
        up_flag = 1;
        disp.clear();
      }
      if (!up_state && up_flag) {
        up_flag = 0;
        change_w_flag = 1;
      }
      //---------кнопка ВВЕРХ--------

      //---------кнопка ВНИЗ--------
      if (down_state && !down_flag) {
        watts -= 1;
        watts = max(watts, 0);
        down_flag = 1;
        disp.clear();
      }
      if (!down_state && down_flag) {
        down_flag = 0;
        change_w_flag = 1;
      }
      //---------кнопка ВНИЗ--------
      disp.digit4(watts);        // отобразить на дисплее
    }
    // ------------------режим ВАРИВАТТ--------------

    // ----------режим установки сопротивления-----------
    if (mode == 2 && !vape_state && !set_hold) {
      if (mode_flag) {                     // приветствие
        mode_flag = 0;
        disp_send(COIL);
        delay(400);
        disp.clear();
      }
      //---------кнопка ВВЕРХ--------
      if (up_state && !up_flag) {
        ohms += 0.05;
        ohms = min(ohms, 3);
        up_flag = 1;
        disp.clear();
      }
      if (!up_state && up_flag) {
        up_flag = 0;
        change_o_flag = 1;
      }
      //---------кнопка ВВЕРХ--------

      //---------кнопка ВНИЗ--------
      if (down_state && !down_flag) {
        ohms -= 0.05;
        ohms = max(ohms, 0);
        down_flag = 1;
        disp.clear();
      }
      if (!down_state && down_flag) {
        down_flag = 0;
        change_o_flag = 1;
      }
      //---------кнопка ВНИЗ--------
      disp.float_dot(ohms, 2);        // отобразить на дисплее
    }
    // ----------режим установки сопротивления-----------

    //---------отработка нажатия кнопки парения-----------
    if (vape_state && flag && !wake_up_flag) {

      if (!vape_flag) {
        vape_flag = 1;
        vape_mode = 1;            // первичное нажатие
        delay(20);                // анти дребезг (сделал по-тупому, лень)
        vape_press = millis();    // первичное нажатие
      }

      if (vape_release_count == 1) {
        vape_mode = 2;               // двойное нажатие
        delay(20);                   // анти дребезг (сделал по-тупому, лень)
      }
      if (vape_release_count == 2) {
        vape_mode = 3;               // тройное нажатие
      }

      if (millis() - vape_press > vape_threshold * 1000) {  // "таймер затяжки"
        vape_mode = 0;
        digitalWrite(mosfet, 0);
      }

      if (vape_mode == 1) {                                           // обычный режим парения
        if (round(millis() / 150) % 2 == 0)
          disp_send(vape1); else disp_send(vape2);                    // мигать медленно
        if (mode == 0) {                                              // если ВАРИВОЛЬТ
          PWM = (float)volts / bat_volt_f * 1024;                     // считаем значение для ШИМ сигнала
          if (PWM > 1023) PWM = 1023;                                 // ограничил PWM "по тупому", потому что constrain сука не работает!
          PWM_f = PWM_filter_k * PWM + (1 - PWM_filter_k) * PWM_old;  // фильтруем
          PWM_old = PWM_f;                                            // фильтруем
        }
        Timer1.pwm(mosfet, PWM_f);                                    // управление мосфетом
      }
      if (vape_mode == 2 && turbo_mode) {                             // турбо режим парения (если включен)
        if (round(millis() / 50) % 2 == 0)
          disp_send(vape1); else disp_send(vape2);                    // мигать быстро
        digitalWrite(mosfet, 1);                                      // херачить на полную мощность
      }
      if (vape_mode == 3) {                                           // тройное нажатие
        vape_release_count = 0;
        vape_mode = 1;
        vape_flag = 0;
        good_night();    // вызвать функцию сна
      }
      vape_btt = 1;
    }

    if (!vape_state && vape_btt) {  // если кнопка ПАРИТЬ отпущена
      if (millis() - vape_press > 180) {
        vape_release_count = 0;
        vape_mode = 0;
        vape_flag = 0;
      }
      vape_btt = 0;
      if (vape_mode == 1) {
        vape_release_count = 1;
        vape_press = millis();
      }
      if (vape_mode == 2) vape_release_count = 2;

      digitalWrite(mosfet, 0);
      disp.clear();
      mode_flag = 0;

      // если есть изменения в настройках, записать в память
      if (change_v_flag) {
        EEPROM.writeInt(0, volts);
        change_v_flag = 0;
      }
      if (change_w_flag) {
        EEPROM.writeInt(2, watts);
        change_w_flag = 0;
      }
      if (change_o_flag) {
        EEPROM.writeFloat(4, ohms);
        change_o_flag = 0;
      }
      // если есть изменения в настройках, записать в память
    }
    if (vape_state && !flag) { // если акум сел, а мы хотим подымить
      disp.clear();
      disp_send(LOWB);
      delay(1000);
      vape_flag = 1;
    }
    //---------отработка нажатия кнопки парения-----------
  }

  if (wake_up_flag) wake_puzzle();                   // вызвать функцию 5 нажатий для пробудки

  if (millis() - wake_timer > sleep_timer * 1000) {  // если кнопки не нажимались дольше чем sleep_timer секунд
    good_night();
  }
} // конец loop

//------функция, вызываемая при выходе из сна (прерывание)------
void wake_up() {
  digitalWrite(disp_vcc, HIGH);  // включить дисплей
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
  wake_timer = millis();         // запомнить время пробуждения
  wake_up_flag = 1;
  vape_release_count = 0;
  vape_mode = 0;
  vape_flag = 0;
  mode_flag = 1;
}
//------функция, вызываемая при выходе из сна (прерывание)------

//------функция 5 нажатий для полного пробуждения------
void wake_puzzle() {
  detachInterrupt(0);    // отключить прерывание
  vape_btt_f = 0;
  boolean wake_status = 0;
  byte click_count = 0;
  while (1) {
    vape_state = !digitalRead(butt_vape);

    if (vape_state && !vape_btt_f) {
      vape_btt_f = 1;
      click_count++;
      switch (click_count) {
        case 1: disp_send(V);
          break;
        case 2: disp_send(A);
          break;
        case 3: disp_send(P);
          break;
        case 4: disp_send(E);
          break;
      }
      if (click_count > 4) {               // если 5 нажатий сделаны за 3 секунды
        wake_status = 1;                   // флаг "проснуться"
        break;
      }
    }
    if (!vape_state && vape_btt_f) {
      vape_btt_f = 0;
      delay(70);
    }
    if (millis() - wake_timer > 3000) {    // если 5 нажатий не сделаны за 3 секунды
      wake_status = 0;                     // флаг "спать"
      break;
    }
  }
  if (wake_status) {
    wake_up_flag = 0;
    disp.clear();
    delay(100);
  } else {
    disp.clear();
    good_night();     // спать
  }
}
//------функция 5 нажатий для полного пробуждения------

//-------------функция ухода в сон----------------
void good_night() {
  disp_send(BYE);      // попрощаться
  delay(500);
  disp.clear();
  Timer1.disablePwm(mosfet);    // принудительно отключить койл
  digitalWrite(mosfet, LOW);    // принудительно отключить койл
  delay(50);

  // подать 0 на все пины питания дисплея
  digitalWrite(disp_vcc, LOW);

  delay(50);
  attachInterrupt(0, wake_up, FALLING);                   // подключить прерывание для пробуждения
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);    // спать. mode POWER_OFF, АЦП выкл
}
//-------------функция ухода в сон----------------

//----------режим теста кнопок----------
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
//----------режим теста кнопок----------

// функция вывода моих слов на дисплей
void disp_send(byte sym[]) {
  for (int i = 0; i < 4; i++) {
    disp.set(SYM[sym[i]], 3 - i);
  }
}

void timerIsr()  //нужно для дисплея
{
  disp.timerIsr();
}

// символы для дисплея
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
