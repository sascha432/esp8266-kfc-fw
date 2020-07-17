/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// https://tools.ietf.org/html/rfc854

#define NVT_IAC                 0xff    // 255 - Data Byte 255. IAC
#define NVT_DONT                0xfe    // 254 - Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option. DON'T
#define NVT_DO                  0xfd    // 253 - Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option. DO
#define NVT_WONT                0xfc    // 252 - Indicates the refusal to perform, or continue performing, the indicated option. WON'T
#define NVT_WILL                0xfb    // 251 - Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option. WILL
#define NVT_SB                  0xfa    // 250 - Indicates that what follows is subnegotiation of the indicated option.
#define NVT_GA                  0xf9    // 249 - The GA signal. Go ahead
#define NVT_EL                  0xf8    // 248 - The function EL. Erase Line
#define NVT_EC                  0xf7    // 247 - The function EC. Erase character
#define NVT_AYT                 0xf6    // 246 - The function AYT. Are You There
#define NVT_AO                  0xf5    // 245 - The function AO. Abort output
#define NVT_IP                  0xf4    // 244 - The function IP. Interrupt Process
#define NVT_BRK                 0xf3    // 243 - NVT character BRK.
#define NVT_DM                  0xf2    // 242 - The data stream portion of a Synch. This should always be accompanied by a TCP Urgent notification.
#define NVT_NOP                 0xf1    // 241 - No operation.
#define NVT_SE                  0xf0    // 240 - End of subnegotiation parameters.

#define NVT_SB_BINARY           0x00    // 0 - TRANSMIT-BINARY
#define NVT_SB_ECHO             0x01    // 1 - ECHO
                                        // 3 - SUPPRESS-GO-AHEAD
                                        // 5 - STATUS
                                        // 6 - TIMING-MARK
                                        // 24 - TERMINAL-TYPE
                                        // 25 - END-OF-RECORD
                                        // 32 - TERMINAL-SPEED
                                        // 33 - TOGGLE-FLOW-CONTROL
                                        // 34 - LINEMODE
                                        // 37 - AUTHENTICATION
                                        // 39 - NEW-ENVIRON
#define NVT_SB_CPO              0x2c    // 44
                                        // 255 - EXTENDED-OPTIONS-LIST (EXOPL)

#define NVT_CPO_SET_BAUDRATE    0x01    // 1
#define NVT_CPO_SET_DATASIZE    0x02    // 2
#define NVT_CPO_SET_PARITY      0x03    // 3
#define NVT_CPO_SET_STOPSIZE    0x04    // 4
#define NVT_CPO_SET_CONTROL     0x05    // 5

// Telnet Com Port Control Option
// https://tools.ietf.org/html/rfc2217
