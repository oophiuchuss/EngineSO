module ResourceHandle;

template<typename T>
T* ResourceHandle<T>::Get() const
{
	if (!ResourceManager)
	{
		return nullptr;
	}

	// TODO: implement get resource by id
	return ResourceManager->GetResource<T>(ResourceID);
}