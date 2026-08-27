// project/core/dispatcher/dispatcher.cpp
#include "../../global.hpp"

namespace core {

	NTSTATUS dispatcher::device_add( WDFDRIVER driver, PWDFDEVICE_INIT device_init )
	{
		UNREFERENCED_PARAMETER( driver );

		DECLARE_CONST_UNICODE_STRING( device_name, L"\\Device\\pfnwatch" );
		DECLARE_CONST_UNICODE_STRING( symlink_name, L"\\DosDevices\\pfnwatch" );

		::WdfDeviceInitSetIoType( device_init, WDF_DEVICE_IO_TYPE::WdfDeviceIoBuffered );
		::WdfDeviceInitAssignName( device_init, &device_name );

		WDFDEVICE device{};
		auto status = ::WdfDeviceCreate( &device_init, WDF_NO_OBJECT_ATTRIBUTES, &device );
		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		status = ::WdfDeviceCreateSymbolicLink( device, &symlink_name );
		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		WDF_IO_QUEUE_CONFIG queue_config{};
		::WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE( &queue_config, WDF_IO_QUEUE_DISPATCH_TYPE::WdfIoQueueDispatchParallel );
		queue_config.EvtIoDeviceControl = dispatcher::device_control;

		WDFQUEUE queue{};
		status = ::WdfIoQueueCreate( device, &queue_config, WDF_NO_OBJECT_ATTRIBUTES, &queue );
		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		if ( !g.m_kernel.initialize( ) ) {
			return STATUS_FAILED_DRIVER_ENTRY;
		}

		status = g.m_worker.start( );
		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		return STATUS_SUCCESS;
	}

	void dispatcher::driver_unload( WDFDRIVER )
	{
		g.m_worker.stop( );
		g.m_collector.cleanup( );
		g.m_scanner.cleanup( );
	}

	void dispatcher::device_control( WDFQUEUE queue, WDFREQUEST request, size_t output_buffer_length, size_t input_buffer_length, ULONG io_control_code )
	{
		shared::ioctl::request* input{};
		auto status = ::WdfRequestRetrieveInputBuffer( request, sizeof( shared::ioctl::request ), reinterpret_cast< void** >( &input ), nullptr );

		if ( !NT_SUCCESS( status ) )
		{
			::WdfRequestComplete( request, status );
			return;
		}

		switch ( static_cast< shared::ioctl::command >( io_control_code ) )
		{
		case shared::ioctl::command::assign_process:
			status = dispatcher::handle_assign_process( input );
			break;

		case shared::ioctl::command::read_virtual_memory:
			status = dispatcher::handle_read_virtual_memory( input, request );
			if ( NT_SUCCESS( status ) ) {
				return;
			}
			break;

		case shared::ioctl::command::read_report:
			status = dispatcher::handle_read_report( request );
			if ( NT_SUCCESS( status ) ) {
				return;
			}
			break;

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}

		::WdfRequestComplete( request, status );
	}

	NTSTATUS dispatcher::handle_assign_process( const shared::ioctl::request* input )
	{
		g.m_scanner.reset( );
		return g.m_collector.assign( input->assign_process.process_id );
	}

	NTSTATUS dispatcher::handle_read_virtual_memory( const shared::ioctl::request* input, WDFREQUEST request )
	{
		PEPROCESS process{};
		auto status = ::PsLookupProcessByProcessId( reinterpret_cast< void* >( static_cast< uintptr_t >( input->read_virtual_memory.process_id ) ), &process );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}
		 
		KAPC_STATE apc_state{};
		::KeStackAttachProcess( process, &apc_state );

		const auto physical = ::MmGetPhysicalAddress( reinterpret_cast< void* >( input->read_virtual_memory.address ) );

		::KeUnstackDetachProcess( &apc_state );
		::ObfDereferenceObject( process );

		if ( !physical.QuadPart ) {
			return STATUS_INVALID_ADDRESS;
		}

		void* output{};
		status = ::WdfRequestRetrieveOutputBuffer( request, input->read_virtual_memory.size, &output, nullptr );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		MM_COPY_ADDRESS source{};
		source.PhysicalAddress = physical;

		unsigned long long bytes_copied{ 0ull };
		status = ::MmCopyMemory( output, source, input->read_virtual_memory.size, MM_COPY_MEMORY_PHYSICAL, reinterpret_cast< size_t* >( &bytes_copied ) );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		::WdfRequestCompleteWithInformation( request, STATUS_SUCCESS, bytes_copied );
		return STATUS_SUCCESS;
	}

	NTSTATUS dispatcher::handle_read_report( WDFREQUEST request )
	{
		void* output{};
		size_t output_length{};
		const auto status = ::WdfRequestRetrieveOutputBuffer( request, sizeof( shared::ioctl::detection ), &output, &output_length );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		const auto max_count = static_cast< unsigned long >( output_length / sizeof( shared::ioctl::detection ) );

		KIRQL old_irql{};
		KeAcquireSpinLock( &g.m_scanner.m_lock, &old_irql );

		const auto count = g.m_scanner.m_detection_count < max_count ? g.m_scanner.m_detection_count : max_count;
		const auto size = count * sizeof( shared::ioctl::detection );

		::memcpy( output, g.m_scanner.m_detections, size );
		g.m_scanner.reset_detections( );

		::KeReleaseSpinLock( &g.m_scanner.m_lock, old_irql );

		::WdfRequestCompleteWithInformation( request, STATUS_SUCCESS, size );
		return STATUS_SUCCESS;
	}

} // namespace core