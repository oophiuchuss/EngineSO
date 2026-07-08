module;
export module ReprocessOptions;

export struct ReprocessOptions
{
    // true: full reconstruction from original source (re-read file/re-parse).
    // false: reprocess using already-loaded in-memory data only (e.g. regenerate
    // normals from existing positions/indices, no disk access).
    bool bFullReconstruct = false;

    virtual ~ReprocessOptions() = default;
};