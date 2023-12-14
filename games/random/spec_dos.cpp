

#ifdef DOS
#include <dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <conio.h>
#include <assert.h>
#include <process.h>

#include "spec.h"
#include "conf.h"
#include "menu.h"

void DBG(const char* str)
{
}

bool has_key_releases()
{
	return true;
}

float sqrtf(float f)
{
	//assert(f>=0);
	return (float)(sqrt((double)f));
}

float logf(float f)
{
	//assert(f>1.E-6);
	return (float)(log((double)f));
}

float floorf(float f)
{
	return (float)(floor((double)f));
}

float sinf(float f)
{
	return (float)(sin((double)f));
}

float cosf(float f)
{
	return (float)(cos((double)f));
}

float expf(float f)
{
	return (float)(exp((double)f));
}


#define _strdup(str) strdup(str)

int fopen_s(FILE** f, const char* path, const char* mode)
{
	if (!f)
		return -1;
	*f = fopen(path,mode);
	return *f==0;
}

void LoadFont(unsigned char* get, const unsigned char* set, int char_height)
{
	outpw(0x03ce,5);		// clear even/odd mode
	outpw(0x03ce,0x0406);	// map vga to a0000
	outpw(0x03c4,0x0402);	// set bitplane 2
	outpw(0x03c4,0x0604);	// clear even/odd mode 

	unsigned char* ptr = (unsigned char*)0xA0000;
	
	if (get)
	{
		for (int i=0; i<256; i++)
		{
			for (int y=0; y<char_height; y++)
				get[i*char_height+y] = ptr[i*32+y];
		}
	}
	
	if (set)
	{
		for (int i=0; i<256; i++)
		{
			for (int y=0; y<char_height; y++)
				ptr[i*32+y] = set[i*char_height+y];
		}
	}
	
	// restore vga
	outpw(0x03c4,0x0302);
	outpw(0x03c4,0x0204);
	outpw(0x03ce,0x1005);
	outpw(0x03ce,0x0E06);	
}

static int  screen_w=0;
static int  screen_h=0;
static int  screen_font=0;
static int  screen_lines=0;

void get_terminal_wh(int* dw, int* dh)
{
	*dw = screen_w;
	*dh = screen_h;
}


#define CLOCK_INT       (0x08)
static  int     fastclock_on = 0;       // Is the clock speed set higher?
static  void    interrupt (*oldfunc)(void);     // interrupt function pointer
static unsigned int tv_fives=0;
static unsigned int tv_ticks=0;

static void interrupt handle_clock(void) 
{
    static  int     callmod = 0;

    /* Increment the time */
    tv_ticks++;
    if (tv_ticks==5824)
    {
        tv_fives++;
        tv_ticks=0;
    }

    /* Increment the callmod */
    callmod = (callmod+1)%64;

    /* If this is the 64th call, then call handler */
    if (callmod == 0) 
            oldfunc();
	else 
            outp(0x20,0x20);        // End of interrupt
}

static void stop_fastclock(void) 
{
    /* See if the clock is on - exit if not */
    if (!fastclock_on) 
	{
            return;
    }

    /* Disable interrupts */
    __asm {cli};

    /* Reinstate the old interrupt handler */
    _dos_setvect(CLOCK_INT, oldfunc);

    /* Reinstate the clock rate to 18.2 Hz */
    outp(0x43, 0x36);       // Set up for count to be sent
    outp(0x40, 0x00);       // LSB = 00  \_together make 65536 (0)
    outp(0x40, 0x00);       // MSB = 00  /

    /* Enable interrupts */
    __asm {sti};

    /* Mark clock is not fast */
    fastclock_on = 0;
}

