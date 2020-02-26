#include <intrin.h>
#include "hooks.hpp"
#include "globals.hpp"
#include "minhook/minhook.h"
#include "renderer/d3d9.hpp"

/* features */
#include "features/movement.hpp"
#include "features/prediction.hpp"

bool __fastcall fire_event_hk( REG );
void __fastcall override_view_hk( REG, void* setup );
void __fastcall drawmodelexecute_hk( REG, void* ctx, void* state, const mdlrender_info_t& info, matrix3x4_t* bone_to_world );
bool __fastcall createmove_hk( REG, float sampletime, ucmd_t* ucmd );
void __fastcall framestagenotify_hk( REG, int stage );
long __fastcall endscene_hk( REG, IDirect3DDevice9* device );
long __fastcall reset_hk( REG, IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentation_params );
void __fastcall lockcursor_hk( REG );
void __fastcall doextraboneprocessing_hk( REG, int a2, int a3, int a4, int a5, int a6, int a7 );
vec3_t* __fastcall get_eye_angles_hk( REG );
bool __fastcall get_bool_hk( REG );
long __stdcall wndproc( HWND hwnd, std::uint32_t msg, std::uintptr_t wparam, std::uint32_t lparam );

decltype( &fire_event_hk ) fire_event = nullptr;
decltype( &override_view_hk ) override_view = nullptr;
decltype( &get_bool_hk ) get_bool = nullptr;
decltype( &createmove_hk ) createmove = nullptr;
decltype( &framestagenotify_hk ) framestagenotify = nullptr;
decltype( &endscene_hk ) endscene = nullptr;
decltype( &reset_hk ) reset = nullptr;
decltype( &lockcursor_hk ) lockcursor = nullptr;
decltype( &drawmodelexecute_hk ) drawmodelexecute = nullptr;
decltype( &doextraboneprocessing_hk ) doextraboneprocessing = nullptr;
decltype( &get_eye_angles_hk ) get_eye_angles = nullptr;

/* fix event delays */
bool __fastcall fire_event_hk( REG ) {
	auto local = csgo::i::ent_list->get< player_t* >( csgo::i::engine->get_local_player( ) );

	if ( !local->valid( ) )
		return fire_event( REG_OUT );

	struct event_t {
	public:
		PAD( 4 );
		float delay;
		PAD( 48 );
		event_t* next;
	};

	auto ei = *( event_t** ) ( std::uintptr_t( csgo::i::client_state ) + 0x4E64 );

	event_t* next = nullptr;

	if ( !ei )
		return fire_event( REG_OUT );

	do {
		next = ei->next;
		ei->delay = 0.f;
		ei = next;
	} while ( next );

	return fire_event( REG_OUT );
}

void __fastcall override_view_hk( REG, void* setup ) {
	auto get_ideal_dist = [ & ] ( float ideal_distance ) {
		vec3_t inverse;
		csgo::i::engine->get_viewangles( inverse );

		inverse.x *= -1.0f, inverse.y += 180.0f;

		vec3_t direction = csgo::angle_vec( inverse );

		ray_t ray;
		trace_t trace;
		trace_filter_t filter( g::local );

		csgo::util_traceline( g::local->eyes( ), g::local->eyes( ) + ( direction * ideal_distance ), 0x600400B, g::local, &trace );

		return ( ideal_distance * trace.m_fraction ) - 10.0f;
	};

	if ( GetKeyState( VK_MBUTTON ) && g::local->valid( ) ) {
		vec3_t ang;
		csgo::i::engine->get_viewangles( ang );
		csgo::i::input->m_camera_in_thirdperson = true;
		csgo::i::input->m_camera_offset = vec3_t( ang.x, ang.y, get_ideal_dist( 150.0f ) );
	}
	else {
		csgo::i::input->m_camera_in_thirdperson = false;
	}

	override_view( REG_OUT, setup );
}

void __fastcall drawmodelexecute_hk( REG, void* ctx, void* state, const mdlrender_info_t& info, matrix3x4_t* bone_to_world ) {
	drawmodelexecute( REG_OUT, ctx, state, info, bone_to_world );
}

