module;

#include <memory>
#include <string>

export module ResourceHandle;

export template<typename T>
class ResourceHandle {
public:
    ResourceHandle() = default;

    explicit ResourceHandle(std::shared_ptr<T> InResource):
        WeakRef(InResource),
        ResourceID(InResource ? InResource->GetResourceID() : "")
    {
    }

    T* Get() const { auto S = WeakRef.lock(); return S ? S.get() : nullptr; }
    bool IsValid() const { return !WeakRef.expired(); }
    const std::string& GetResourceID() const { return ResourceID; }
    T* operator->() const { return Get(); }
    T& operator*() const { return *Get(); }
    operator bool() const { return IsValid(); }

private:
    std::weak_ptr<T> WeakRef;
    std::string      ResourceID;
};