export module Component;

import EventBase;

export class Entity;

export class ComponentBase 
{
public:
	virtual ~ComponentBase() = default;

	virtual void Initialize() {}
	virtual void Update(float DeltaTime) {}
	virtual void Render() {}
	virtual void OnEvent(const EventBase& Event) {}


	void SetOwner(Entity* InOwner) { Owner = InOwner; }
	Entity* GetOwner() const { return Owner; }

private:
	Entity* Owner = nullptr;

};