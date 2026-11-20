#include "SysInfo.h"
#if defined (_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include <psapi.h>
 #pragma comment (lib, "psapi.lib")
 namespace sysinfo { double processMemoryMB() { PROCESS_MEMORY_COUNTERS pmc {}; if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc))) return (double) pmc.WorkingSetSize / (1024.0 * 1024.0); return 0.0; } }
#elif defined (__linux__)
 #include <cstdio>
 #include <unistd.h>
 namespace sysinfo { double processMemoryMB() { long total = 0, res = 0; if (FILE* f = std::fopen ("/proc/self/statm", "r")) { if (std::fscanf (f, "%ld %ld", &total, &res) != 2) res = 0; std::fclose (f); } return (double) res * (double) sysconf (_SC_PAGESIZE) / (1024.0 * 1024.0); } }
#else
 namespace sysinfo { double processMemoryMB() { return 0.0; } }
#endif
