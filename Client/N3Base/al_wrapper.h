#ifndef CLIENT_N3BASE_AL_WRAPPER_H
#define CLIENT_N3BASE_AL_WRAPPER_H

#pragma once

#include <AL/al.h>

#define AL_CHECK_ERROR() al_check_error_impl(__FILE__, __LINE__)
#define AL_CLEAR_ERROR_STATE() alGetError()

bool al_check_error_impl(const char* file, int line);

static constexpr int MAX_SOURCE_IDS			= 128;
static constexpr int MAX_STREAM_SOURCES		= 6;
static constexpr uint32_t INVALID_SOURCE_ID	= ~0U;

#endif // CLIENT_N3BASE_AL_WRAPPER_H
