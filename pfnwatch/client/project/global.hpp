// project/global.hpp
#pragma once

#define NOMINMAX

// windows
#include <Windows.h>

// standard
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <utility>
#include <thread>
#include <algorithm>
#include <chrono>

// shared
#include "../../shared/ioctl.hpp"

// core
#include "core/console/console.hpp"
#include "core/driver/driver.hpp"
#include "core/reader/reader.hpp"

struct global
{
	core::console m_console;
	core::driver m_driver;
	core::reader m_reader;
} inline g;