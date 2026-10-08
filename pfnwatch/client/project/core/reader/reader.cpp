// project/core/reader/reader.cpp
#include "../../global.hpp"

namespace core {

	void reader::start( )
	{
		std::thread( [ ] { reader::routine( ); } ).detach( );
	}

	unsigned long __stdcall reader::routine( )
	{
		const auto base = reinterpret_cast< std::uintptr_t >( ::GetModuleHandleW( nullptr ) );

		unsigned char buffer[ 0x1000 ];

		while ( true )
		{
			g.m_driver.read_virtual_memory( base + 0x1000, buffer, 0x1000u );
			g.m_driver.read_virtual_memory( base + 0x2000, buffer, 0x1000u );
			g.m_driver.read_virtual_memory( base + 0x3000, buffer, 0x1000u );
			g.m_driver.read_virtual_memory( base + 0x4000, buffer, 0x1000u );
			g.m_driver.read_virtual_memory( base + 0x5000, buffer, 0x1000u );
			g.m_driver.read_virtual_memory( base + 0x6000, buffer, 0x1000u );
		}

		return 0u;
	}

} // namespace core