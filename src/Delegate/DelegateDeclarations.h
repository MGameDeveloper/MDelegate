#pragma once

#include "DelegateImpl.h"

#define DECLARE_NORET_DELEGATE(Name, ...) typedef M::Delegate<void(__VA_ARGS__)> Name
#define DECLARE_RET_DELEGATE(ReturnType, Name, ...) typedef M::Delegate<ReturnType(__VA_ARGS__)> Name

#define DECLARE_DELEGATE_CONTAINER(Name, ...) typedef M::DelegateContainer<void(__VA_ARGS__)> Name