#pragma once

#include <cxxabi.h>
#include <string>
#include <typeinfo>
#include <memory>

namespace Sage
{

template<typename T>
std::string DemangleTypeName(const T& type)
{
    int status{ -1 };
    const std::type_info& typeInfo{ typeid(type) };
    std::unique_ptr<char, decltype(&std::free)> name{
           abi::__cxa_demangle(typeInfo.name(), nullptr, nullptr, &status),
           std::free
    };

    if (status == 0 and name != nullptr)
    {
        return std::string{ name.get() };
    }
    else
    {
        return std::string{ typeInfo.name() };
    }
}

} // namespace Sage
