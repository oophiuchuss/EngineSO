#pragma once

// Macto to help define event type
#define DEFINE_EVENT_TYPE(type, CategoryFlags) \
	static const char* GetStaticType() { return #type; } \
	virtual const char* GetType() const override { return GetStaticType(); } \
	virtual EventBase* Clone() const override { return new type(*this); } \
	virtual int GetCategoryFlags() const override { return CategoryFlags; } \