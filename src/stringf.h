//! \file stringf.h
//! String formatting helper functions.
#ifndef STRINGF_H
#define STRINGF_H
#include <string>

//! Return a printf-formatted std::string.
/*!
 * \param format Format.
 * \param ... Data parameters.
 */
std::string stringf(const char* format, ...);

#endif
