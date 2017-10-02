/*
** initial coding by fx2
*/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

#include "draw.h"
#include "rcinput.h"

// #include <config.h>
#define HAVE_DREAMBOX_HARDWARE 1
#if defined HAVE_DREAMBOX_HARDWARE || defined HAVE_IPBOX_HARDWARE
	static int fd_is_ext = 0;
	static int keyboard = 0;
	static int drop = 0;
#endif

#define Debug	if (debug) printf

static	int	fd = -1;
static	int	kbfd = -1;
unsigned short	realcode=0xee;
unsigned short	actcode=0xee;
int		doexit=0;
int		debug=0;

static	struct termios	tios;

void	KbInitialize( void )
{
	struct termios	ntios;

	kbfd = 0;

	if ( tcgetattr(kbfd,&tios) == -1 )
	{
		kbfd=-1;
		return;
	}
	memset(&ntios,0,sizeof(ntios));
	tcsetattr(kbfd,TCSANOW,&ntios);

	return;
}
static unsigned short translate( unsigned short code )
{
	int rccode=-1;

	switch(code)
	{
	case KEY_UP:
		rccode = RC_UP;
		break;
	case KEY_DOWN:
		rccode = RC_DOWN;
		break;
	case KEY_LEFT:
		rccode = RC_LEFT;
		break;
	case KEY_RIGHT:
		rccode = RC_RIGHT;
		break;
	case KEY_OK:
		rccode = RC_OK;
		break;
	case KEY_RED:
		rccode = RC_RED;
		break;
	case KEY_GREEN:
		rccode = RC_GREEN;
		break;
	case KEY_YELLOW:
		rccode = RC_YELLOW;
		break;
	case KEY_BLUE:
		rccode = RC_BLUE;
		break;
	case KEY_INFO:
		rccode = RC_HELP;
		break;
	case KEY_MENU:
		rccode = RC_DBOX;
		break;
	case KEY_EXIT:
		rccode = RC_HOME;
		break;
	case KEY_POWER:
		rccode = RC_STANDBY;
		break;
	default:
		if( code > 0x7F )
		{
			rccode = 0;
			if( code == 0x110 )
			{
				rccode = RC_ON;
			}
		}
		else
		{
			int rctable[] =
			{
				0x00, RC_ESC, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 'ß', '´', RC_BS, 0x09,
				'q',  'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 'ü', '+', RC_RET, RC_STRG, 'a', 's',
				'd',  'f', 'g', 'h', 'j', 'k', 'l', 'ö', 'ä', '^', RC_LSHIFT, '#', 'y', 'x', 'c', 'v',
				'b',  'n', 'm', ',', '.', '-', RC_RSHIFT, 0x00, RC_ALT, 0x20, RC_CAPSLOCK,RC_F1,RC_F2,RC_F3,RC_F4,RC_F5,
				RC_F6,RC_F7,RC_F8,RC_F9,RC_F10,RC_NUM,RC_ROLLEN,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, RC_STANDBY, 0x00, 0x00, 0x00, 0x00, '<', RC_OK, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, RC_ALTGR, 0x00, RC_POS1, RC_UP, RC_PAGEUP, RC_LEFT, RC_RIGHT, RC_END, RC_DOWN,RC_PAGEDOWN,RC_INS,RC_ENTF,
				0x00, RC_MUTE, RC_MINUS, RC_PLUS, RC_STANDBY, 0x00, 0x00, RC_PAUSE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
			};

			rccode = rctable[code & 0x7F];
		}
	}

	return rccode;

}

int	RcInitialize( int extfd )
{
	char	buf[32];
	if ( extfd == -1 )
	{
		fd_is_ext = 0;
		fd = open("/dev/input/nevis_ir", O_RDONLY );
		if ( fd == -1 )
			return kbfd;
		fcntl(fd, F_SETFL, O_NONBLOCK );
	}
	else
	{
		fd_is_ext = 1;
		fd = extfd;
		fcntl(fd, F_SETFL, O_NONBLOCK );
	}
/* clear rc-buffer */
	read( fd, buf, 32 );
	return 0;
}


void RcGetActCode( void )
{
	int				x=0;
	unsigned short	code = 0;
	static  unsigned short cw=0;
	struct input_event ev;

	if ( fd != -1 ) {

		do {

			x = read(fd, &ev, sizeof(struct input_event));

			if ((x == sizeof(struct input_event)) && ((ev.value == 1)||(ev.value == 2)))
				break;

		} while (x == sizeof(struct input_event));

	}

	if ( x % sizeof(struct input_event) )
	{
		//KbGetActCode();
		realcode=0xee;
		return;
	}

	Debug("%d bytes from FB received ...\n",x);
	Debug("ev.code=%04x\n",ev.code);

	code=translate(ev.code);
	realcode=code;
	if ( code == 0xee )
	{
		drop = 0;
		return;
	}

	Debug("code=%04x\n",code);

	if ( cw == 2 )
	{
		actcode=code;
		return;
	}

	switch(code)
	{
#if 0
	case RC_HELP:
		if ( !cw )
			FBPrintScreen();
		cw=1;
		break;
#endif
	case RC_MUTE:
		if ( !cw )
		{
			cw=2;
			FBPause();
			cw=0;
		}
		break;
	case RC_HOME:
		doexit=3;
		break;
#if 0
	case RC_UP:
	case RC_DOWN:
	case RC_RIGHT:
	case RC_LEFT:
	case RC_OK:
#endif
	default :
		cw=0;
		actcode=code;
		break;
	}
	Debug("actcode=%04x\n",actcode);

	return;
}

void	RcClose( void )
{
	if ( fd == -1 )
		return;
	if ( !fd_is_ext )
	close(fd);
}
