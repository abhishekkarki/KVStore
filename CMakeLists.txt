# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS /Users/abhishek/Documents/project/KV_Store/new-test/exploring-blinking-on-esp32-testing/components/esp-idf-lib/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nvs_example)
