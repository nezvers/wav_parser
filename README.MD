# Single header Wav file
- Read from file    
- Read from memory buffer    
- Write to a file    

I tried to write it easy to read.    
All features are showcased in [test/main.c](test/main.c)    

To use `*.c` part of library define implementation macro in single place of your code, like this:
```C
#define WAV_IMPLEMENTATION
#include "wav.h"
```
