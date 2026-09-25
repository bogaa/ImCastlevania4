#include "asardll.h"

#include <cstring>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

struct AsarDll::Functions {
    using Init = bool (*)();
    using Close = void (*)();
    using Version = int (*)();
    using ApiVersion = int (*)();
    using Reset = bool (*)();
    using Patch = bool (*)(const char*, char*, int, int*);
    using PatchEx = bool (*)(const patchparams*);
    using MaxRomSize = int (*)();
    using GetErrors = const errordata* (*)(int*);
    using GetWarnings = const errordata* (*)(int*);
    using GetPrints = const char* const* (*)(int*);
    using GetAllLabels = const labeldata* (*)(int*);
    using GetLabelValue = int (*)(const char*);
    using GetDefine = const char* (*)(const char*);
    using GetAllDefines = const definedata* (*)(int*);
    using ResolveDefines = const char* (*)(const char*, bool);
    using Math = double (*)(const char*, const char**);
    using GetWrittenBlocks = const writtenblockdata* (*)(int*);
    using GetMapper = mappertype (*)();
    using GetSymbolsFile = const char* (*)(const char*);

    Init init = nullptr;
    Close close = nullptr;
    Version version = nullptr;
    ApiVersion apiVersion = nullptr;
    Reset reset = nullptr;
    Patch patch = nullptr;
    PatchEx patchEx = nullptr;
    MaxRomSize maxRomSize = nullptr;
    GetErrors getErrors = nullptr;
    GetWarnings getWarnings = nullptr;
    GetPrints getPrints = nullptr;
    GetAllLabels getAllLabels = nullptr;
    GetLabelValue getLabelValue = nullptr;
    GetDefine getDefine = nullptr;
    GetAllDefines getAllDefines = nullptr;
    ResolveDefines resolveDefines = nullptr;
    Math math = nullptr;
    GetWrittenBlocks getWrittenBlocks = nullptr;
    GetMapper getMapper = nullptr;
    GetSymbolsFile getSymbolsFile = nullptr;
};

namespace {

template <typename Function>
bool AssignFunction(Function& target, void* address)
{
    if (!address) {
        target = nullptr;
        return false;
    }
    static_assert(sizeof(Function) == sizeof(address));
    std::memcpy(&target, &address, sizeof(target));
    return true;
}

#if defined(_WIN32)
void* FindSymbol(void* module, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
}
#else
void* FindSymbol(void* module, const char* name)
{
    return dlsym(module, name);
}
#endif

} // namespace

AsarDll::~AsarDll()
{
    Close();
}

bool AsarDll::Init()
{
    if (IsLoaded()) {
        return true;
    }

    module_ = LoadLibraryDefault();
    if (!module_) {
        return false;
    }
    return InitShared();
}

bool AsarDll::InitWithDllPath(const char* dllPath)
{
    if (IsLoaded()) {
        return true;
    }
    if (!dllPath || !*dllPath) {
        return false;
    }

    module_ = LoadLibraryFromPath(dllPath);
    if (!module_) {
        return false;
    }
    return InitShared();
}

void AsarDll::Close()
{
    if (functions_ && functions_->close) {
        functions_->close();
    }
    delete functions_;
    functions_ = nullptr;
    UnloadLibrary();
}

bool AsarDll::IsLoaded() const
{
    return module_ != nullptr && functions_ != nullptr;
}

bool AsarDll::InitShared()
{
    std::string error;
    if (!LoadFunctionPointers(error)) {
        Close();
        return false;
    }
    if (functions_->apiVersion() < expectedapiversion
        || functions_->apiVersion() / 100 > expectedapiversion / 100) {
        Close();
        return false;
    }
    if (!functions_->init()) {
        Close();
        return false;
    }
    return true;
}

