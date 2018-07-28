/**
 * Licensed under GNU GPLv3 <http://gplv3.fsf.org/>
 * Copyright © 2018
 *
 * @author Marcin Szalomski (github: @baldram | twitter: @baldram)
 */

#ifndef __ESP_VS1053_LIBRARY_CONSOLE_LOGGER__
    #define __ESP_VS1053_LIBRARY_CONSOLE_LOGGER__

    /**
     * To enable debug, add build flag to your platformio.ini as below (depending on platform).
     *
     * For ESP8266:
     *      build_flags = -D DEBUG_ESP_PORT=Serial
     *
     * For ESP32:
     *      build_flags = -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
     *
     */
    #ifdef ARDUINO_ARCH_ESP32
        #define LOG(...) ESP_LOGD("ESP_VS1053", __VA_ARGS__)
    #elif defined(ARDUINO_ARCH_ESP8266) && defined(DEBUG_ESP_PORT)
        #define LOG(...) DEBUG_ESP_PORT.printf(__VA_ARGS__)
    #else
        #define LOG(...)
    #endif

#endif // __ESP_VS1053_LIBRARY_CONSOLE_LOGGER__
