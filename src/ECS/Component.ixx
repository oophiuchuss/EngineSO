export module Component;

import EventBase;
import ComponentTypeIDSystem;

export class Entity;

export enum class ComponentState {
	Uninitialized,
	Initializing,
	Active,
	Destroying,
	Destroyed
};

export class ComponentBase 
{
	friend class Entity;

public:
	virtual ~ComponentBase()
	{
		if (State != ComponentState::Destroyed)
		{
			State = ComponentState::Destroying;
			OnDestroy();
			State = ComponentState::Destroyed;
		}
	}

	void Initialize()
	{
		if (State == ComponentState::Uninitialized)
		{
			State = ComponentState::Initializing;
			OnInitialize();
			State = ComponentState::Active;
		}
	}

	void Destroy()
	{
		if (State == ComponentState::Active)
		{
			State = ComponentState::Destroying;
			OnDestroy();
			State = ComponentState::Destroyed;
		}
	}

	inline Entity* GetOwner() const { return Owner; }

	template<typename T>
	static size_t GetTypeID()
	{
		return ComponentTypeIDSystem::GetTypeID<T>();
	}


protected:
	inline void SetOwner(Entity* InOwner) { Owner = InOwner; }
	
	virtual void Update(float DeltaTime) {}
	virtual void Render() {}

	virtual void OnInitialize() {}
	virtual void OnDestroy() {}

private:
	ComponentState State = ComponentState::Uninitialized;
	Entity* Owner = nullptr;

};