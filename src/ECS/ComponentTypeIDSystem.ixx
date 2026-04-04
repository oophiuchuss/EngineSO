export module ComponentTypeIDSystem;

export class ComponentTypeIDSystem
{
public:
	template<typename T>
	static size_t GetTypeID()
	{
		static const size_t TypeID = NextTypeID++;
		return TypeID;
	}

private:
	static size_t NextTypeID;
};

size_t ComponentTypeIDSystem::NextTypeID = 0;