static int start_fastclock(void) 
{
	/* Make sure the clock is not already set faster */
	if (fastclock_on) 
	{
			return(-1);
	}

	/* Disable interrupts */
	__asm {cli};

	/* Store the old interrupt handler */
	oldfunc = _dos_getvect(CLOCK_INT);

	/* Install the new interrupt handler */
	_dos_setvect(CLOCK_INT,handle_clock);

	/* Increase the clock rate */
	outp(0x43, 0x36);       // Set up for count to be sent
	outp(0x40, 0x00);       // LSB = 00  \_together make 2^10 = 1024
	outp(0x40, 0x04);       // MSB = 04  /

	/* Set the time to seconds since GMT whatever time/date */
	tv_fives = 0;
	tv_ticks = 0;

	/* Enable interrupts */
	__asm {sti};

	/* Mark the clock as faster */
	fastclock_on = 1;
	return(0);
}

unsigned int get_time()
{
    int ms;

    /* Disable interrupts */
    __asm {cli};

    /* Read the time */
    ms = tv_fives * 5000 + tv_ticks * 625 / 728;

    /* Enable interrupts */
    __asm {sti};

    if (!ms)
		ms=1;

	return ms;
}

void sleep_ms(int ms)
{
    int us;
    if (ms<=0)
        return;

    us = ms*1000;

    __asm
    {
            xor eax,eax
            mov ah,86h
            mov ecx, dword ptr [us]
            mov edx, ecx
            and edx, 0ffffh
            shr ecx,16
            int 15h
    }
}

#define Keyboard_Int 9
static void (interrupt *old_keyboard) (void);
static unsigned char key_queue[256]={0};
static int key_queue_head=0;
static int key_queue_size=0;
static int key_queue_lost=0;

static void interrupt keyboard_handler (void)
{
    unsigned char ack, keycode;

    keycode = inp (0x60);
    ack = inp (0x61);
    outp (0x61, ack | 0x80);
    outp (0x61, ack & 0x7f);
    outp (0x20, 0x20);

    // do something with keycode
    // if bit 7 is set then key was released
    // bits 0-6 contain the scan code

	key_queue[ key_queue_head ] = keycode;
	key_queue_head = (key_queue_head+1)&0xFF;
	if (key_queue_size==256)
		key_queue_lost++;
	else
		key_queue_size++;
}

bool get_input_len( int* r)
{
	*r = key_queue_size;
	return true;
}

static const int key_map[]=
{
	 0,27,'!1','@2','#3','$4','%5','^6','&7','*8','(9',')0','_-','+=',8,
	   '\t','Qq','Ww','Ee','Rr','Tt','Yy','Uu','Ii','Oo','Pp','{[','}]',13,
	        0,'Aa','Ss','Dd','Ff','Gg','Hh','Jj','Kk','Ll',':;','\"\'',
   '~`',0,'|\\','Zz','Xx','Cc','Vv','Bb','Nn','Mm','<,','>.','?/',0,
                   0,0,' ',0, 0,0,0,0,0,0,0,0,0,0, 0,0,

				    // unescaped: keypad
					// 0xe0 escaped: grey keys
					KBD_HOM,KBD_UP,KBD_PUP,0,
					KBD_LT,0,KBD_RT,0,
					KBD_END,KBD_DN,KBD_PDN,
					KBD_INS,KBD_DEL
};

