# Secretlock
Замок, реагирующий на последовательность стуков. В основе используется код из проекта [AlexGyver](https://github.com/AlexGyver/SecretKnockLock).
Используется штатная ардуиновская библиотека [Servo library](https://www.arduino.cc/en/reference/servo).
Проект начат в Arduino IDE и продолжен в VSCode с фреймворком Platformio.

## Описание проекта
Замок с сервоприводом и датчиком касания
- Записывает последовательность стуков (время между "ударами")
- Сохраняет записанную последовательность в энергонезависимую память
- При настукивании правильной последовательности открывает замок

## Схема
 
(https://github.com/eulampy/secretlock/blob/master/doc/Schematic_secret_lock.png)

<a id="chapter-3"></a>
## Материалы и компоненты
* Arduino NANO https://aliexpress.ru/item/32353404307.html
* Серво https://aliexpress.ru/item/32952155231.html
* Сенсорная кнопка на чипе TTP223 (красная плата) https://aliexpress.ru/item/32788526867.html
