
#include "KFCTimezone.h"
#include <iostream>

int main()
{
	char buf[32];
	String url = "http://www.d0g3.space/timezone/api.php?by=zone&format=json&zone=${timezone}";
	RemoteTimezone rtz;

	rtz.setUrl(url);
	rtz.setTimezone("America/Vancouver");
	rtz.get();
	
//	get_default_timezone().setAbbreviation(F("PDT"));
//	get_default_timezone().setOffset(-3600 * 7);
//	get_default_timezone().setDst(false);

	timezone_strftime_P(buf, sizeof(buf), PSTR("%D %T %Z %z"), timezone_localtime(nullptr));

	std::cout << buf << std::endl;
}
