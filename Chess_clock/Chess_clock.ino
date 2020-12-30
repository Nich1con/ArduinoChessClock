/*
   Шахматные часы для контроля рабочего времени
   Два потока времени - обратный отсчет рабочего времени и прямой счет перерывов
   Встроенные оповещения о длительном перерыве (если необходимо)
   Проверка присутствия на рабочем месте (если необходимо)
*/

/*---------------------------------------Настройки----------------------------------------------*/
#define BIG_BTN     2       // Пин для большой кнопки
#define LEFT_BTN    3       // Пин для левой кнопки управления
#define RIGHT_BTN   4       // Пин для правой кнопки управления
#define BUZZER_PIN  5       // Пин для баззера (если нужен)
#define BUZZER_TONE 2500    // Частота баззера в герцах
#define BUZZER_DUR  300     // Длинна сигнала баззера в мс
#define CLK         A5      // Пин дисплея CLK
#define DIO         A4      // Пин дисплея DIO

#define CHECK_ENABLE   1    // Включить проверку наличия на рабочем месте (0 - выкл, 1 - вкл)
#define CHECK_TIMEOUT 15    // Таймаут на нажатие кнопки при проверке в секундах
#define CHECK_PERIOD 3      // Постоянный период проверки в минутах
#define CHECK_PERIOD_RAND 1 // Генерировать опрос со случайным периодом  (0 - выкл, 1 - вкл)
#define CHECK_PERIOD_MIN 1  // Мин период опроса в минутах
#define CHECK_PERIOD_MAX 15 // Макс период опроса в минутах

#define IDLE_ALERT 1        // Делать оповещения о долгом перерыве 
#define IDLE_ALERT_PERIOD 5 // Период оповещений в минутах
/*-----------------------------------------------------------------------------------------------*/

#include <GyverButton.h>
GButton big(BIG_BTN, HIGH_PULL);
GButton left(LEFT_BTN, HIGH_PULL);
GButton right(RIGHT_BTN, HIGH_PULL);

#include <GyverTM1637.h>
GyverTM1637 disp(CLK, DIO);

int8_t hours_need;   // Ввод часов
int8_t hours_left;   // Часы на обратный отсчет
int8_t hours_idle;   // Часы безделья
int8_t minutes_need; // Ввод минут
int8_t minutes_left; // Минуты на обратный отсчет
int8_t minutes_idle; // Минуты безделья

#if CHECK_PERIOD_RAND
  uint32_t checkPeriod = random(CHECK_PERIOD_MIN * 60000UL, CHECK_PERIOD_MAX * 60000UL); // Если рандом период - получаем этот случайный период
#else
  uint32_t checkPeriod = CHECK_PERIOD * 60000UL;                                         // А если постоянный - просто взять из константы
#endif


void setup() {
  disp.clear();       // Очистить дисп
  disp.brightness(7); // Макс яркость (0-7)
  disp.point(0);      // Выкл точки

  big.setTimeout(1500);      // Таймаут длительного удержания красной кнопки
  left.setStepTimeout(120);  // Таймаут инкремента при удержании других кнопок
  right.setStepTimeout(120);

  pinMode(BUZZER_PIN, OUTPUT); // Пин баззера как выход
}

void loop() {
  static uint32_t pointTimer = 0;
  static bool pointState = false;
  static bool insertHours = true;

  big.tick();   // опрос кнопок
  left.tick();
  right.tick();

  if (big.isClick()) {           // короткое нажатие
    insertHours = !insertHours;  // инверт флага
  }

  if (big.isHolded()) { // длинное нажатие
    workCycle();        // переход в рабочий цикл
  }

  if (left.isClick() or left.isStep()) {  // Короткое нажатие или удержание
    if (insertHours) {                    // Если ввод часов
      if (--hours_need < 0) {             // Уменьшаем и сравниваем
        hours_need = 12;                  // Если меньше 0 - переполняем
      }
    } else {                              // Если ввод минут
      if (--minutes_need < 0) {           // Уменьшаем и сравниваем
        minutes_need = 59;                // Если меньше 0 - переполняем
      }
    }
  }

  if (right.isClick() or right.isStep()) { // Аналогично для другой кнопки
    if (insertHours) {
      if (++hours_need > 12) {
        hours_need = 0;
      }
    } else {
      if (++minutes_need > 59) {
        minutes_need = 0;
      }
    }
  }

  if (millis() - pointTimer >= 600) {   // мигание точкой каждые 600 мс
    pointTimer = millis();
    pointState = !pointState;
    disp.point(pointState);
  }

  disp.displayClock(hours_need, minutes_need); // вывод вводимых часов/минут
}

