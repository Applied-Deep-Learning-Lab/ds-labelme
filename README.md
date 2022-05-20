# ds-labelme

## Установка deepstream

Для начала требуется установить deepstream (Если он ещё не установлен). Инструкция по следующей ссылке
https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_Quickstart.html

## Сборка и установка ds-labelme

Следует установить следующие зависимости
```bash 
sudo apt install build-essential -y
sudo apt install git libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good libjson-glib-1.0-0 libjson-glib-1.0-0-dev
```
Клонируем репозиторий
```bash
cd path/to/project
git clone https://github.com/ProgriX/ds-labelme.git
cd ds-labelme
```
Настраиваем пути и конфиги
```bash
./install.sh jetson # Для джетсона
./install.sh apollo # Для аполло
```

После чего в папке *platforms/(italic)* есть следующие файлы
*paths.sh(italic)* - Файл настроек с путями к библиотекам и заголовочным файлам.
Данная конфигурация нуждается в редактировании если какие либо библиотеки установленны не в стандартной дериктории или имеют другую версию  

*config/deepstream_app_config.txt(italic)* - основной файл конфугурации.


И собираем
```bash
make -C sources/
```

## Запуск

Для запуска нужно скачать дополнительные данные: Сети и пример видео:
https://drive.google.com/file/d/1KG81dmKlVLi9O2qmGdGusAb78VNJJA-n/view?usp=sharing

Разархивировать в корень проекта. 

После чего вызвать команду 

```bash
./run.sh
```
