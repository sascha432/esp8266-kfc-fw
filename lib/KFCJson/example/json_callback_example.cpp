/**
 * Author: sascha_lammers@gmx.de
 */

#include "JsonCallbackReader.h"
#include <StreamString.h>

void callback_test() {

	const char *test = "{\"status\":\"OK\",\"details\":{\"countryCode\":\"CA\",\"zoneName\":\"America/Vancouver\",\"abbreviation\":\"PST\",\"gmOffset\":-28800,\"dst\":\"0\",\"zoneStart\":1552212000,\"zoneEnd\":1583661599,\"nextAbbreviation\":\"PDT\",\"timestamp\":1542119691,\"formatted\":\"2018-11-13 14:34:51\"}}";
	StreamString stream;
    stream.write(reinterpret_cast<const uint8_t *>(test), strlen(test));

	JsonCallbackReader reader(stream, [](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
		DEBUG_JSON_CALLBACK(Serial.printf, key, value, json);
		// DEBUG_JSON_CALLBACK_P(Serial.printf_P, key, value, json);
		return true;
	});

	reader.parse();
}
