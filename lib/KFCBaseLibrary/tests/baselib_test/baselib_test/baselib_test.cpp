// baselib_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <assert.h>
#include "Arduino_compat.h"
#include "Buffer.h"
#include "PrintBuffer.h"
#include "PrintHtmlEntitiesString.h"
#include "BufferStream.h"

void test_string() {

	String s1 = "test";

	s1.remove(0, 1);
	assert(s1.equals("est"));

	s1.replace("es", "tes");
	assert(s1.equalsIgnoreCase("TEST"));

	s1.toUpperCase();
    assert(s1 == "TEST");

	s1.toLowerCase();
    assert(s1 == "test");


}

void test_buffer() {

	Buffer b1;

	b1 += 't';
	b1 += 'e';
	b1 += 's';
	b1 += 't';

	assert(b1.length() == 4);
	assert(b1.toString().equals("test"));


	PrintBuffer b2;

	b2.printf("%d\n", 123);
	assert(b2.toString().equals("123\n"));

	b2.clear();
	b2.write("123", 3);
	assert(b2.toString().equals("123"));

	BufferStream b3;

	b3.print("test");
	assert(b3.seek(0, SeekSet) == true);

	assert(b3.readString() == "test");

	PrintHtmlEntitiesString b4;

	b4.print(HTML_S(br) "<notag>");
	assert(b4 == "<br>&lt;notag&gt;");


}

int main()
{
	test_string();
	test_buffer();
}
