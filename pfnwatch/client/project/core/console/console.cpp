// project/core/console/console.cpp
#include "../../global.hpp"

namespace core {

	namespace ansi {

		constexpr auto reset = "\x1b[0m";
		constexpr auto bold = "\x1b[1m";
		constexpr auto dim = "\x1b[2m";
		constexpr auto red = "\x1b[91m";
		constexpr auto green = "\x1b[92m";
		constexpr auto yellow = "\x1b[93m";
		constexpr auto cyan = "\x1b[38;2;140;200;210m";
		constexpr auto white = "\x1b[97m";
		constexpr auto gray = "\x1b[90m";

	} // namespace ansi

	void console::initialize( )
	{
		::SetConsoleTitleA( "pfnwatch" );
		::SetConsoleOutputCP( CP_UTF8 );

		auto* handle = ::GetStdHandle( STD_OUTPUT_HANDLE );

		unsigned long mode{ 0u };
		::GetConsoleMode( handle, &mode );
		::SetConsoleMode( handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );

		std::printf( "\x1b[?25l" );
	}

	void console::shutdown( )
	{
		std::printf( "\x1b[?25h" );
	}

	void console::render( const std::vector<tracked_detection>& detections )
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi{};
		::GetConsoleScreenBufferInfo( ::GetStdHandle( STD_OUTPUT_HANDLE ), &csbi );
		const auto max_rows = static_cast< unsigned long >( csbi.srWindow.Bottom - csbi.srWindow.Top - 5 );

		std::printf( "\x1b[H" );

		this->render_header( );

		const auto visible = static_cast< unsigned long >( detections.size( ) ) < max_rows ? static_cast< unsigned long >( detections.size( ) ) : max_rows;
		for ( unsigned long i{ 0u }; i < visible; i++ ) {
			this->render_row( detections[ i ] );
		}

		std::printf( "\x1b[J" );
		this->render_footer( static_cast< unsigned long >( detections.size( ) ) );
	}

	void console::render_header( )
	{
		std::printf( "\n" );
		std::printf( "  %s%-28s %-10s %-20s %5s %7s  %-14s%s\n", ansi::dim, "target", "pfn", "kernel va", "refs", "caught", "flags", ansi::reset );
		std::printf( "  %s%-28s %-10s %-20s %5s %7s  %-14s%s\n\n", ansi::gray, "------", "---", "---------", "----", "------", "-----", ansi::reset );
	}

	void console::render_row( const tracked_detection& detection )
	{
		char target[ 256 ]{};
		this->resolve_target( detection, target, sizeof( target ) );

		char flags[ 64 ]{};
		this->format_flags( detection.pte_value, flags, sizeof( flags ) );

		const auto* color = this->tick_color( detection.total_ticks );
		std::printf( "  %s%-28s%s %s0x%-8llx%s %s0x%016llx%s %s%5hu%s %s%7lu%s  %s%s%s\n", ansi::cyan, target, ansi::reset, ansi::gray, detection.pfn, ansi::reset, ansi::gray, detection.virtual_address, ansi::reset, ansi::dim, detection.reference_count, ansi::reset, color, detection.total_ticks, ansi::reset, ansi::dim, flags, ansi::reset );
	}

	void console::render_footer( unsigned long count )
	{
		std::printf( "\n  %s%lu detection(s)%s\n", ansi::gray, count, ansi::reset );
	}

	void console::resolve_target( const tracked_detection& detection, char* out, unsigned long max_length )
	{
		if ( !detection.target_address )
		{
			::sprintf_s( out, max_length, "unknown" );
			return;
		}

		HMODULE module{};
		if ( !::GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast< LPCWSTR >( detection.target_address ), &module ) )
		{
			::sprintf_s( out, max_length, "0x%llx (private)", detection.target_address );
			return;
		}

		wchar_t path[ MAX_PATH ]{};
		::GetModuleFileNameW( module, path, MAX_PATH );

		auto* name = ::wcsrchr( path, L'\\' );
		name = name ? name + 1 : path;

		const auto offset = detection.target_address - reinterpret_cast< unsigned long long >( module );
		::sprintf_s( out, max_length, "%ls+0x%llx", name, offset );
	}

	void console::format_flags( unsigned long long pte, char* out, unsigned long max_length )
	{
		if ( !pte )
		{
			::sprintf_s( out, max_length, "-" );
			return;
		}

		::sprintf_s( out, max_length, "%s%s%s%s%s%s", ( pte & ( 1ull << 0 ) ) ? "P" : "-", ( pte & ( 1ull << 1 ) ) ? "|W" : "|R", ( pte & ( 1ull << 2 ) ) ? "|U" : "|K", ( pte & ( 1ull << 5 ) ) ? "|A" : "", ( pte & ( 1ull << 6 ) ) ? "|D" : "", ( pte & ( 1ull << 63 ) ) ? "|NX" : "|X" );
	}

	const char* console::tick_color( unsigned long ticks )
	{
		if ( ticks >= 50u ) {
			return ansi::red;
		}

		if ( ticks >= 10u ) {
			return ansi::yellow;
		}

		return ansi::white;
	}

} // namespace core