bool spec_read_input( CON_INPUT* ir, int n, int* r)
{
	static int caps=0;
	static int l_shift=0;
	static int r_shift=0;

	if (n>key_queue_size)
		n=key_queue_size;
	int from = key_queue_head + 256 - key_queue_size;
	for (int i=0; i<n; i++)
	{
		unsigned char c = key_queue[ (from+i)&0xFF ];

		ir[i].EventType = CON_INPUT_KBD;
		ir[i].Event.KeyEvent.bKeyDown = c<0x80;
		ir[i].Event.KeyEvent.uChar.AsciiChar=0;
		switch (c&0x7f)
		{
			case 0xe0: // escape code
				break;

			case 0x2a:
				l_shift = (~c>>7)&1;
				break;

			case 0x36:
				r_shift = (~c>>7)&1;
				break;

			case 0x3a:
				caps = (~c>>7)&1;
				break;

			default:
			{
				c&=0x7f;

				if (c*sizeof(int)<sizeof(key_map))
				{
					int s = (l_shift+r_shift) > 0;
					if ((key_map[c]&0xFF) >='a' && (key_map[c]&0xFF) <='z')
						s ^= caps;

					if (s && (key_map[c]&0xFF00))
						ir[i].Event.KeyEvent.uChar.AsciiChar = (key_map[c]>>8)&0xFF;
					else
						ir[i].Event.KeyEvent.uChar.AsciiChar = key_map[c]&0xFF;
				}
			}
		}
	}

	key_queue_size-=n;
	if (r)
		*r=n;

	bool ok = key_queue_lost==0;
	key_queue_lost=0;
	return ok;
}




unsigned char font_org[256*16];
unsigned char font_8[256*8]={0};

