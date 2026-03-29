module;

#include <string>
#include <vector>
#include <memory>

export module Entity;

import Component;

export class Entity 
{
public:
	explicit Entity(const std::string& InName) : Name(InName) {}
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

		//Create a new component and add it to the entity's component list
		auto NewComponent = std::make_unique<T>(std::forward<Args>(args)...);
		T* ComponentPtr = NewComponent.get();
		ComponentPtr->SetOwner(this);
		Components.push_back(std::move(NewComponent));
		return ComponentPtr;
	}

	template<typename T>
	T* GetComponent() const
	{
		for (auto& CurComponent : Components)
		{
			if (T* result = dynamic_cast<T*>(CurComponent.get()))
			{
				return result;
			}
		}

		return nullptr;
	}

	template<typename T>
	bool RemoveComponent()
	{
		for (auto& it = Components.begin(); it != Components.end(); it++)
		{
			if (dynamic_cast<T*>(it->get()))
			{
				Components.erase(it);
				return true;
			}
		}

		return false;
	}

protected:

	std::string Name;
	bool bIsActive = true;
	std::vector<std::unique_ptr<ComponentBase>> Components;

};