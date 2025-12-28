#pragma once

#include <mpg123.h>

mpg123_ssize_t mpg123_filereader_read(void* userData, void* dst, size_t bytes);
off_t mpg123_filereader_seek(void* userData, off_t offset, int whence);
void mpg123_filereader_cleanup(void* userData);
