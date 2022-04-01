// Copyright (C) 2022 Kyle Sylvestre
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

//
// STL Switcheroo
//

extern "C" {
    void *dlmalloc(size_t);
    void dlfree(void*);
}

template <class T>
struct DL_Allocator
{
    // the ugly stuff
    typedef T value_type;
    typedef char char_type;

    DL_Allocator() noexcept {} //default ctor not required by C++ Standard Library
    template<class U> DL_Allocator(const DL_Allocator<U>&) noexcept {}
    template<class U> bool operator==(const DL_Allocator<U>&) const noexcept
    {
        return true;
    }
    template<class U> bool operator!=(const DL_Allocator<U>&) const noexcept
    {
        return false;
    }

    // the good stuff
    T* allocate(const size_t n)
    {
        return (T *)dlmalloc(n * sizeof(T));
    }
    void deallocate(T* const p, size_t)
    {
        dlfree(p);
    }
};

template <typename T>
using Vector = std::vector<T, DL_Allocator<T>>;

using String = std::basic_string<char, std::char_traits<char>, DL_Allocator<char>>;
