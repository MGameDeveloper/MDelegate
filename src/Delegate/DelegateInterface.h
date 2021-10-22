#pragma once

namespace M
{
	template<typename Signiture>
	struct DelegateInterface;

	template<typename ReturnType, typename ... Args>
	struct DelegateInterface<ReturnType(Args...)>
	{
		virtual ReturnType Execute(Args...) = 0;
	};
}

