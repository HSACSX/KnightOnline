#ifndef CLIENT_N3BASE_AL_WRAPPER_H
#define CLIENT_N3BASE_AL_WRAPPER_H

#pragma once

#include <AL/al.h>

#define AL_CHECK_ERROR() AL_CHECK_ERROR_IMPL(__FILE__, __LINE__)

bool AL_CHECK_ERROR_IMPL(const char* file, int line);

static constexpr int MAX_SOURCE_IDS			= 128;
static constexpr int MAX_STREAM_SOURCES		= 6;
static constexpr uint32_t INVALID_SOURCE_ID	= ~0U;

#endif // CLIENT_N3BASE_AL_WRAPPER_H
