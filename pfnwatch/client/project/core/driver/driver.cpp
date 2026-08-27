// project/core/driver/driver.cpp
#include "../../global.hpp"

namespace core {

	bool driver::open( )
	{
		auto handle = ::CreateFileW( L"\\\\.\\pfnwatch", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr );
		if ( handle == INVALID_HANDLE_VALUE ) {
			return false;
		}

		this->m_handle = handle;

		return true;
	}

	void driver::close( )
	{
		::CloseHandle( this->m_handle );
	}

	bool driver::assign_process( unsigned long pid )
	{
		shared::ioctl::request request{};
		request.assign_process.process_id = pid;

		this->m_pid = pid;

		unsigned long bytes{ 0u };
		return ::DeviceIoControl( this->m_handle, static_cast< unsigned long >( shared::ioctl::command::assign_process ), &request, sizeof( request ), nullptr, 0, &bytes, nullptr );
	}

	bool driver::read_virtual_memory( unsigned long long address, void* buffer, unsigned long size )
	{
		shared::ioctl::request request{};
		request.read_virtual_memory.process_id = this->m_pid;
		request.read_virtual_memory.address = address;
		request.read_virtual_memory.size = size;

		unsigned long bytes{ 0u };
		return ::DeviceIoControl( this->m_handle, static_cast< unsigned long >( shared::ioctl::command::read_virtual_memory ), &request, sizeof( request ), buffer, size, &bytes, nullptr );
	}

	unsigned long driver::read_report( shared::ioctl::detection* buffer, unsigned long max_count )
	{
		shared::ioctl::request request{};
		unsigned long bytes{ 0u };

		if ( !::DeviceIoControl( this->m_handle, static_cast< unsigned long >( shared::ioctl::command::read_report ), &request, sizeof( request ), buffer, max_count * sizeof( shared::ioctl::detection ), &bytes, nullptr ) ) {
			return 0u;
		}

		return bytes / sizeof( shared::ioctl::detection );
	}

} // namespace core