unsigned char font_14[256*14]=
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x7E,0x81,0xA5,0x81,0x81,0xBD,0x99,0x81,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x7E,0xFF,0xDB,0xFF,0xFF,0xC3,0xE7,0xFF,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x6C,0xEE,0xFE,0xFE,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10,0x38,0x7C,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10,0x38,0x10,0x6C,0xEE,0x6C,0x10,0x38,0x00,0x00,0x00,
	0x00,0x00,0x10,0x38,0x7C,0x7C,0xFE,0xFE,0x6C,0x10,0x38,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x18,0x3C,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xE7,0xC3,0xC3,0xE7,0xFF,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x00,0x00,0x18,0x3C,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00,
	0xFF,0xFF,0xFF,0xFF,0xE7,0xC3,0x99,0x99,0xC3,0xE7,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x1E,0x0E,0x1E,0x36,0x78,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,
	0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,
	0x00,0x00,0x1E,0x1A,0x1E,0x18,0x18,0x18,0x78,0xF8,0x70,0x00,0x00,0x00,
	0x00,0x3E,0x36,0x3E,0x36,0x36,0x76,0xF6,0x66,0x0E,0x1E,0x0C,0x00,0x00,
	0x00,0x18,0xDB,0x7E,0x3C,0x66,0x66,0x3C,0x7E,0xDB,0x18,0x00,0x00,0x00,
	0x00,0x00,0x80,0xE0,0xF0,0xFC,0xFE,0xFC,0xF0,0xE0,0x80,0x00,0x00,0x00,
	0x00,0x00,0x02,0x0E,0x3E,0x7E,0xFE,0x7E,0x3E,0x0E,0x02,0x00,0x00,0x00,
	0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,0x00,0x00,
	0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x66,0x66,0x00,0x00,0x00,
	0x00,0x00,0x7F,0xDB,0xDB,0xDB,0x7B,0x1B,0x1B,0x1B,0x1B,0x00,0x00,0x00,
	0x00,0x7C,0xC6,0xC6,0x60,0x7C,0xF6,0xDE,0x7C,0x0C,0xC6,0xC6,0x7C,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0xFE,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x7E,0x3C,0x18,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,
	0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x0C,0x0E,0xFF,0x0E,0x0C,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x30,0x70,0xFE,0x70,0x30,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xC0,0xC0,0xC0,0xFE,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x24,0x66,0xFF,0x66,0x24,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x10,0x38,0x38,0x38,0x7C,0x7C,0xFE,0xFE,0x00,0x00,0x00,0x00,
	0x00,0x00,0xFE,0xFE,0x7C,0x7C,0x7C,0x38,0x38,0x10,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,
	0x00,0x36,0x36,0x36,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,
	0x00,0x18,0x18,0x7C,0xC6,0xC0,0x78,0x3C,0x06,0xC6,0x7C,0x18,0x18,0x00,
	0x00,0x06,0x06,0xCC,0xCC,0x18,0x18,0x30,0x30,0x66,0x66,0xC0,0xC0,0x00,
	0x00,0x00,0x38,0x6C,0x38,0x38,0x76,0xF6,0xCE,0xCC,0x76,0x00,0x00,0x00,
	0x00,0x0C,0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,
	0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,
	0x00,0x00,0x00,0x00,0x6C,0x38,0xFE,0x38,0x6C,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x0C,0x18,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,
	0x00,0x06,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0xC0,0xC0,0x00,
	0x00,0x00,0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x18,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x00,0x00,0x00,
	0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00,0x0C,0x0C,0x0C,0x18,0x00,
	0x00,0x00,0x0C,0x18,0x30,0x60,0xC0,0x60,0x30,0x18,0x0C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x38,0x6C,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,
	0x00,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0xC0,0xC0,0x66,0x3C,0x00,0x00,0x00,
	0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,
	0x00,0x00,0xFE,0x66,0x60,0x60,0x7C,0x60,0x60,0x66,0xFE,0x00,0x00,0x00,
	0x00,0x00,0xFE,0x66,0x60,0x60,0x7C,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC0,0xC0,0xCE,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0xD8,0xD8,0x70,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xCC,0xD8,0xF0,0xF0,0xD8,0xCC,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xEE,0xFE,0xD6,0xD6,0xD6,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xE6,0xE6,0xF6,0xDE,0xCE,0xCE,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0x7C,0x06,0x00,0x00,
	0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC0,0x60,0x38,0x0C,0x06,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x7E,0x5A,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0xEE,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0xC6,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x00,0xFE,0xC6,0x8C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x7C,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7C,0x00,0x00,0x00,
	0x00,0xC0,0xC0,0x60,0x60,0x30,0x30,0x18,0x18,0x0C,0x0C,0x06,0x06,0x00,
	0x00,0x00,0x7C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x7C,0x00,0x00,0x00,
	0x00,0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,
	0x00,0x18,0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x00,0xE0,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0xCC,0xCC,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x1C,0x36,0x30,0x30,0xFC,0x30,0x30,0x30,0x78,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x76,0xCE,0xC6,0xC6,0x7E,0x06,0xC6,0x7C,0x00,
	0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,
	0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x00,0x0C,0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00,
	0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,
	0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x6C,0xFE,0xD6,0xD6,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00,
	0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00,
	0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x70,0x1C,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x30,0x30,0x30,0xFC,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,0x38,0x6C,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xCE,0x76,0x06,0xC6,0x7C,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFE,0x8C,0x18,0x30,0x62,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,
	0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,
	0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x70,0x00,0x00,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x10,0x38,0x38,0x6C,0x6C,0xFE,0x00,0x00,0x00,0x00,
	0x00,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0xC6,0x66,0x3C,0x18,0xCC,0x38,0x00,
	0x00,0x00,0xC6,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xCE,0x76,0x00,0x00,0x00,
	0x00,0x0C,0x18,0x30,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x30,0x78,0xCC,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x00,0xCC,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x60,0x30,0x18,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x38,0x6C,0x38,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC6,0x7C,0x18,0x6C,0x38,0x00,
	0x00,0x30,0x78,0xCC,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xCC,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x30,0x18,0x0C,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x66,0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x18,0x3C,0x66,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x60,0x30,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0xC6,0x00,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x00,0x00,
	0x38,0x6C,0x38,0x00,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,0x00,0x00,
	0x0C,0x18,0x30,0x00,0xFE,0x60,0x60,0x7C,0x60,0x60,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x66,0xDB,0x1B,0x7F,0xD8,0xDF,0x76,0x00,0x00,0x00,
	0x00,0x7E,0xD8,0xD8,0xD8,0xD8,0xFE,0xD8,0xD8,0xD8,0xDE,0x00,0x00,0x00,
	0x00,0x30,0x78,0xCC,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0xC6,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x30,0x18,0x0C,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x30,0x78,0xCC,0x00,0xC6,0xC6,0xC6,0xC6,0xCE,0x76,0x00,0x00,0x00,
	0x00,0x60,0x30,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xCE,0x76,0x00,0x00,0x00,
	0x00,0x00,0xC6,0x00,0x00,0xC6,0xC6,0xC6,0xCE,0x76,0x06,0xC6,0x7C,0x00,
	0x00,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0xC6,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x18,0x18,0x3C,0x66,0x60,0x60,0x66,0x3C,0x18,0x18,0x00,0x00,0x00,
	0x00,0x00,0x38,0x6C,0x60,0x60,0xF0,0x60,0x66,0xF6,0x6C,0x00,0x00,0x00,
	0x00,0x00,0x66,0x66,0x3C,0x18,0x7E,0x18,0x3C,0x18,0x18,0x00,0x00,0x00,
	0x00,0xFC,0xC6,0xFC,0xC0,0xCC,0xDE,0xCC,0xCC,0xCC,0xC6,0x00,0x00,0x00,
	0x00,0x0E,0x1B,0x18,0x18,0x18,0x7E,0x18,0x18,0x18,0x18,0xD8,0x70,0x00,
	0x00,0x0C,0x18,0x30,0x00,0x78,0x0C,0x7C,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x0C,0x18,0x30,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,
	0x00,0x0C,0x18,0x30,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x18,0x30,0x60,0x00,0xCC,0xCC,0xCC,0xCC,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x00,0x76,0xDC,0x00,0xBC,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,
	0x00,0x76,0xDC,0x00,0xC6,0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0x00,0x00,0x00,
	0x00,0x3C,0x6C,0x6C,0x3E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x38,0x6C,0x6C,0x38,0x00,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x60,0xC6,0xC6,0x7C,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x06,0x06,0x06,0x00,0x00,0x00,0x00,
	0x00,0x60,0x62,0x66,0x6C,0x18,0x30,0x60,0xDC,0x36,0x0C,0x18,0x3E,0x00,
	0x00,0x60,0x62,0x66,0x6C,0x18,0x36,0x6E,0xDE,0x36,0x7E,0x06,0x06,0x00,
	0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x3C,0x3C,0x3C,0x18,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x36,0x6C,0xD8,0x6C,0x36,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0xD8,0x6C,0x36,0x6C,0xD8,0x00,0x00,0x00,0x00,0x00,
	0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,
	0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,
	0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x36,0x36,0x36,0x36,0x36,0x36,
	0x00,0x00,0x00,0x00,0x00,0xF8,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,
	0x36,0x36,0x36,0x36,0x36,0xF6,0x06,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x00,0x00,0x00,0x00,0x00,0xFE,0x06,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0xF6,0x06,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,
	0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x37,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x37,0x30,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x3F,0x30,0x37,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0xF7,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xF7,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x37,0x30,0x37,0x36,0x36,0x36,0x36,0x36,0x36,
	0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x36,0x36,0x36,0x36,0x36,0xF7,0x00,0xF7,0x36,0x36,0x36,0x36,0x36,0x36,
	0x18,0x18,0x18,0x18,0x18,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,
	0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x1F,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFF,0x36,0x36,0x36,0x36,0x36,0x36,
	0x18,0x18,0x18,0x18,0x18,0xFF,0x18,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
	0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x76,0xDC,0xD8,0xD8,0xD8,0xDC,0x76,0x00,0x00,0x00,
	0x00,0x00,0x00,0x78,0xCC,0xD8,0xFC,0xC6,0xC6,0xC6,0xCC,0x00,0x00,0x00,
	0x00,0x00,0xFE,0x66,0x62,0x60,0x60,0x60,0x60,0x60,0x60,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFE,0x6C,0x6C,0x6C,0x6C,0x6C,0x00,0x00,0x00,
	0x00,0x00,0xFE,0xC6,0x62,0x30,0x18,0x30,0x62,0xC6,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7E,0xD8,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x7C,0x60,0xC0,0x80,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x76,0xDC,0x18,0x18,0x18,0x18,0x00,0x00,0x00,
	0x00,0x00,0xFE,0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0xFE,0x00,0x00,0x00,
	0x00,0x00,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,
	0x00,0x00,0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x6C,0x6C,0xEE,0x00,0x00,0x00,
	0x00,0x00,0x3E,0x60,0x30,0x3C,0x66,0xC6,0xC6,0xCC,0x78,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x7E,0xDB,0xDB,0x7E,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x06,0x0C,0x7C,0xDE,0xF6,0xE6,0x7C,0x60,0xC0,0x00,0x00,0x00,
	0x00,0x00,0x1C,0x30,0x60,0x60,0x7C,0x60,0x60,0x30,0x1C,0x00,0x00,0x00,
	0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0xFE,0x00,0xFE,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00,0x7E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x0C,0x1E,0x1A,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
	0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x58,0x78,0x30,0x00,0x00,
	0x00,0x00,0x00,0x18,0x18,0x00,0x7E,0x00,0x18,0x18,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x76,0xDC,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,
	0x00,0x00,0x78,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x1F,0x18,0x18,0x18,0x18,0xD8,0x78,0x38,0x18,0x00,0x00,
	0x00,0xD8,0x6C,0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x70,0xD8,0x30,0x60,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

