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
#include "solboard.h"
#include "colors.h"
#include "pig.h"
#include "fx2math.h"

#define SOLBOARD_DEBUG

extern	int	doexit;
extern	int	debug;
extern	unsigned short	actcode;
extern	unsigned short	realcode;
#define	RC_0			0
#define	RC_1			1
#define	RC_2			2
#define	RC_3			3
#define	RC_4			4
#define	RC_5			5
#define	RC_6			6
#define	RC_7			7
#define	RC_8			8
#define	RC_9			9

/**
 * Sets framebuffer colors according to definitions in colors.h
 */
static	void	setup_colors( void )
{
	FBSetColor( YELLOW, 255, 255, 30 );
	FBSetColor( GREEN, 30, 255, 30 );
	FBSetColor( STEELBLUE, 30, 30, 180 );
	FBSetColor( BLUE, 130, 130, 255 );
	FBSetColor( GRAY, 130, 130, 130 );
	FBSetColor( DARK, 30, 30, 30 );
	FBSetColor( ORANGE, 255, 199, 15);

	FBSetupColors( );
}

/**
 * The main loop
 * @param fdfb 1 if framebuffer should be used, 0 otherwise
 * @param fdrc 1 if remote control should be used, 0 otherwise
 * @param fdlcd 1 if lcd should be used, 0 otherwise
 * @param cfgfile the config file
 * @return 0
 */
int main( )
{
	struct timeval tv;
	int x;
	int fdfb = -1, fdrc = -1;

	if ( FBInitialize( 720, 576, 8, fdfb ) < 0 )
		return -1;

	setup_colors();
	FBFillRect( 0, 0, 1400, 800, BNR0 );

	if ( RcInitialize( fdrc ) < 0 )
		return -1;
	// 228, 187
//	Fx2ShowPig( 420, 300, 228, 187 );

	while( doexit != 3 )
	{
#ifdef SOLBOARD_DEBUG
	  printf("somain: board init\n");
#endif
		BoardInitialize();
		DrawBoard();

		doexit=0;
		while( !doexit )
		{
		  tv.tv_sec = 0;
		  tv.tv_usec = 10000;
		  x = select( 0, 0, 0, 0, &tv );		/* 100ms pause */
		  
		  RcGetActCode( );
		  MoveMouse();
#ifdef USEX
		  FBFlushGrafic();
#endif
		}
		
		if ( doexit != 3 )
		  {
#ifdef SOLBOARD_DEBUG
		    printf("somain: actcode=0xee\n");
#endif
		    
		    actcode=0xee;
		    if ( doexit ==2 ) {
#ifdef SOLBOARD_DEBUG
		      printf("somain: before DrawScore\n");
#endif
		      DrawScore();
		    }
		    else {
#ifdef SOLBOARD_DEBUG
		      printf("somain: //DrawGameOver()\n");
#endif
		      
				//DrawGameOver();
		    }
#ifdef USEX
#ifdef SOLBOARD_DEBUG
		    printf("somain: FBFlushGrafic()\n");
#endif
		    
		    FBFlushGrafic();
#endif
		    doexit=0;
		    while(( actcode != RC_GREEN ) && !doexit )
		      {
#ifdef SOLBOARD_DEBUG
			printf("somain: RC_loop\n");
#endif
			
			tv.tv_sec = 0;
				tv.tv_usec = 100000;
				x = select( 0, 0, 0, 0, &tv );		/* 100ms pause */
				RcGetActCode( );
			}
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

