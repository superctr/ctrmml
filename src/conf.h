/*! \file src/conf.h
 * \brief Configuration file handling.
 *
 * Handles a simple hierarchial configuration file format.
 */
#ifndef CONF_H
#define CONF_H
#include <string>
#include <memory>
#include <vector>

class Conf;

//! Configuration object.
class Conf
{
public:
	Conf();
	Conf(const std::string& key);
	Conf(const std::string& key, const std::vector<Conf>& subkeys);

	Conf& get_subkey(const std::string& key);
	static Conf from_string(const char* str);

	std::vector<Conf> subkeys;
	std::string key;

private:
	const char* parse_token(const char* head);
};
#endif