unsigned char font_16[256*16]=
{
	0
};

#pragma  pack ( push, 1 )

typedef unsigned char  Byte;
typedef unsigned short Word;
typedef unsigned long  Dword;

typedef struct
{
  Byte x, y;		// (0, 0) is top-left corner
} Cursor;

typedef struct
{
  Dword  static_tbl;
  Byte	 mode;		// Current video mode
  Word	 cols;		// Number of columns on screen
  Word	 size;		// Size of video page buffer
  Word	 offset;	// Starting offset of video buffer
  Cursor curs[8];       // Cursor position in each of eight pages
  Word	 cursor_line;	// Starting and ending line of cursor
  Byte	 page;		// Currently active display page
  Word	 port;		// Port address of active display
  Byte   reg3x8;
  Byte   reg3x9;
  Byte	 rows;		// Number of rows on screen
  Word	 font;		// Font height
  Byte   displ_comb;
  Byte   dcc;
  Word   colors;
  Byte   pages;
  Byte   scanlines; // 1,2,3,4 : 200,350,400,480
  Byte   primary_char_block;
  Byte   secondary_char_block;
  Byte   misc_flags;
  Byte   reserved[3];
  Byte   vid_mem; // 0,1,2,3,4 : 64k,128k,192k,256k
  Byte   save_ptr;
  Byte   reserved2[13];
} video_BIOS;

