// project/core/worker/worker.hpp
#pragma once

namespace core {

	class worker
	{
	public:
		NTSTATUS start( );
		void stop( );

	private:
		static void routine( void* context );
		void loop( );

	private:
		void* m_thread_object;
		KEVENT m_stop_event;
	};

} // namespace core