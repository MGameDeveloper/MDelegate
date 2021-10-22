#pragma once

#include "DelegateInstances.h"

#include <vector>

namespace M
{
	template<typename Signiture>
	struct Delegate;

	template<typename ReturnType, typename ... Args>
	struct Delegate<ReturnType(Args...)>
	{
	private:
		using StrictDelegatePtr = std::unique_ptr<DelegateInterface<ReturnType(Args...)>>;
		
	public:
		Delegate() : IDelegate(nullptr) {}

		template<typename ClassType>
		void BindMethod(ClassType* InInstance, typename MethodPtrType<ClassType, ReturnType(Args...)>::Type InMethod)
		{
			IDelegate = std::make_unique<MethodDelegateInstance<ClassType, ReturnType(Args...)>>(InInstance, InMethod);
		}


		void BindFunction(typename FunctionPtrType<ReturnType(Args...)>::Type InFunction)
		{
			IDelegate = std::make_unique<FunctionDelegateInstance<ReturnType(Args...)>>(InFunction);
		};

		template<typename Functor>
		void BindLambda(const Functor& InLambda)
		{
			IDelegate = std::make_unique<LambdaDelegateInstance<Functor, typename MethodPtrType<Functor, ReturnType(Args...)>::Type>>(std::move(InLambda));
		}

		ReturnType Execute(Args... args)
		{
			return IDelegate->Execute(std::forward<Args>(args)...);
		}

	private:
		StrictDelegatePtr IDelegate;
	};





	template<typename Signiture>
	struct DelegateContainer;

	template<typename ReturnType, typename ... Args>
	struct DelegateContainer<ReturnType(Args...)>
	{
	private:
		using StrictDelegatePtr = std::unique_ptr<DelegateInterface<ReturnType(Args...)>>;

	public:

		template<typename ClassType>
		void AddMethod(ClassType* InInstance, typename MethodPtrType<ClassType, ReturnType(Args...)>::Type InMethod)
		{
			delegates.push_back(std::make_unique<MethodDelegateInstance<ClassType, ReturnType(Args...)>>(InInstance, InMethod));
		}

		void AddFunction(typename FunctionPtrType<ReturnType(Args...)>::Type InFunction)
		{
			delegates.push_back(std::make_unique<FunctionDelegateInstance<ReturnType(Args...)>>(InFunction));
		}

		template<typename Functor>
		void AddLambda(const Functor& InLambda)
		{
			delegates.push_back(std::make_unique<LambdaDelegateInstance<Functor, typename MethodPtrType<Functor, ReturnType(Args...)>::Type>>(std::move(InLambda)));
		}

		void Announce()
		{
			for (auto& delegate : delegates)
			{
				delegate->Execute();
			}
		}

	private:
		std::vector<StrictDelegatePtr> delegates;
	};
}