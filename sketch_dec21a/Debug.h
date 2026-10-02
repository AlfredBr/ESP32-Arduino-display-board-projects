#ifndef DEBUG_H
#define DEBUG_H

#if DEBUG == 1

#define debug(x) Serial.print(x)
#define debugLn(x) Serial.println(x)

#else

#define debug(x)
#define debugLn(x)

#endif

#endif