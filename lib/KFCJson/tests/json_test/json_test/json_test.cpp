// json_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "JsonCallbackReader.h"

int main()
{
	String test = "{\"status\":\"OK\",\"message\":\"\",\"countryCode\":\"CA\",\"zoneName\":\"America/Vancouver\",\"abbreviation\":\"PST\",\"gmOffset\":-28800,\"dst\":\"0\",\"zoneStart\":1552212000,\"zoneEnd\":1583661599,\"nextAbbreviation\":\"PDT\",\"timestamp\":1542119691,\"formatted\":\"2018-11-13 14:34:51\"}";
	StringStream stream = StringStream(test);
	JsonCallbackReader reader(stream, [](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
		printf("callback: %s = %s\n", key.c_str(), value.c_str());
		return true;
	});

	reader.parse();
}

