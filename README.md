# Мультикаст-открытки

## Цель проекта

Реализовать утилиту create_postal, которая на основе видеоролика и заданного мультикаст-адреса формирует исполняемый ELF-файл-«открытку».
Запуск такой «открытки» обеспечивает трансляцию встроенного видеоролика в виде зацикленного потока по указанному мультикаст-адресу.

## Требования к функционалу

В результате сборки проекта должно получиться одно приложение: create_postal, оно является универсальным «создателем открыток».
Вызов приложения осуществляется в формате  
./create_postal \<video_file\> \<multicast_address\>  
\<video_file\> — путь к исходному видеофайлу (например, gendalf.mp4),  
\<multicast_address\> — IPv4-адрес в диапазоне мультикаст (например, 224.1.1.1).

На выходе create_postal формирует ELF-файл с именем, совпадающим с названием видеофайла без расширения.
./create_postal gendalf.mp4 224.1.1.1 -> gendalf  
При запуске ELF-открытки файл начинает вещание зацикленного видеопотока, вещание ведётся на указанный мультикаст-адрес, любое устройство в локальной сети может подключиться к потоку (например, через VLC) и просматривать видео.

Видеофайл должен быть встроен в открытку на этапе создания. Передача ELF-открытки на другое устройство или по сети должна позволять её запуск без наличия исходного видео в файловой системе целевого устройства.

## Требования к инструментарию

* Основной язык выполнения: С, не менее 90% кода.
* ЗАПРЕЩЕНО использовать system() и его идеологические аналоги без согласования с куратором
* Запускаемая платформа: Linux
* Допускается использование сторонних библиотек для работы с мультимедиа, но вещание должно быть реализовано рабочей группой.

## Структура директорий

Для каждого модуля выделена отдельная директория с соответствующим названием.

* common - общие для нескольких модулей файлы
* create_postal - сама утилита, сборщик открыток
* postal_template - шаблон открытки (чтение файла и отправка дейтаграмм)

В папке utils находятся внутренние утилиты, необходимые для сборки целевого исполняемого файла

* injector - внедряет postal_template в create_postal

В папке модуля находятся директории:

* src - исходный код (.h и .c файлы)
* bin - артефакты сборки

## Сборка

### Зависимости

