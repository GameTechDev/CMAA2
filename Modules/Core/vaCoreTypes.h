///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef max
#undef min

#pragma once

#include <cstdint>

///////////////////////////////////////////////////////////////////////////////////////////////////
// defines
#ifndef NULL
   #define NULL    (0)
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace VertexAsylum
{
   typedef int8_t               sbyte;
   typedef uint8_t              byte;

   typedef int8_t               int8;
   typedef uint8_t              uint8;

   typedef int16_t              int16;
   typedef uint16_t             uint16;

   typedef int32_t              int32;
   typedef uint32_t             uint32;
   typedef uint32_t             uint;       // for compatibility with shaders

   typedef int64_t              int64;
   typedef uint64_t             uint64;
}

// see example below
#define BITFLAG_ENUM_CLASS_HELPER( ENUMCLASSTYPE )                                                                                                              \
inline ENUMCLASSTYPE operator | ( ENUMCLASSTYPE lhs, const ENUMCLASSTYPE & rhs )                                                                                \
{                                                                                                                                                               \
    return (ENUMCLASSTYPE)( static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs ) | static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( rhs ) );            \
}                                                                                                                                                               \
                                                                                                                                                                \
inline ENUMCLASSTYPE & operator |= ( ENUMCLASSTYPE & lhs, const ENUMCLASSTYPE & rhs )                                                                           \
{                                                                                                                                                               \
    return lhs = (ENUMCLASSTYPE)( static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs ) | static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( rhs ) );      \
}                                                                                                                                                               \
                                                                                                                                                                \
inline ENUMCLASSTYPE operator & ( ENUMCLASSTYPE lhs, const ENUMCLASSTYPE & rhs )                                                                                \
{                                                                                                                                                               \
    return (ENUMCLASSTYPE)( static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs )& static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( rhs ) );             \
}                                                                                                                                                               \
                                                                                                                                                                \
inline ENUMCLASSTYPE & operator &= ( ENUMCLASSTYPE & lhs, const ENUMCLASSTYPE & rhs )                                                                           \
{                                                                                                                                                               \
    return lhs = (ENUMCLASSTYPE)( static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs )& static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( rhs ) );       \
}                                                                                                                                                               \
                                                                                                                                                                \
inline ENUMCLASSTYPE operator ~ ( ENUMCLASSTYPE lhs )                                                                                                           \
{                                                                                                                                                               \
    return (ENUMCLASSTYPE)( ~static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs ) );                                                                       \
}                                                                                                                                                               \
                                                                                                                                                                \
inline bool operator == ( ENUMCLASSTYPE lhs, const std::underlying_type_t<ENUMCLASSTYPE> & rhs )                                                                \
{                                                                                                                                                               \
    return static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs ) == rhs;                                                                                    \
}                                                                                                                                                               \
                                                                                                                                                                \
inline bool operator != ( ENUMCLASSTYPE lhs, const std::underlying_type_t<ENUMCLASSTYPE> & rhs )                                                                \
{                                                                                                                                                               \
    return static_cast<std::underlying_type_t<ENUMCLASSTYPE>>( lhs ) != rhs;                                                                                    \
}
// usage example:
// enum class vaResourceBindSupportFlags : uint32
// {
//     None                        = 0,
//     VertexBuffer                = (1 << 0),
//     IndexBuffer                 = (1 << 1),
//     ConstantBuffer              = (1 << 2),
//     ShaderResource              = (1 << 3),
//     RenderTarget                = (1 << 4),
//     DepthStencil                = (1 << 5),
//     UnorderedAccess             = (1 << 6),
// };
//
// BITFLAG_ENUM_CLASS_HELPER( vaResourceBindSupportFlags ); // <- and now you can just do "vaResourceBindSupportFlags::VertexBuffer | vaResourceBindSupportFlags::UnorderedAccess