bool __fastcall createmove_hk( REG, float sampletime, ucmd_t* ucmd ) {
	if ( !ucmd || !ucmd->m_cmdnum )
		return false;

	auto ret = createmove( REG_OUT, sampletime, ucmd );

	g::ucmd = ucmd;

	features::movement::run( ucmd );

	const auto backup_angle = ucmd->m_angs;
	const auto backup_sidemove = ucmd->m_smove;
	const auto backup_forwardmove = ucmd->m_fmove;

	features::prediction::run( [ & ] ( ) {
		/* prediction-dependent code goes in here. */
	} );

	csgo::clamp( ucmd->m_angs );

	csgo::rotate_movement( ucmd );

	*( bool* ) ( *( uintptr_t* ) ( uintptr_t( _AddressOfReturnAddress( ) ) - 4 ) - 28 ) = g::send_packet;

	return false;
}

void __fastcall framestagenotify_hk( REG, int stage ) {
	g::local = ( !csgo::i::engine->is_connected( ) || !csgo::i::engine->is_in_game( ) ) ? nullptr : csgo::i::ent_list->get< player_t* >( csgo::i::engine->get_local_player( ) );

	framestagenotify( REG_OUT, stage );
}

long __fastcall endscene_hk( REG, IDirect3DDevice9* device ) {
	static auto ret = _ReturnAddress( );

	if ( ret != _ReturnAddress( ) )
		return endscene( REG_OUT, device );

	IDirect3DStateBlock9* pixel_state = nullptr;
	IDirect3DVertexDeclaration9* vertex_decleration = nullptr;
	IDirect3DVertexShader9* vertex_shader = nullptr;

	device->CreateStateBlock( D3DSBT_PIXELSTATE, &pixel_state );
	device->GetVertexDeclaration( &vertex_decleration );
	device->GetVertexShader( &vertex_shader );

	const auto phase = std::sinf( csgo::i::globals->m_curtime * ( 2.0f * csgo::pi ) ) / ( 2.0f * csgo::pi ) * 55.0f + 200.0f;

	render::text( 20 + 1, 20 + 1, rgba( 0, 0, 0, 255 ), fonts::def, L"你好" );
	render::text( 20, 20, rgba( phase, phase, phase, 255 ), fonts::def, L"你好" );

	pixel_state->Apply( );
	pixel_state->Release( );

	device->SetVertexDeclaration( vertex_decleration );
	device->SetVertexShader( vertex_shader );

	return endscene( REG_OUT, device );
}

long __fastcall reset_hk( REG, IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentation_params ) {
	reinterpret_cast< ID3DXFont* >( fonts::def )->Release( );

	auto hr = reset( REG_OUT, device, presentation_params );

	if ( SUCCEEDED( hr ) )
		render::create_font( ( void** ) &fonts::def, L"Tahoma", 32, true );

	return hr;
}

void __fastcall lockcursor_hk( REG ) {
	if ( /* menu::open( ) */ false ) {
		csgo::i::surface->unlock_cursor( );
		return;
	}

	lockcursor( REG_OUT );
}

void __fastcall doextraboneprocessing_hk( REG, int a2, int a3, int a4, int a5, int a6, int a7 ) {
	auto e = reinterpret_cast< player_t* >( ecx );

	if ( !e ) {
		doextraboneprocessing( REG_OUT, a2, a3, a4, a5, a6, a7 );
		return;
	}

	auto state = e->animstate( );

	if ( !state || !state->m_entity ) {
		doextraboneprocessing( REG_OUT, a2, a3, a4, a5, a6, a7 );
		return;
	}

	auto backup_on_ground = state->m_on_ground;
	state->m_on_ground = false;
	doextraboneprocessing( REG_OUT, a2, a3, a4, a5, a6, a7 );
	state->m_on_ground = backup_on_ground;
}

vec3_t* __fastcall get_eye_angles_hk( REG ) {
	auto local = csgo::i::ent_list->get< player_t* >( csgo::i::engine->get_local_player( ) );

	if ( ecx != local )
		return get_eye_angles( REG_OUT );

	static auto ret_to_thirdperson_pitch = pattern::search( "client_panorama.dll", "8B CE F3 0F 10 00 8B 06 F3 0F 11 45 ? FF 90 ? ? ? ? F3 0F 10 55 ?" ).get< std::uintptr_t >( );
	static auto ret_to_thirdperson_yaw = pattern::search( "client_panorama.dll", "F3 0F 10 55 ? 51 8B 8E ? ? ? ?" ).get< std::uintptr_t >( );

	if ( std::uintptr_t( _ReturnAddress( ) ) == ret_to_thirdperson_pitch
		|| std::uintptr_t( _ReturnAddress( ) ) == ret_to_thirdperson_yaw )
		return g::ucmd ? &g::ucmd->m_angs : &local->angles( );

	return get_eye_angles( REG_OUT );
}

