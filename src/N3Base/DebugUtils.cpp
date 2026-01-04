#include "StdAfxBase.h"

void DebugString(const std::string_view logMessage)
{
#ifdef _WIN32
	constexpr std::string_view NewLine = "\r\n";
#else
	constexpr std::string_view NewLine = "\n";
#endif

	std::string outputMessage;
	outputMessage.reserve(logMessage.size() + NewLine.size());
	outputMessage  = logMessage;
	outputMessage += NewLine;

#ifdef WIN32
	OutputDebugStringA(outputMessage.c_str());
#else
	printf("%s", outputMessage.c_str());
#endif
}