bool AsarDll::LoadFunctionPointers(std::string& error)
{
    functions_ = new Functions;

#define LOAD_ASAR_FUNCTION(member, name) \
    if (!AssignFunction(functions_->member, FindSymbol(module_, "asar_" name))) { \
        error = "Missing Asar function: asar_" name; \
        return false; \
    }

    LOAD_ASAR_FUNCTION(init, "init");
    LOAD_ASAR_FUNCTION(close, "close");
    LOAD_ASAR_FUNCTION(version, "version");
    LOAD_ASAR_FUNCTION(apiVersion, "apiversion");
    LOAD_ASAR_FUNCTION(reset, "reset");
    LOAD_ASAR_FUNCTION(patch, "patch");
    LOAD_ASAR_FUNCTION(patchEx, "patch_ex");
    LOAD_ASAR_FUNCTION(maxRomSize, "maxromsize");
    LOAD_ASAR_FUNCTION(getErrors, "geterrors");
    LOAD_ASAR_FUNCTION(getWarnings, "getwarnings");
    LOAD_ASAR_FUNCTION(getPrints, "getprints");
    LOAD_ASAR_FUNCTION(getAllLabels, "getalllabels");
    LOAD_ASAR_FUNCTION(getLabelValue, "getlabelval");
    LOAD_ASAR_FUNCTION(getDefine, "getdefine");
    LOAD_ASAR_FUNCTION(getAllDefines, "getalldefines");
    LOAD_ASAR_FUNCTION(resolveDefines, "resolvedefines");
    LOAD_ASAR_FUNCTION(math, "math");
    LOAD_ASAR_FUNCTION(getWrittenBlocks, "getwrittenblocks");
    LOAD_ASAR_FUNCTION(getMapper, "getmapper");
    LOAD_ASAR_FUNCTION(getSymbolsFile, "getsymbolsfile");

#undef LOAD_ASAR_FUNCTION
    return true;
}

bool AsarDll::Patch(const char* patchPath, std::vector<std::uint8_t>& rom, std::string& error)
{
    error.clear();
    if (!IsLoaded()) {
        error = "Asar DLL is not initialized.";
        return false;
    }
    if (!patchPath || !*patchPath) {
        error = "No Asar patch path was provided.";
        return false;
    }
    if (rom.size() > static_cast<std::size_t>(INT_MAX)) {
        error = "ROM is too large for the Asar API.";
        return false;
    }

    const std::size_t originalSize = rom.size();
    const int maximumSize = functions_->maxRomSize();
    if (maximumSize <= 0) {
        error = "Asar returned an invalid maximum ROM size.";
        return false;
    }

    rom.resize(static_cast<std::size_t>(maximumSize));
    int romLength = static_cast<int>(originalSize);
    const bool success = functions_->patch(
        patchPath,
        reinterpret_cast<char*>(rom.data()),
        maximumSize,
        &romLength);

    if (!success || romLength < 0 || romLength > maximumSize) {
        rom.resize(originalSize);
        error = CollectErrors();
        if (error.empty()) {
            error = "Asar failed to apply the patch.";
        }
        return false;
    }

    rom.resize(static_cast<std::size_t>(romLength));
    return true;
}

std::string AsarDll::CollectErrors() const
{
    if (!IsLoaded() || !functions_->getErrors) {
        return {};
    }

    int count = 0;
    const errordata* errors = functions_->getErrors(&count);
    std::ostringstream result;
    for (int i = 0; errors && i < count; ++i) {
        if (i != 0) {
            result << '\n';
        }
        result << (errors[i].fullerrdata ? errors[i].fullerrdata : "Asar reported an unknown error.");
    }
    return result.str();
}

void* AsarDll::LoadLibraryDefault()
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryW(L"asar.dll"));
#else
    return dlopen("./libasar.so", RTLD_LAZY);
#endif
}

void* AsarDll::LoadLibraryFromPath(const char* path)
{
#if defined(_WIN32)
    const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (requiredSize <= 0) {
        return nullptr;
    }
    std::vector<wchar_t> widePath(static_cast<std::size_t>(requiredSize));
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath.data(), requiredSize) <= 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(LoadLibraryW(widePath.data()));
#else
    return dlopen(path, RTLD_LAZY);
#endif
}

void AsarDll::UnloadLibrary()
{
    if (!module_) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(module_));
#else
    dlclose(module_);
#endif
    module_ = nullptr;
}