Для **сборки** проекта необходимо наличие gcc, make, pkg-cрonfig, а также статические библиотеки [FFmpeg](https://github.com/ffmpeg/ffmpeg) ([репозиторий с собранными статическими версиями](https://github.com/savonet/ffmpeg-static-build))

Автоматически библиотеки можно установить используя:

```bash
make install_dependencies
```

### Команды сборки

#### Сборка всего проекта

```bash
make 
```

В ходе выполнения этой команды последовательно собираются:

1. Вспомогательная утилита внедрения utils/injector.

2. Бинарный шаблон открытки postal_template.

3. Основной исполняемый файл create_postal с автоматическим внедрением внутри него собранного шаблона.

Собранный файл помещается в bin/create_postal  
При сборке можно задать следующие переменные:

| Переменная    | Описание                                                                          | Пример                                        |  
| ---           | ---                                                                               | ---                                           |  
| DEBUG         | Сборка с отладочной информацией                                                   | ```make DEBUG=1```                            |  
| CFLAGS        | Переопределение флагов компилятора. По умолчанию ```-static -Wall -Wpedantic```   | ```make CFLAGS="-Wall -O2"```                 |  
| INJECTOR      | Путь к собранной утилите инъекции                                                 | ```make INJECTOR=/usr/local/bin/injector```   |  
| TEMPLATE      | Путь к собранному шаблону                                                         | ```make TEMPLATE=bin/postal_template```       |  

#### Сборка модулей

При необходимости собрать только определённый модуль, можно вызвать следующие цели:

```bash
# Сборка только утилиты внедрения
make injector

# Сборка только бинарного шаблона открытки
make template

# Сборка только создателя открыток с явным указанием компонентов
# если не указать цели, то соберутся с помощью соответствующей цели,
# но промежуточный результат не сохранится 
make create_postal INJECTOR=path/to/injector TEMPLATE=path/to/template
```

#### Тестирование

Запуск модульных тестов для postal_template и create_postal

```bash
make test
```

#### Очистка

Очистка артефактов сборки и временных файлов

```bash
make clean
```

## Использование

В общем случае использование утилиты выглядит следующим образом:

```bash
./create_postal <video> <multicast-ip>[:port]
```

Утилита create_postal принимает 2 аргумента:

* ```<video>``` - имя видеофайла (принимает видео в зависимости от используемого видеокодека. Принимаемые кодеки: H.264, HEVC, AV1. На видео с длиной более 5 минут не гарантирована правильная работа утилиты в связи с техническими ограничениями)
* ```<multicast-ip>[:port]``` - IP мультикаст адрес, с необязательным указанием порта через двоеточие (если порт не указан - 6000)

### Примеры использования

```bash
./create_postal my_vid.mp4 224.1.1.1
./create_postal video.mkv 224.1.1.1:6222
```

## Описание возможных ошибок

### Ошибки сборки

При попытке установить зависимости через команду

```bash
make install_dependencies
```

Может возникнуть следующая ошибка:

```txt
Unknown package manager. Install ffmpeg manually (static libraries are required)
```

Автоматическая установка необходимых для сборки библиотек происходит только на системах с пакетным менеджером apt (т.е. Ubuntu, Debian и пр.). Установить библиотеки на другие дистрибутивы можно через ваш пакетный менеджер или через официальный [github](https://github.com/FFmpeg/FFmpeg) FFmpeg

При попытке сборки compiler может выдать следующие ошибки

| Код ошибки                     | Вывод ошибки                     | Описание                              |  
| ---                            | ---                              | ---                                   |  
| INJECTOR_ERROR_READ            | Failed to write target file      | Невозможно прочитать шаблон открытки  |  
| INJECTOR_ERROR_WRITE           | Failed to read injectable file   | Невозможно слить исполняемые файлы    |  

Проверьте наличие файлов и убедитесь, что у вас есть права на доступ к нему

### Ошибки при запуске

Способы устранения приложены к ошибкам, где их устранение возможно. В общем случае, если не удалось создать открытку - попробуйте заново

#### Ошибки модуля args

| Код ошибки                                | Вывод ошибки                                                  | Описание                                              | Способ устранения                                                                 |  
| ---                                       | ---                                                           | ---                                                   | ---                                                                               |  
| POSTAL_ARGS_ERROR_NULL                    | Null argument                                                 | Данные переданные на валидацию равны NULL             | Ошибка не решаема на стороне клиента                                              |  
| POSTAL_ARGS_ERROR_COUNT                   | Usage: create_postal <video_file> <multicast_address[:port]>  | Неверное количество аргументов (должно быть два)      | Использование утилиты: `create_postal <video_file> <multicast_ip>[:port]`         |  
| POSTAL_ARGS_ERROR_VIDEO_OPEN              | Video file does not exist or is not readable                  | Не удалось открыть видеофайл                          | Проверить существование видеофайла и права доступа к нему                         |  
| POSTAL_ARGS_ERROR_VIDEO_TYPE              | Video path does not refer to a regular file                   | Видео не является файлом                              | Видео должно быть файлом                                                          |  
| POSTAL_ARGS_ERROR_ADDRESS_FORMAT          | Invalid IPv4 address format                                   | Неправильная запись IP                                | Формат IPv4 это 4 числа от 0 до 255, написанные через точку, например 225.1.44.2  |  
| POSTAL_ARGS_ERROR_ADDRESS_NOT_MULTICAST   | IPv4 address is outside multicast range 224.0.0.0/4           | IP не является multicast                              | Утилита использует групповые IP в диапазоне от 224.0.0.0 до 239.255.255.255       |  
| POSTAL_ARGS_ERROR_PORT_FORMAT             | UDP port must be an integer                                   | Неправильный формат порта                             | Порт - это целочисленное число                                                    |  
| POSTAL_ARGS_ERROR_PORT_RANGE              | UDP port must be in range 1-65535                             | Порт выходит за границы                               | Принимаемый диапазон портов: 1 - 65535                                            |  
| POSTAL_ARGS_ERROR_VIDEO_NAME              | Video file name must have an extension                        | Видеофайл обязан иметь расширение                     | Переименуйте файл, добавив в конец формат                                         |

#### Ошибки модуля compiler

| Код ошибки                     | Вывод ошибки                     | Описание                        |  
| ---                            | ---                              | ---                             |  
| COMPILER_INVALID_METADATA      | Invalid metadata                 | Некорректные метаданные         |  
| COMPILER_ERROR_WRITE_METADATA  | Failed to write metadata         | Ошибка записи метаданных        |  
| COMPILER_ERROR_COPY_MPEGTS     | Failed to copy video stream      | Ошибка копирования видеопотока  |  
| COMPILER_ERROR_COPY_TEMPLATE   | Failed to copy postal template   | Ошибка копирования шаблона      |  
| COMPILER_ERROR_CREATE_OUTPUT   | Failed to create output file     | Ошибка создания выходного файла |  

#### Ошибки модуля metadata

| Код ошибки                                | Вывод ошибки                          | Описание                                                  |  
| ---                                       | ---                                   | ---                                                       |  
| POSTAL_METADATA_ERROR_NULL                | Null metadata argument                | Передан null указатель на структуру метаданных            |  
| POSTAL_METADATA_ERROR_IO                  | Metadata I/O error                    | Ошибка записи/чтения файла                                |  
| POSTAL_METADATA_ERROR_FILE_TOO_SMALL      | File is too small to contain metadata | Файл слишком мал, чтобы содержать метаданные              |  
| POSTAL_METADATA_ERROR_MAGIC               | Invalid metadata magic number         | Неисправные метаданные: некорректное магическое число     |  
| POSTAL_METADATA_ERROR_VERSION             | Unsupported metadata version          | Неисправные метаданные: не поддерживающая версия          |  
| POSTAL_METADATA_ERROR_SIZE                | Invalid metadata structure size       | Неисправные метаданные: некорректный размер структуры     |  
| POSTAL_METADATA_ERROR_MULTICAST_ADDRESS   | Invalid multicast IPv4 address        | Неисправные метаданные: некорректный IP адрес             |  
| POSTAL_METADATA_ERROR_PORT                | Invalid UDP port                      | Неисправные метаданные: некорректный порт                 |  
| POSTAL_METADATA_ERROR_VIDEO_RANGE         | Invalid embedded video location       | Неисправные метаданные: некорректное расположение видео   |  
| POSTAL_METADATA_ERROR_BITRATE             | Invalid video bitrate                 | Неисправные метаданные: некорректный битрейт              |  

#### Ошибки модуля remux

| Код ошибки                | Вывод ошибки                             | Описание                                          |
| ---                       | ---                                      | ---                                               |  
| REMUX_NOMEM               | Memory allocation failed                 | Возникает при недостатке памяти                   |  
| REMUX_ARGERR              | Null pointer passed for file descriptor  | Указатель на дескриптор оказался NULL             |  
| REMUX_TMPFILE_ERR         | Failed to create temporary file          | Не удалось создать временный файл в tmp           |  
| REMUX_INPUT_OPEN_ERR      | Failed to open input file                | Невозможно открыть видео                          |  
| REMUX_STREAM_INFO_ERR     | Failed to retrieve stream information    | Невозможно получить данные потоков                |  
| REMUX_OUT_CTX_ERR         | Failed to allocate output context        | Не удалось создать выходной контекст видео        |  
| REMUX_OUT_STREAM_ERR      | Failed to initialize output streams      | Не удалось инициализировать выходные потоки       |  
| REMUX_IO_INIT_ERR         | Failed to initialize I/O context         | Не удалось инициализировать правила ввода/вывода  |  
| REMUX_WRITE_HEADER_ERR    | Failed to write header to output file    | Не удалось записать метаданные в файл             |  
| REMUX_PROCESS_ERR         | Error processing and writing packets     | Не удалось обработать пакеты                      |  
| REMUX_NO_SUPPORTED_VIDEO  | No supported video codec found           | Не найден поддерживаемый видеопоток               |  
| REMUX_BAD_VIDEO           | Failed to create temporary video         | Не удалось создать временное видео в tmp          |

#### Ошибки модуля reader

| Код ошибки                    | Вывод ошибки                                      | Описание                               |
| ---                           | ---                                               | ---                                    |  
| POSTAL_READER_ERR_ARG         | Invalid arguments for reading the postcard        | Некорректные аргументы                 |  
| POSTAL_READER_ERR_OPEN        | Couldn't open the file                            | Не удалось открыть файл                |  
| POSTAL_READER_ERR_STAT        | Failed to retrieve the size of the postcard file  | Не удалось получить размер файла       |  
| POSTAL_READER_ERR_MMAP        | Failed to load the postcard file into memory      | Не удалось отобразить файл в память    |  
| POSTAL_READER_ERR_METADATA    | Postcard metadata is missing or corrupted         | Метаданные отсутствуют или повреждены  |  

#### Ошибки модуля broadcaster

| Код ошибки                    | Вывод ошибки                                      | Описание                                      |
| ---                           | ---                                               | ---                                           |  
| POSTAL_BROADCAST_ERR_ARG      | Invalid arguments for broadcasting the postcard   | Некорректные аргументы                        |  
| POSTAL_BROADCAST_ERR_SOCKET   | Failed to create the UDP socket                   | Не удалось создать UDP-сокет                  |  
| POSTAL_BROADCAST_ERR_CLOCK    | Failed to get the current time                    | Не удалось получить получить текущее время    |  
| POSTAL_BROADCAST_ERR_SEND     | Failed to send the datagram                       | Не удалось отправить дейтаграмму              |  
| POSTAL_BROADCAST_ERR_MEMORY   | Not enough memory for broadcasting the postcard   | Метаданные выделить память                    |  
| POSTAL_BROADCAST_ERR_STREAM   | Postcard video is not a valid MPEG-TS stream      | Видео не является MPEG-TS                     |  

#### Ошибки модуля ts_loop

| Код ошибки                    | Вывод ошибки                                      | Описание                                      |
| ---                           | ---                                               | ---                                           |  
| POSTAL_TS_LOOP_ERR_ARG        | Wrong argument send to loop                       | Некорректные аргументы                        |  
| POSTAL_TS_LOOP_ERR_STREAM     | Postal is not MPEG-TS                             | Видео не является на MPEG-TS                  |  