struct RMI {
	Dword EDI;
	Dword ESI;
	Dword EBP;
	Dword reserved_by_system;
	Dword EBX;
	Dword EDX;
	Dword ECX;
	Dword EAX;
	Word flags;
	Word ES,DS,FS,GS,IP,CS,SP,SS,padding;
};

#pragma  pack ( pop )

static void AllocateDosMemory(Dword NumPages,Word* Segment,Word* Selector)
// Pass this the number of 16 byte pages you want from DOS memory
// This is used to hold vesainfo and vesamodeinfo in DOS memory space
{
	Word TempSeg,TempSel;

	_asm {
		mov eax, 0x100
		mov ebx, NumPages
		int 0x31  // Call DPMI Function 0x100 to allocate a block
		mov TempSeg,ax
		mov TempSel,dx
	}
	*Segment = TempSeg;
	*Selector = TempSel;
}

/////////////////////////////////////////////////////////////////////////////
// DeallocateDosMemory(word Selector): Deallocates pages in <1M memory space
static void DeallocateDosMemory(Word Selector)
{
	_asm {
		mov eax,0x101
		mov dx,Selector
		int 0x31 // Call DPMI Function 0x101 to deallocate a block
	}
}

void terminal_done()
{
	stop_fastclock();
	_dos_setvect (Keyboard_Int, old_keyboard);
	
	switch (screen_font)
	{
		case 14:
			LoadFont(0,font_org,14);
			break;
	}
}

