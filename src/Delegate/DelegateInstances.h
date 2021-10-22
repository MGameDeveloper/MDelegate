#pragma once

#include "DelegateSigniture.h"
#include "DelegateInterface.h"

#include <memory>
#include <utility>

namespace M
{
	template<typename ClassType, typename Signiture>
	struct MethodDelegateInstance;

	template<typename ClassType, typename ReturnType, typename ... Args>
	struct MethodDelegateInstance<ClassType, ReturnType(Args...)> : public DelegateInterface<ReturnType(Args...)>
	{
	public:
		using MethodPtr = typename MethodPtrType<ClassType, ReturnType(Args...)>::Type;

		MethodDelegateInstance(ClassType* InInstance, MethodPtr InMethod) : Instance(InInstance), Method(InMethod)
		{
		}

		ReturnType Execute(Args... args) override
		{
			return (Instance->*Method)(std::forward<Args>(args)...);
		}

	private:
		ClassType*  Instance;
		MethodPtr   Method;
	};





	template<typename Signiture>
	struct FunctionDelegateInstance;

	template<typename ReturnType, typename ... Args>
	struct FunctionDelegateInstance<ReturnType(Args...)> : public DelegateInterface<ReturnType(Args...)>
	{
	public:
		using FunctionPtr = typename FunctionPtrType<ReturnType(Args...)>::Type;

		FunctionDelegateInstance(FunctionPtr InFunction) : Function(InFunction)
		{
		}

		ReturnType Execute(Args... args) override
		{
			return (*Function)(std::forward<Args>(args)...);
		}

	private:
		FunctionPtr Function;
	};





	template<typename Functor, typename Signiture>
	struct LambdaDelegateInstance;

	template<typename Functor, typename ReturnType, typename ... Args>
	struct LambdaDelegateInstance<Functor, ReturnType(Functor::*)(Args...)> : public DelegateInterface<ReturnType(Args...)>
	{
		using StrictFunctorPtr = std::unique_ptr<Functor>;

		LambdaDelegateInstance(const Functor& InLambda) : Lambda(std::make_unique<Functor>(std::move(InLambda)))
		{
		}

		ReturnType Execute(Args... args) override
		{
			return (*Lambda.get())(std::forward<Args>(args)...);
		}

	private:
		StrictFunctorPtr Lambda;
	};
};