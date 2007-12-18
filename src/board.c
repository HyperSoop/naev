/*
 * See Licensing and Copyright notice in naev.h
 */


#include "board.h"

#include "naev.h"
#include "pilot.h"
#include "player.h"
#include "toolkit.h"
#include "space.h"
#include "rng.h"
#include "economy.h"


#define BOARDING_WIDTH  300
#define BOARDING_HEIGHT 200


/*
 * externs
 */
extern unsigned int player_target;


static unsigned int board_credits = 0; /* money on the ship */
static unsigned int board_wid = 0;


/*
 * prototypes
 */
static void board_exit( char* str );
static void board_stealCreds( char* str );
static void board_fail (void);
static void board_update (void);


/*
 * attempt to board the player's target
 */
void player_board (void)
{  
	Pilot *p;
	char str[128];
	char cred[10];

	if (player_target==PLAYER_ID) {
		player_message("You need a target to board first!");
		return;
	}

	p = pilot_get(player_target);

	if (!pilot_isDisabled(p)) {
		player_message("You cannot board a ship that isn't disabled!");
		return;
	}
	else if (vect_dist(&player->solid->pos,&p->solid->pos) >
			p->ship->gfx_space->sw * PILOT_SIZE_APROX) {
		player_message("You are too far away to board your target");
		return;
	}
	else if ((pow2(VX(player->solid->vel)-VX(p->solid->vel)) +
				pow2(VY(player->solid->vel)-VY(p->solid->vel))) >
			(double)pow2(MAX_HYPERSPACE_VEL)) {
		player_message("You are going to fast to board the ship");
		return;
	}
	else if (pilot_isFlag(p,PILOT_BOARDED)) {
		player_message("Your target cannot be boarded again");
		return;
	};

	/* pilot will be boarded */
	pilot_setFlag(p,PILOT_BOARDED); 
	player_message("Boarding ship %s", p->name);

	/* calculate credits based on ship price */
	board_credits = RNG(20*p->ship->price, 50*p->ship->price)/1000;

	/*
	 * create the boarding window
	 */
	board_wid = window_create( "Boarding", -1, -1, BOARDING_WIDTH, BOARDING_HEIGHT );

	window_addText( board_wid, 20, -30, 120, 60,
			0, "txtCargo", &gl_smallFont, &cDConsole,
			"Credits:\n"
			"Cargo:\n"
			);
	credits2str( cred, board_credits, 2 );

	snprintf( str, 128,
			"%s\n"
			"%s\n"
			, cred, "none" );
	window_addText( board_wid, 80, -30, 120, 60,
			0, "txtData", &gl_smallFont, &cBlack, str );

	window_addButton( board_wid, 20, 20, 50, 30, "btnStealCredits", "Credits", board_stealCreds);

	window_addButton( board_wid, -20, 20, 50, 30, "btnBoardingClose", "Leave", board_exit );
}
static void board_exit( char* str )
{
	(void)str;
	window_destroy( window_get("Boarding") );
}


static void board_stealCreds( char* str )
{
	(void)str;
	Pilot* p;

	p = pilot_get(player_target);

	if (board_credits==0) { /* you can't steal from the poor */
		player_message("The ship has no credits left");
		return;
	}

	/* calculate success */
	if (RNG(0,100) < (int)(50. * (double)p->ship->crew/(double)player->ship->crew)) {
		board_fail();
		return;
	}

	player_credits += board_credits;
	board_credits = 0;
	board_update(); /* update the lack of credits */
	player_message("You manage to steal the ship's credits");
}


/*
 * failed to board
 */
static void board_fail (void)
{
	Pilot* p;

	if (RNG(0,2)==0) { /* 33% of instadeath */
		p = pilot_get(player_target);
		p->armour = -1.;
		player_message("You have tripped the ship's self destruct mechanism");
	}
	else /* you just got locked out */
		player_message("The ship's security system locks you out");

	board_exit(NULL);
}


/*
 * updates the cargo and credit fields
 */
static void board_update (void)
{
	char str[128];
	char cred[10];

	credits2str( cred, board_credits, 2 );

	snprintf( str, 128,
			"%s\n"
			"%s\n"
			, cred, "none" );

	window_modifyText( board_wid, "txtData", str );	
}
