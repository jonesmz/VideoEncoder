/*
 * Copyright © 2021 Michael Jones
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef STRING_CONCAT_H_6D89C3EC_FB7D_4D6D_8442_A65184719E66
#define STRING_CONCAT_H_6D89C3EC_FB7D_4D6D_8442_A65184719E66

#include <string>
#include <algorithm>

/**
 * Gets a pointer to the underlying data of a string type object
 *
 * If the parameter is a char *, or a char[], then this
 * function simply returns the parameter.
 *
 * If the parameter is neither a char* or char[], then return
 * the result of calling the parameters .data() function.
 *
 * If the parameter has no .data() function, a compile error results.
 *
 * The returned pointer provides no guarantee on the string being nul
 * terminated. Always use string_data() with string_size() to ensure
 * no out of bounds data access occurs.
 */
template<class STRING_T>
constexpr auto string_data(STRING_T && str)
{
    if constexpr(std::is_pointer_v<std::remove_reference_t<STRING_T>>)
    {
        return std::forward<STRING_T>(str);
    }
    else
    {
        return std::data(std::forward<STRING_T>(str));
    }
} // string_data()


//-----------------------------------------------------------------------------

/**
 * Gets the size of the string provided as the parameter
 *
 * If the parameter is a char *, or a char[], then this
 * function calls the string length function, and therefore
 * the provided string must be nul-terminated.
 *
 * (Future enhancement will allow the size of char[] parameters
 *  to be statically determined at compile time)
 *
 * If the parameter is neither a char* or char[], then return
 * the result of calling std::size().
 */
template<class STRING_T>
constexpr size_t string_size(STRING_T const& str)
{
    if constexpr(std::is_pointer_v<STRING_T>)
    {
        return std::char_traits<std::remove_pointer_t<STRING_T>>::length(str);
    }
    else if constexpr(std::is_array_v<STRING_T>)
    {
        // TODO: Throw if the array contains an embedded nul character.
        // TODO: Return the static size based on template deduction.
        return std::char_traits<std::remove_pointer_t<std::decay_t<STRING_T>>>::length(str);
    }
    else
    {
        return std::size(str);
    }
} // string_size()


//-----------------------------------------------------------------------------

/**
 * Effecienctly appends multiple strings together using, at most, a single
 * allocation to reserve the necessary space, via the "reserve" function call.
 *
 * \arg dest   -- The destination where all of the appended strings will be stored
 * \arg others -- The strings to append into the destination.
 *
 * \returns Nothing.
 */
template<typename DESTINATION_T, typename ... STRS_T>
constexpr void string_append(DESTINATION_T & dest, STRS_T const& ... others)
{
    std::size_t const curPos = string_size(dest);

    // C++17 fold for summation
    dest.resize( ( curPos + ... + string_size(others) ) );

    auto * pDest = string_data(dest) + curPos;

    // C++17 fold for function calls.
    ( (pDest = std::copy_n(string_data(others), string_size(others), pDest) ), ... );
} // string_append()


//-----------------------------------------------------------------------------

/**
 * Concatinates all of the provided string-like objects into a single
 * string of the type RESULT_T. RESULT_T must have reserve() and append()
 * functions.
 *
 * This operation uses a single allocation to acquire storage, via the "reserve" function call.
 *
 * \arg string -- the strings to concatinate together.
 *
 * \returns A RESULT_T object containing the strings concatinated together.
 */
template<typename RESULT_T, typename ... STRS_T>
constexpr RESULT_T basic_string_concat(STRS_T const& ... others)
{
    RESULT_T dest;

    // C++17 fold for summation
    dest.resize( ( 0 + ... + string_size(others) ) );

    auto * pDest = string_data(dest);

    // C++17 fold for function calls.
    ( (pDest = std::copy_n(string_data(others), string_size(others), pDest) ), ... );

    return dest;
} // basic_string_concat()


//-----------------------------------------------------------------------------

/**
 * Concatinates all of the provided string-like objects into a single std::string.
 *
 * This operation uses a single allocation to acquire storage, via the "reserve" function call.
 *
 * \arg string -- the strings to concatinate together.
 *
 * \returns A std::string object containing the strings concatinated together.
 */
template<typename ... STRS_T>
constexpr decltype(auto) string_concat(STRS_T  const& ... strs)
{
    return basic_string_concat<std::string>(strs...);;
} // string_concat()

#endif // STRING_CONCAT_H_6D89C3EC_FB7D_4D6D_8442_A65184719E66
