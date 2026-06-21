export module EventBase;

export enum class EventCategory
{
	None = 0,
	Application = 1 << 0,
	Input = 1 << 1,
	Keyboard = 1 << 2,
	Mouse = 1 << 3,
	MouseButton = 1 << 4,
	Window = 1 << 5,
	Scene = 1 << 6
};

export enum class KeyAction
{
	Press,
	Release,
	Repeat
};

export class EventBase
{
public:
	virtual ~EventBase() = default;

	virtual const char* GetType() const = 0;

	virtual EventBase* Clone() const = 0;

	virtual int GetCategoryFlags() const = 0;

	bool IsInCategory(EventCategory Category) const
	{
		return GetCategoryFlags() & static_cast<int>(Category);
	}
};