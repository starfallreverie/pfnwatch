// project/core/dispatcher/dispatcher.hpp
#pragma once

namespace core {

	class dispatcher
	{
	public:
		NTSTATUS device_add( WDFDRIVER driver, PWDFDEVICE_INIT device_init );
		void driver_unload( WDFDRIVER );

	private:
		static void device_control( WDFQUEUE queue, WDFREQUEST request, size_t output_buffer_length, size_t input_buffer_length, ULONG io_control_code );

		static NTSTATUS handle_assign_process( const shared::ioctl::request* input );
		static NTSTATUS handle_read_virtual_memory( const shared::ioctl::request* input, WDFREQUEST request );
		static NTSTATUS handle_get_detection_count( WDFREQUEST request );
		static NTSTATUS handle_read_report( WDFREQUEST request );
	};

} // namespace core