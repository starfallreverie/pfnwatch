// project/core/worker/worker.cpp
#include "../../global.hpp"

namespace core {

	NTSTATUS worker::start( )
	{
		::KeInitializeEvent( &this->m_stop_event, EVENT_TYPE::NotificationEvent, 0 );

		HANDLE handle{ nullptr };
		const auto status = ::PsCreateSystemThread( &handle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, &worker::routine, this );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		::ObReferenceObjectByHandle( handle, THREAD_ALL_ACCESS, *::PsThreadType, MODE::KernelMode, &this->m_thread_object, nullptr );
		::ZwClose( handle );

		return STATUS_SUCCESS;
	}

	void worker::stop( )
	{
		::KeSetEvent( &this->m_stop_event, 0, 0 );

		if ( this->m_thread_object )
		{
			::KeWaitForSingleObject( this->m_thread_object, KWAIT_REASON::Executive, MODE::KernelMode, 0, nullptr );
			::ObfDereferenceObject( this->m_thread_object );
			this->m_thread_object = nullptr;
		}
	}

	void worker::routine( void* context )
	{
		static_cast< worker* >( context )->loop( );
		::PsTerminateSystemThread( STATUS_SUCCESS );
	}

	void worker::loop( )
	{
		LARGE_INTEGER timeout{};

		while ( ::KeWaitForSingleObject( &this->m_stop_event, KWAIT_REASON::Executive, MODE::KernelMode, 0, &timeout ) == STATUS_TIMEOUT )
		{
			g.m_scanner.tick( );
		}
	}

} // namespace core