bool __fastcall get_bool_hk( REG ) {
	static auto cam_think = pattern::search( "client_panorama.dll", "85 C0 75 30 38 86" ).get< std::uintptr_t >( );

	if ( !ecx )
		return false;

	auto local = csgo::i::ent_list->get< player_t* >( csgo::i::engine->get_local_player( ) );

	if ( std::uintptr_t( _ReturnAddress( ) ) == cam_think )
		return true;

	return get_bool( REG_OUT );
}

long __stdcall wndproc( HWND hwnd, std::uint32_t msg, std::uintptr_t wparam, std::uint32_t lparam ) {
	/* run window input processing here. */
	/*
	if ( menu::wndproc( hwnd, msg, wparam, lparam ) )
		return true;
	*/

	return CallWindowProcA( g_wndproc, hwnd, msg, wparam, lparam );
}

bool hooks::init( ) {
	/* create fonts */
	render::create_font( ( void** ) &fonts::def, L"Tahoma", 32, true );

	/* load default config */
	const auto clientmodeshared_createmove = pattern::search( "client_panorama.dll", "55 8B EC 8B 0D ? ? ? ? 85 C9 75 06 B0" ).get< decltype( &createmove_hk ) >( );
	const auto chlclient_framestagenotify = pattern::search( "client_panorama.dll", "55 8B EC 8B 0D ? ? ? ? 8B 01 8B 80 74 01 00 00 FF D0 A2" ).get< decltype( &framestagenotify_hk ) >( );
	const auto idirect3ddevice9_endscene = vfunc< decltype( &endscene_hk ) >( csgo::i::dev, 42 );
	const auto idirect3ddevice9_reset = vfunc< decltype( &reset_hk ) >( csgo::i::dev, 16 );
	const auto vguimatsurface_lockcursor = pattern::search( "vguimatsurface.dll", "80 3D ? ? ? ? 00 8B 91 A4 02 00 00 8B 0D ? ? ? ? C6 05 ? ? ? ? 01" ).get< decltype( &lockcursor_hk ) >( );
	const auto modelrender_drawmodelexecute = vfunc< decltype( &drawmodelexecute_hk ) >( csgo::i::mdl_render, 21 );
	const auto c_csplayer_doextraboneprocessing = pattern::search( "client_panorama.dll", "55 8B EC 83 E4 F8 81 EC FC 00 00 00 53 56 8B F1 57" ).get< void* >( );
	const auto c_csplayer_get_eye_angles = pattern::search( "client_panorama.dll", "56 8B F1 85 F6 74 32" ).get< void* >( );
	const auto convar_getbool = pattern::search( "client_panorama.dll", "8B 51 1C 3B D1 75 06" ).get< void* >( );
	const auto overrideview = pattern::search( "client_panorama.dll", "55 8B EC 83 E4 F8 83 EC 58 56 57 8B 3D ? ? ? ? 85 FF" ).get< void* >( );
	const auto engine_fire_event = vfunc< void* >( csgo::i::engine, 59 );

	MH_Initialize( );

	/* hook all the functions you need! */
	MH_CreateHook( engine_fire_event, fire_event_hk, ( void** ) &fire_event );
	MH_CreateHook( clientmodeshared_createmove, createmove_hk, ( void** ) &createmove );
	MH_CreateHook( chlclient_framestagenotify, framestagenotify_hk, ( void** ) &framestagenotify );
	MH_CreateHook( idirect3ddevice9_endscene, endscene_hk, ( void** ) &endscene );
	MH_CreateHook( idirect3ddevice9_reset, reset_hk, ( void** ) &reset );
	MH_CreateHook( vguimatsurface_lockcursor, lockcursor_hk, ( void** ) &lockcursor );
	MH_CreateHook( modelrender_drawmodelexecute, drawmodelexecute_hk, ( void** ) &drawmodelexecute );
	MH_CreateHook( c_csplayer_doextraboneprocessing, doextraboneprocessing_hk, ( void** ) &doextraboneprocessing );
	MH_CreateHook( c_csplayer_get_eye_angles, get_eye_angles_hk, ( void** ) &get_eye_angles );
	MH_CreateHook( convar_getbool, get_bool_hk, ( void** ) &get_bool );
	MH_CreateHook( overrideview, override_view_hk, ( void** ) &override_view );

	MH_EnableHook( MH_ALL_HOOKS );

	/* hook wndproc handler so we can intercept input. */
	g_wndproc = ( WNDPROC ) SetWindowLongA( FindWindowA( "Valve001", nullptr ), GWLP_WNDPROC, ( long ) wndproc );

	return true;
}