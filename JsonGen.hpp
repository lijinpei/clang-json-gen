#pragma once

#include "spdlog.h"

#include <memory>

#ifndef NDEBUG
#define SPDLOG_ENTER() SPDLOG_TRACE(debug_logger, "{} enter", __func__)
extern std::shared_ptr<spdlog::logger> console_logger;
extern std::shared_ptr<spdlog::logger> debug_logger;
#endif
