/*
** initial coding by fx2
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include "rcinput.h"
#include "draw.h"
#include "board.h"
#include "colors.h"
#include "pig.h"
#include "fx2math.h"

extern	int	doexit;
extern	int	debug;
extern	unsigned short	actcode;
extern	unsigned short	realcode;

static	void	setup_colors( void )
{
	FBSetColor( YELLOW, 255, 255, 30 );
	FBSetColor( GREEN, 30, 255, 30 );
	FBSetColor( STEELBLUE, 20, 20, 180 );
	FBSetColor( BLUE, 130, 130, 255 );
	FBSetColor( GRAY, 130, 130, 130 );
	FBSetColor( DARK, 30, 30, 30 );
	FBSetColor( MAGENTA, 220, 30, 220 );
	FBSetColor( CYAN, 30, 220, 220 );
	FBSetColor( BROWN, 139, 69, 19 );
	FBSetColor( AIR, 30, 30, 180 );
	FBSetColor( DACH, 0xb2, 17, 17 );
	FBSetColor( ORANGE, 255, 180, 10 );
	FBSetColor( SAIR, 40, 40, 240 );

	FBSetupColors( );
}

int main( )
{
	struct timeval	tv;
	int				x;
	int fdfb = -1, fdrc = -1;

	if ( FBInitialize( 720, 576, 8, fdfb ) < 0 )
		return -1;

	setup_colors();
	FBFillRect( 0, 0, 1400, 800, BNR0 );

	if ( RcInitialize( fdrc ) < 0 )
		return -1;

	while( doexit != 3 )
	{
		TankInitialize();

#ifdef USEX
		FBFlushGrafic();
#endif

		doexit=0;
		while( !doexit )
		{
			tv.tv_sec = 0;
			tv.tv_usec = 200000;
			select( 0, 0, 0, 0, &tv );

			actcode=0xee;
			RcGetActCode( );
			Play();
#ifdef USEX
			FBFlushGrafic();
#endif
			while( realcode != 0xee )
				RcGetActCode( );
		}
		if ( doexit == 4 )		// level changed
		{
			doexit=0;
			actcode=0xee;
			while(( actcode != RC_OK ) && !doexit )
			{
				tv.tv_sec = 0;
				tv.tv_usec = 200000;
				select( 0, 0, 0, 0, &tv );

				RcGetActCode( );
			}
			while( realcode != 0xee )
				RcGetActCode( );
			actcode=0xee;
		}
	}

/* fx2 */
/* buffer leeren, damit neutrino nicht rumspinnt */
	realcode = RC_0;
	while( realcode != 0xee )
	{
		tv.tv_sec = 0;
		tv.tv_usec = 300000;
		x = select( 0, 0, 0, 0, &tv );		/* 300ms pause */
		RcGetActCode( );
	}

	RcClose();
	FBClose();

	return 0;
}

