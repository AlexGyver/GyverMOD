# Вейп БоксМод GyverMOD

## Папки

- **GyverMOD_libs** - папка с файлами библиотек, установить в C:\Program Files\Arduino\libraries  
- **GyverMOD_v._1.0.*_DISP** - прошивка для Ардуино, актуальная версия с варивольтом
- **GyverMOD_v._1.2.*_DISP** - прошивка для Ардуино, актуальная версия с варивольтом и ручным вариваттом
- **images** - рендеры 3D модели, компоновка

**ВНИМАНИЕ! ПУТЬ К ПАПКЕ СО СКЕТЧЕМ НЕ ДОЛЖЕН СОДЕРЖАТЬ РУССКИХ СИМВОЛОВ!
ВО ИЗБЕЖАНИЕ ПРОБЛЕМ ПОЛОЖИТЕ ПАПКУ В КОРЕНЬ ДИСКА С.**

### Схема подключения, версия 1.0
![GyverMOD 1.0](https://github.com/AlexGyver/GyverMOD/blob/master/scheme_1.0.jpg)

### Компоновка, версия 1.0
![GyverMOD 1.0](https://github.com/AlexGyver/GyverMOD/blob/master/images/GM%201.0/comp1.jpg)

### Схема подключения, версия 1.2
![GyverMOD 1.0](https://github.com/AlexGyver/GyverMOD/blob/master/scheme_1.2.jpg)

### Компоновка, версия 1.2
![GyverMOD 1.0](https://github.com/AlexGyver/GyverMOD/blob/master/images/GM%201.2/comp3.jpg)

## Нужная инфа
### Страница проекта на моём сайте
http://AlexGyver.ru/gyvermod/

### Настройки в прошивке (1 - вкл, 0 - выкл): 
* initial_calibration - калибровка вольтметра 1 - включить, 0 - выключить
* welcome - приветствие (слова GYVER VAPE при включении), 1 - включить, 0 - выключить
* battery_info - отображение напряжения аккумулятора при запуске, 1 - включить, 0 - выключить
* sleep_timer - таймер сна в секундах
* vape_threshold - отсечка затяжки, в секундах
* turbo_mode - турбо режим 1 - включить, 0 - выключить
* battery_percent - отображать заряд в процентах, 1 - включить, 0 - выключить
* battery_low - нижний порог срабатывания защиты от переразрядки аккумулятора, в Вольтах!

###Статья первые шаги с Arduino
► http://alexgyver.ru/arduino-first/
