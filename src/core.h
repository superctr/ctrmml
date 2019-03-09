//! \file core.h
#ifndef CORE_H
#define CORE_H

#include <vector>
#include <map>
#include <memory>

class Track;
class Song;
class Input;
class InputRef;

typedef std::vector<std::string> Tag;
typedef std::map<std::string,Tag> Tag_Map;
typedef std::map<uint16_t,Track> Track_Map;
typedef std::shared_ptr<InputRef> InputRefPtr;

#endif
