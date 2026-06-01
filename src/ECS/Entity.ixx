module;

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

export module Entity;

import Component;

export class Entity 
{
public:

	friend class Scene; // Allow Scene to access private constructors and members for entity management

	virtual ~Entity() = default;

	const std::string& GetName() const { return Name; }
	bool IsActive() const { return bIsActive; }
	void SetActive(bool bInActive) { bIsActive = bInActive; }

	virtual void Initialize();
	virtual void Update(float DeltaTime);
	virtual void Render();

	template<typename T, typename ...Args>
	T* AddComponent(Args && ...args)
	{
		static_assert(std::is_base_of_v<ComponentBase, T>, "T must be a subclass of ComponentBase");

		size_t TypeID = ComponentBase::GetTypeID<T>();

		// prohibit adding multiple components of the same type
		auto it = ComponentMap.find(TypeID);
		if (it != ComponentMap.end())
		{
			return dynamic_cast<T*>(it->second);
		}

		//Create a new component and add it to the entity's component list
		auto NewComponent = std::make_unique<T>(std::forward<Args>(args)...);
		T* ComponentPtr = NewComponent.get();
		ComponentMap[TypeID] = ComponentPtr;
		ComponentPtr->SetOwner(this);
		Components.push_back(std::move(NewComponent));

		if (IsActive())
		{
			ComponentPtr->Initialize(); // Initialize the component immediately after adding it to the entity
		}

		return ComponentPtr;
	}

	template<typename T>
	T* GetComponent() const
	{
		size_t TypeID = ComponentBase::GetTypeID<T>();

		auto it = ComponentMap.find(TypeID);
		if (it != ComponentMap.end())
		{
			return dynamic_cast<T*>(it->second);
		}
		
		return nullptr;
	}

	template<typename T>
	bool RemoveComponent()
	{
		size_t TypeID = ComponentBase::GetTypeID<T>();

		auto it = ComponentMap.find(TypeID);
		if (it != ComponentMap.end())
		{
			ComponentBase* ComponentToRemove = it->second;
			ComponentToRemove->Destroy();
			ComponentMap.erase(it);

			// Asuming that each component type is unique, we can break and return after finding the first match
			for (auto CompIt = Components.begin(); CompIt != Components.end(); ++CompIt)
			{
				if (CompIt->get() == ComponentToRemove)
				{
					Components.erase(CompIt);
					return true;
				}
			}
		}

		return false;
	}

protected:

	std::string Name;
	bool bIsActive = false;
	std::vector<std::unique_ptr<ComponentBase>> Components;
	std::unordered_map<size_t, ComponentBase*> ComponentMap;

private:
	// Private constructor to enforce creation through the Scene class, which manages entity lifetimes and ensures proper initialization
	explicit Entity(const std::string& InName) : Name(InName) {}

};