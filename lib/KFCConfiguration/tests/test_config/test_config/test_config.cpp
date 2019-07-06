// test_config.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Arduino_compat.h>
#include <Buffer.h>
#include <algorithm>
#include <map>
#include "Configuration.h"
#include "ConfigurationParameter.h"

EEPROMFile EEPROM;


typedef struct {
    char string1[100];
    char string2[100];
} SubConfig1_t;

typedef struct {

    char string1[100];
    SubConfig1_t sub1;
    
} Configuration_t;

int main() {

    EEPROM.begin();
    //EEPROM.clear();
    //EEPROM.commit();
    Configuration config(0, (uint16_t)EEPROM.length());

    if (!config.read()) {
        printf("Failed to read config\n");
    }

    auto ptr = config.getString(CONFIG_GET_HANDLE(config.string1));
    printf("%s\n", ptr);

    config.release();

    config.setString(CONFIG_GET_HANDLE(config.string1), "bla2");

    if (config.isDirty()) {
        config.write();
    }

    return 0;
}
