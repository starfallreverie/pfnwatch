// project/core/driver/driver.hpp
#pragma once

namespace core {

	class driver
	{
	public:
		bool open( );
		void close( );

		bool assign_process( unsigned long pid );
		bool read_virtual_memory( unsigned long long address, void* buffer, unsigned long size );
		unsigned long read_report( shared::ioctl::detection* buffer, unsigned long max_count );

	private:
		unsigned long m_pid;
		void* m_handle;
	};

} // namespace core