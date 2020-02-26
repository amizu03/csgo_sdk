#include "movement.hpp"
#include "../globals.hpp"

void features::movement::run( ucmd_t* ucmd ) {
	if ( !g::local->valid( )
		|| g::local->movetype( ) == movetypes::movetype_noclip
		|| g::local->movetype( ) == movetypes::movetype_ladder )
		return;

	ucmd->m_buttons = ( g::local && !( g::local->flags( ) & 1 ) ) ? ucmd->m_buttons & ~2 : ucmd->m_buttons;
}