int terminal_init(int argc, char* argv[], int* dw, int* dh)
{
	LoadConf();

	int args = 0;

	Word seg,sel;
	video_BIOS* video;
		
	union REGS regs={0};
	struct SREGS sregs={0}; 
	struct RMI rmi={0};
	
	AllocateDosMemory((sizeof(video_BIOS)+15)>>4,&seg,&sel);
	video = (video_BIOS*)(seg<<4);
	
	rmi.EAX = 0x1B00;
	rmi.ES = seg;
	
	regs.w.bx = 0x10;
	regs.x.eax = 0x300;
	regs.x.ecx = 0;
    sregs.es = FP_SEG(&rmi);
    regs.x.edi = FP_OFF(&rmi);	
	
	int386x(0x31,&regs,&regs,&sregs);
	
	screen_w = video->cols;
	screen_h = video->rows;
	screen_font = video->font;
	screen_lines = video->scanlines;
	
	DeallocateDosMemory(sel);
	
	printf("cols:%d, rows:%d, font:%d, lines:%d\n",screen_w,screen_h,screen_font,screen_lines);

	*dw=screen_w;
	*dh=screen_h;

	// disable blink on bright bkgnd
    __asm
    {
            xor eax,eax
            mov ah,10h
            mov al,03h
            mov bl,0h
            int 10h
    }
	
	switch (screen_font)
	{
		case 14:
			LoadFont(font_org,font_14,14);
			break;
	}

	// put cursor outa screen

	int position = screen_w*screen_h;

	outp(0x3D4,0x0F);
	outp(0x3D5,position & 0xFF);
	outp(0x3D4,0x0E);
	outp(0x3D5,(position>>8) & 0xFF);	

	start_fastclock();
    old_keyboard = _dos_getvect (Keyboard_Int);
    _dos_setvect (Keyboard_Int, keyboard_handler);

	return args;
}

void vsync_wait()
{
  /* wait until any previous retrace has ended */
  do {
  } while (inp(0x3DA) & 8);

  /* wait until a new retrace has just begun */
  do {
  } while (!(inp(0x3DA) & 8));
}

void free_con_output(CON_OUTPUT* screen)
{
	screen->arr = 0;
}

int screen_write(CON_OUTPUT* screen, int dw, int dh, int sx, int sy, int sw, int sh)
{
	int w = screen->w;
	int h = screen->h;
	char* buf = screen->buf;
	char* color = screen->color;

	unsigned char pal_conv[16]=
	{
		0,4,2,6,1,5,3,7,
		8,12,10,14,9,13,11,15
	};

	if (!color)
	{
		unsigned short* ptr=(unsigned short*)(0xB8000);
		for (int y=0; y<sh; y++)
		{
			for (int x=0; x<sw; x++)
			{
				ptr[dw*(dy+y)+dx+x] = buf[(w+1)*(y+sy)+x+sx] | 0xF000;
			}
		}

		return sw*sh;
	}

	int size=screen_w*screen_h;
	unsigned short* ptr=(unsigned short*)(0xB8000);
	for (int y=0,i=0; y<sh; y++)
	{
		for (int x=0; x<sw; x++)
		{
			int fg = pal_conv[ color[(w+1)*(y+sy)+x+sx] & 0xF ];
			int bg = pal_conv[ (color[(w+1)*(y+sy)+x+sx]>>4) & 0xF ];

			unsigned short attr = (unsigned char)(fg | (bg<<4));
			ptr[dw*(dy+y)+dx+x] = buf[(w+1)*(y+sy)+x+sx] | (attr<<8);
		}
	}

	return 2*sw*sh;
}

