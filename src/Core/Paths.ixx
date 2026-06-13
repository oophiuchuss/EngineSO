module;

#include <string>

export module Paths;

export class Paths
{
public:
    static void SetAssetsRoot(const std::string& Path)
    {
        AssetsRoot = Path;
        if (!AssetsRoot.empty() && AssetsRoot.back() != '/')
        {
            AssetsRoot += '/';
        }
    }

    static void SetShadersRoot(const std::string& Path)
    {
        ShadersRoot = Path;
        if (!ShadersRoot.empty() && ShadersRoot.back() != '/')
        {
            ShadersRoot += '/';
        }
    }

    static void SetCacheRoot(const std::string& Path)
    {
        CacheRoot = Path;
        if (!CacheRoot.empty() && CacheRoot.back() != '/')
        {
            CacheRoot += '/';
        }
    }

    static const std::string& GetAssetsRoot() { return AssetsRoot; }
    static const std::string& GetShadersRoot() { return ShadersRoot; }
    static const std::string& GetCacheRoot() { return CacheRoot; }

private:
    inline static std::string AssetsRoot = "assets/";
    inline static std::string ShadersRoot = "shaders/";
    inline static std::string CacheRoot = "cache/";
};