#define YEAR    "2021"          // Copyright year
#define VERSION "1.20a"         // Version string
#define VER_RES 1,20,1,00       // For VERSIONINFO resource
#define DEFAULT_RAM PAGE_OFFSET+0x200000 // Initial amount of RAM to allocate
#ifdef __EMSCRIPTEN__
#define MAXIMUM_RAM 0x1000000   // Maximum amount of RAM to allocate
#else
#define MAXIMUM_RAM 0x10000000  // Maximum amount of RAM to allocate
#endif