void workCycle(void) {              // цикл отсчета рабочего времени
  static uint32_t minsTimer = 0;
  static bool checkFlag = false;
  #if CHECK_ENABLE
    static uint32_t checkTimer = 0;
  #endif
  #if IDLE_ALERT
    bool buzzerEnable = false; // переменные и флаги
  #endif

  hours_idle = 0;                   // передаем нужные значения и обнуляем перерывы
  minutes_idle = 0;
  minutes_left = minutes_need - 1;
  hours_left = hours_need;

  tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR); // подаем сигнал
  disp.point(1);                             // вкл точку

  minsTimer = millis();
  #if CHECK_ENABLE
    checkTimer = millis();                   // обновляем таймеры
  #endif

  for (;;) {    // бесконечный цикл
    big.tick(); // опрос кнопки

    if (big.isClick()) {    // короткое нажатие
      if (checkFlag) {      // Если флаг установлен
        checkFlag = false;  // сброс флага
      } else {              // Если флага нет
        idleCycle();        // перейти в режим счета перерыва
      }
    }

    if (big.isHolded()) {   // Длительное нажатие на клаву
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR); // подать сигнал и вернуться назад
      return;
    }

    if (millis() - minsTimer >= 60000) { // минутный таймер
      minsTimer = millis();
      if (--minutes_left < 0) {   // уменьшаем минуты и сравниваем, меньше нуля - идем дальше
        if (hours_left-- > 0) {   // Уменьшаем еще и часы и сравниваем, больше нуля - идем дальше
          minutes_left = 59;      // Переполняем часы
        } else {                  // А если время закончилось
          disp.point(0);          // выкл точку
          disp.displayByte(_empty, _E, _n, _d); // вывод надписи
          for (uint8_t i = 0; i < 3; i++) {     // цикл с подачей сигналов
            tone(BUZZER_PIN, BUZZER_TONE);
            delay(BUZZER_DUR);
            noTone(BUZZER_PIN);
            delay(BUZZER_DUR);
          }
          do {
            big.tick();               // Опрашивать кнопку
          } while (!big.isClick());   // пока ее нажмут
          disp.point(1);              // как нажали - вкл точку
          disp.displayClockTwist(hours_idle, minutes_idle, 50); // вывести время безделья
          do {
            big.tick();               // Опрашивать кнопку
          } while (!big.isClick());   // пока ее нажмут
          return;                     // По нажатию вернуться назад
        }
      }
    }

  #if CHECK_ENABLE                              // Если проверка включена
    if (millis() - checkTimer >= checkPeriod) { // Таймер опроса
      checkTimer = millis();
      checkFlag = true;
      #if IDLE_ALERT
        buzzerEnable = true;
      #endif
    #if CHECK_PERIOD_RAND
      checkPeriod = random(CHECK_PERIOD_MIN * 60000UL, CHECK_PERIOD_MAX * 60000UL); // получение случайного периода
    #endif
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR);    // Подать первый сигнал
    }

    #if IDLE_ALERT
      buzzerEnable = false;
      if (buzzerEnable and millis() - checkTimer >= BUZZER_DUR * 2UL) { // После окончания подать еще один сигнал по таймеру
    #else
      if (millis() - checkTimer >= BUZZER_DUR * 2UL) { // После окончания подать еще один сигнал по таймеру
    #endif
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR);
                                                 // Выкл баззер
    }

    if (checkFlag and millis() - checkTimer >= CHECK_TIMEOUT * 1000UL) { // Таймаут нажатия на кнопку
      checkFlag = false;
      idleCycle();                                                       // автоматом идем в режим безделья
    }
  #endif // CHECK_ENABLE


    disp.displayClock(hours_left, minutes_left);                         // Вывод часов и минут обратного отсчета
  }
}

void idleCycle(void) {
  static uint32_t pointTimer = 0;
  static uint32_t minsTimer = 0;
  static bool pointState = false;
  #if IDLE_ALERT
    static uint32_t alertTimer = 0;
    static bool buzzerEnable = false;
  #endif

  disp.point(0);                    // выкл точку
  disp.displayByte(_1, _d, _L, _E); // вывод надписи
  tone(BUZZER_PIN, BUZZER_TONE);    // подача сигнала
  delay(BUZZER_DUR);  
  noTone(BUZZER_PIN);
  delay(BUZZER_DUR);

  pointTimer = millis(); // апдейт таймеров
  minsTimer = millis();
  #if IDLE_ALERT
    alertTimer = millis();
  #endif

  for(;;) {     // бесконечный цикл
    big.tick(); // опрос кнопки

    if (big.isClick()) {    // нажатие на кнопку
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR); // подать сигнал
      disp.point(1);        // вкл точку
      return;               // вернуться назад
    }

    if (millis() - minsTimer >= 60000) {  // минутный таймер
      minsTimer = millis(); 
      if (++minutes_idle > 59) {          // инкремент минут и проверка на переполнение
        minutes_idle = 0;                 // переполнение минут
        hours_idle++;                     // инкремент часа
      }
    }

    if (millis() - pointTimer >= 1500) {  // таймер мигания точек
      pointTimer = millis();
      pointState = !pointState; 
      disp.point(pointState);
    }

  #if IDLE_ALERT // если включено напоминание о длительном безделье
    if (millis() - alertTimer >= IDLE_ALERT_PERIOD * 60000UL) {  // таймер опроса безделья
      alertTimer = millis();
      buzzerEnable = true;  
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR);                  // подать первый сигнал
    }

    if (buzzerEnable and millis() - alertTimer >= BUZZER_DUR * 2UL) { // Таймер для второго сигнала без delay
      tone(BUZZER_PIN, BUZZER_TONE, BUZZER_DUR);                      // Подача второго сигнала
      buzzerEnable = false;
    }
  #endif

    disp.displayClock(hours_idle, minutes_idle);                      // вывод часов и минут безделья в реальном времени
  }
}
