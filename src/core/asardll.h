#ifndef ASAR_DLL_H_INCLUDED
#define ASAR_DLL_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr int expectedapiversion = 303;

struct errordata {
    const char* fullerrdata;
    const char* rawerrdata;
    const char* block;
    const char* filename;
    int line;
    const char* callerfilename;
    int callerline;
    int errid;
};

struct labeldata {
    const char* name;
    int location;
};

struct definedata {
    const char* name;
    const char* contents;
};

struct writtenblockdata {
    int pcoffset;
    int snesoffset;
    int numbytes;
};

enum mappertype {
    invalid_mapper,
    lorom,
    hirom,
    sa1rom,
    bigsa1rom,
    sfxrom,
    exlorom,
    exhirom,
    norom
};

struct warnsetting {
    const char* warnid;
    bool enabled;
};

struct memoryfile {
    const char* path;
    const void* buffer;
    std::size_t length;
};

struct patchparams {
    int structsize;
    const char* patchloc;
    char* romdata;
    int buflen;
    int* romlen;
    const char** includepaths;
    int numincludepaths;
    bool should_reset;
    const definedata* additional_defines;
    int additional_define_count;
    const char* stdincludesfile;
    const char* stddefinesfile;
    const warnsetting* warning_settings;
    int warning_setting_count;
    const memoryfile* memory_files;
    int memory_file_count;
    bool override_checksum_gen;
    bool generate_checksum;
};

class AsarDll {
public:
    AsarDll() = default;
    ~AsarDll();

    AsarDll(const AsarDll&) = delete;
    AsarDll& operator=(const AsarDll&) = delete;

    bool Init();
    bool InitWithDllPath(const char* dllPath);
    void Close();
    bool IsLoaded() const;

    bool Patch(const char* patchPath, std::vector<std::uint8_t>& rom, std::string& error);

private:
    struct Functions;

    bool InitShared();
    bool LoadFunctionPointers(std::string& error);
    void* LoadLibraryDefault();
    void* LoadLibraryFromPath(const char* path);
    void UnloadLibrary();
    std::string CollectErrors() const;

    void* module_ = nullptr;
    Functions* functions_ = nullptr;
};

#endif
