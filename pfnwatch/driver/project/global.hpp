// project/global.hpp
#pragma once

// wdk
#include <ntifs.h>
#include <wdf.h>
#include <ntimage.h>
#include <intrin.h>

// shared
#include "../../shared/ioctl.hpp"

// common
#include "common/nt.hpp"

// core
#include "core/collector/collector.hpp"
#include "core/dispatcher/dispatcher.hpp"
#include "core/worker/worker.hpp"
#include "core/scanner/scanner.hpp"
#include "core/kernel/kernel.hpp"

struct global
{
	core::collector m_collector;
	core::dispatcher m_dispatcher;
	core::worker m_worker;
	core::scanner m_scanner;
	core::kernel m_kernel;
} inline g;