// client/project/core/console/console.cpp
#include "../../global.hpp"

namespace core {

	static void format_flags( std::uint64_t pte, char* out, std::uint32_t length )
	{
		if ( !pte )
		{
			std::snprintf( out, length, "-" );
			return;
		}

		std::snprintf( out, length, "%s%s%s%s%s%s", ( pte & 1ull ) ? "P" : "-", ( pte & 2ull ) ? "|W" : "|R", ( pte & 4ull ) ? "|U" : "|K", ( pte & ( 1ull << 5 ) ) ? "|A" : "", ( pte & ( 1ull << 6 ) ) ? "|D" : "", ( pte & ( 1ull << 63 ) ) ? "|NX" : "|X" );
	}

	void console::initialize( )
	{
		::SetConsoleTitleA( "pfnwatch" );
		::SetConsoleOutputCP( CP_UTF8 );

		const auto handle = ::GetStdHandle( STD_OUTPUT_HANDLE );

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

		const auto max_rows = static_cast< std::uint32_t >( csbi.srWindow.Bottom - csbi.srWindow.Top - 5 );
		const auto visible = std::min( static_cast< std::uint32_t >( detections.size( ) ), max_rows );

		std::printf( "\x1b[H" );
		this->render_header( );

		for ( std::uint32_t i{ 0u }; i < visible; i++ ) {
			this->render_row( detections[ i ] );
		}

		std::printf( "\x1b[J" );
		this->render_footer( static_cast< std::uint32_t >( detections.size( ) ) );
	}

	void console::render_header( )
	{
		std::printf( "\n" );
		std::printf( "  \x1b[2m%-28s %-10s %-20s %5s %7s  %-14s\x1b[0m\n", "target", "pfn", "kernel va", "refs", "caught", "flags" );
		std::printf( "  \x1b[90m%-28s %-10s %-20s %5s %7s  %-14s\x1b[0m\n\n", "------", "---", "---------", "----", "------", "-----" );
	}

	void console::render_row( const tracked_detection& detection )
	{
		char target[ 256 ]{};

		if ( !detection.target_address ) {
			std::snprintf( target, sizeof( target ), "unknown" );
		}
		else
		{
			HMODULE module{};
			if ( !::GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast< LPCWSTR >( detection.target_address ), &module ) ) {
				std::snprintf( target, sizeof( target ), "0x%llx (private)", detection.target_address );
			}
			else
			{
				wchar_t path[ MAX_PATH ]{};
				::GetModuleFileNameW( module, path, MAX_PATH );

				auto* name = ::wcsrchr( path, L'\\' );
				name = name ? name + 1 : path;

				const auto offset = detection.target_address - reinterpret_cast< std::uint64_t >( module );
				std::snprintf( target, sizeof( target ), "%ls+0x%llx", name, offset );
			}
		}

		char flags[ 64 ]{};
		format_flags( detection.pte_value, flags, sizeof( flags ) );

		const auto* color = detection.total_ticks >= 50u ? "\x1b[91m" : detection.total_ticks >= 10u ? "\x1b[93m" : "\x1b[97m";

		std::printf( "  \x1b[38;2;140;200;210m%-28s\x1b[0m \x1b[90m0x%-8llx\x1b[0m \x1b[90m0x%016llx\x1b[0m \x1b[2m%5hu\x1b[0m %s%7u\x1b[0m  \x1b[2m%s\x1b[0m\n", target, detection.pfn, detection.virtual_address, detection.reference_count, color, detection.total_ticks, flags );
	}

	void console::render_footer( std::uint32_t count )
	{
		std::printf( "\n  \x1b[90m%u detection(s)\x1b[0m\n", count );
	}

} // namespace core