#include "conf.h"

#include <stdexcept>
#include <string.h>
#include <ctype.h>

Conf::Conf()
	: subkeys()
	, key()
{
}

Conf::Conf(const std::string& key)
	: subkeys()
	, key(key)
{
}

Conf::Conf(const std::string& key, const std::vector<Conf>& subkeys)
	: subkeys(subkeys)
	, key(key)
{
}

//! Parse configuration from string.
/*!
 * This function must process one token at a time, and returns after each
 * key, or at the end of the string if there are no more tokens.
 */
const char* Conf::parse_token(const char* head)
{
	std::string k ("");
	bool read_key = false;
	while(*head && (*head != '}'))
	{
		// break sequences
		const char* tail = strpbrk(head, " \t\r\n\":,;{}");
		if(!tail)
			tail = head+strlen(head);

		if(tail > head)
		{
			// read tag
			if(read_key)
				break;
			read_key = true;
			k = std::string(head, tail-head);
			head = tail;
		}
		else if(*head == '"')
		{
			// read quote-enclosed tag
			if(read_key)
				break;
			read_key = true;
			while(*++head)
			{
				char c = *head;
				if(c == '\\')
				{
					c = *++head;
					if(c == 'n')
						k += "\n";
					else if(c == '\t')
						k += "\t";
					else if(c != '\r')
						k.push_back(c);
					continue;
				}
				else if(*head == '"')
				{
					head++;
					break;
				}
				k.push_back(c);
			}
		}
		else if(*head == ':')
		{
			// assign subkey
			Conf subkey(k);
			head = subkey.parse_token(++head);
			subkeys.push_back(subkey);
			return head;
		}
		else if(*head == ',')
		{
			// end of key (use to write empty keys)
			subkeys.push_back(Conf(k));
			return head+1;
		}
		else if(*head == ';')
		{
			// comment
			while((*head != '\n' && *head != '\r') && *++head);
		}
		else if(*head == '{')
		{
			// begin list of subkeys
			Conf subkey(k);
			++head;
			while(*head && *head != '}')
				head = subkey.parse_token(head);
			if(*head++ != '}')
				throw std::runtime_error("missing }");
			subkeys.push_back(subkey);
			return head;
		}
		else if(*head != '}')
		{
			// whitespace
			// end of list of subkeys
			while(*++head && isspace(*head));
		}
	}
	if(read_key)
		subkeys.push_back(Conf(k));
	return head;
}

//! Create a Conf from a string.
/*! Syntax is as follows:
 *
 *		level0 {
 *			level1 { level2 level2 level2 }
 *			level1: level2
 *			level1
 *			level1,
 *			,
 *		}
 *
 */
Conf Conf::from_string(const char* str)
{
	Conf conf("");
	while(*str)
		str = conf.parse_token(str);
	return conf;
}
