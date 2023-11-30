#pragma once

//#define FILE_STREAM
//#define FILE_SYSTEM

#ifdef FILE_STREAM
#include <fstream>

#ifdef FILE_SYSTEM
#include <filesystem>
#endif // FILE_SYSTEM
#endif // FILE_STREAM

#include "Eterfree/Platform/Core/Common.h"
