#include <stdint.h> 
#include <stdio.h>

static inline void
byte_swap(uint32_t * buf, uint32_t len)
{
        int i;
        for (i = 0; i < len; i ++) {
                uint32_t tmp = buf[i];
                buf[i] = ( (uint32_t) ((unsigned char *) &tmp)[0] ) |
                         (((uint32_t) ((unsigned char *) &tmp)[1]) << 8) |
                         (((uint32_t) ((unsigned char *) &tmp)[2]) << 16) |
                         (((uint32_t) ((unsigned char *) &tmp)[3]) << 24);
        }
}


int
main ()
{
	uint32_t a = 10;
	uint32_t b = a;

	byte_swap(&a, 1);

	if ( a == b ) {
		printf("little-endian\n");
		return 0;
	} else {
		printf("big-endian\n");
		return 1;
	}	
}
