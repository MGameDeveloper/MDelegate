#pragma once

namespace M
{
	template<typename ClassType, typename Signiture>
	struct MethodPtrType;

	template<typename ClassType, typename ReturnType, typename ... Args>
	struct MethodPtrType<ClassType, ReturnType(Args...)>
	{
		typedef ReturnType(ClassType::* Type)(Args...);
	};

	template<typename Signiture>
	struct FunctionPtrType;

	template<typename ReturnType, typename ... Args>
	struct FunctionPtrType<ReturnType(Args...)>
	{
		typedef ReturnType(*Type)(Args...);
	};
}
