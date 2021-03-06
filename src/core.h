/*! \file src/core.h
 * \brief ctrmml core include file.
 *
 * Includes forward declarations of the core classes and
 * container data structures used by ctrmml.
 */
#ifndef CORE_H
#define CORE_H

#define CTRMML_VERSION "0.3"

#include <vector>
#include <map>
#include <memory>

class Track;
class Song;
class Input;
class InputRef;
class VGM_Writer;
class VGM_Interface;
class Player;
class Driver;
class Platform;

typedef std::vector<std::string> Tag;
typedef std::map<std::string,Tag> Tag_Map;
typedef std::map<uint16_t,Track> Track_Map;
typedef std::shared_ptr<InputRef> InputRefPtr;

#endif