void terminal_loop()
{
	while (modal)
	{
		if (modal->Run()==-2)
			break;
	}
}

void write_fs()
{
}

const char* conf_path()
{
	return ".\\asciipat.cfg";
}

const char* record_path()
{
	return ".\\asciipat.rec";
}

const char* shot_path()
{
	return ".\\";
}



static HISCORE hiscore_async = {0,0,0,"",""};
static bool hiscore_ready = false;

void request_hiscore()
{
	HISCORE tmp={0,0,0,"",""};

	int err=0;

	if (!err)
	{
		FILE* f = 0;
		fopen_s(&f,".\\asciipat.rnk","rb");
		if (f)
		{
			char line[56];
			if (fgets(line,56,f))
			{
				if ( sscanf_s(line,"%d/%d",&tmp.ofs,&tmp.tot) != 2 )
				{
					fclose(f);
					err = -2;
				}
				else
				{
					char* id = strchr(line,' ');
					if (id)
					{
						id++;
						char* id_end = strchr(id,' ');
						int id_len = id_end-id;
						if (id_len>15)
							id_len=15;
						memcpy(tmp.id,id,id_len);
						tmp.id[id_len]=0;
					}
				}
			}

			if (!err)
			{
				tmp.siz = 0;
				while (fgets(line,56,f))
				{
					/*
					// no need to put terminator before avatar
					line[45]=0;
					*/
					memcpy(tmp.buf + 55*tmp.siz, line, 55);
					tmp.siz++;
				}

				// change last cr to eof
				tmp.buf[55 * tmp.siz -1] = 0; 
			}

			fclose(f);
		}
		else
			err = -3;
	}
	else
		err = -1;

	if (!err)
	{
		// prevent overwriting id if no new id is returned
		if (hiscore_async.id[0] && !tmp.id[0])
			memcpy(tmp.id,hiscore_async.id,16);

		hiscore_async = tmp;
		hiscore_ready = true;
	}
}

void post_hiscore()
{
	int ret = spawnlp(P_WAIT,"curl","curl","--silent","-o",".\\asciipat.rec","-F","rec=@.\\asciipat.rec","http://ascii-patrol.com/rank.php",0);
	if (ret==0)
		request_hiscore();
}

void get_hiscore(int ofs, const char* id)
{
	char uri[256];

	if (id && id[0])
		sprintf_s(uri,256,"http://ascii-patrol.com/rank.php?rank=%d&id=%s", ofs+1, id);
	else
		sprintf_s(uri,256,"http://ascii-patrol.com/rank.php?rank=%d", ofs+1);

	int ret = spawnlp(P_WAIT,"curl","curl","--silent","-o",".\\asciipat.rec",uri,0);
	if (ret==0)
		request_hiscore();
}

bool update_hiscore()
{
	if (hiscore_ready)
	{
		hiscore_ready = false;

		// prevent overwriting id if no new id is returned
		if (hiscore.id[0]!=0 && hiscore_async.id[0]==0)
			memcpy(hiscore_async.id,hiscore.id,16);

		hiscore = hiscore_async;
	}

	return true;
}

void app_exit()
{
	terminal_done();
	_exit(0);
}


void init_sound()
{
	// normaly, in case of audio device presence,
	// load_player() should be called here
}

void set_gain(int mo3, int sfx)
{
}

void lock_player()
{
}

void unlock_player()
{
}

bool play_sfx(unsigned int id, void** voice, bool loop, int vol, int pan)
{
	return false;
}

bool stop_sfx(int fade) // stop all sfx
{
	return false;
}

bool stop_sfx(void* voice, int fade) // returns false if given voice is already stopped
{
	return false;
}

bool set_sfx_params(void* voice, int vol, int pan)
{
	return false;
}


#endif

