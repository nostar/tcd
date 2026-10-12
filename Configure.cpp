/*
 *   Copyright (c) 2023 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <regex>
#include "Configure.h"

// ini file keywords
#define USRPTXGAIN     "UsrpTxGain"
#define USRPRXGAIN     "UsrpRxGain"
#define DMRGAININ      "DmrYsfGainIn"
#define DMRGAINOUT     "DmrYsfGainOut"
#define DSTARGAININ    "DStarGainIn"
#define DSTARGAINOUT   "DStarGainOut"
#define MODULES        "Modules"
#define SERVERADDRESS  "ServerAddress"
#define PORT           "Port"

static inline void split(const std::string &s, char delim, std::vector<std::string> &v)
{
	std::istringstream iss(s);
	std::string item;
	while (std::getline(iss, item, delim))
		v.push_back(item);
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

bool CConfigure::ReadData(const std::string &path)
// returns true on failure
{
	std::regex IPv4RegEx = std::regex("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\\.){3,3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]){1,1}$", std::regex::extended);
	std::regex IPv6RegEx = std::regex("^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}(:[0-9a-fA-F]{1,4}){1,1}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|([0-9a-fA-F]{1,4}:){1,1}(:[0-9a-fA-F]{1,4}){1,6}|:((:[0-9a-fA-F]{1,4}){1,7}|:))$", std::regex::extended);

	std::string modstmp, porttmp;

	std::ifstream cfgfile(path.c_str(), std::ifstream::in);
	if (! cfgfile.is_open()) {
		std::cerr << "ERROR: '" << path << "' was not found!" << std::endl;
		return true;
	}

	std::string line;
	while (std::getline(cfgfile, line))
	{
		trim(line);
		if (3 > line.size())
			continue;	// can't be anything
		if ('#' == line.at(0))
			continue;	// skip comments

		std::vector<std::string> tokens;
		split(line, '=', tokens);
		// check value for end-of-line comment
		if (2 > tokens.size())
		{
			std::cout << "WARNING: '" << line << "' does not contain an equal sign, skipping" << std::endl;
			continue;
		}
		auto pos = tokens[1].find('#');
		if (std::string::npos != pos)
		{
			tokens[1].assign(tokens[1].substr(0, pos));
			rtrim(tokens[1]); // whitespace between the value and the end-of-line comment
		}
		// trim whitespace from around the '='
		rtrim(tokens[0]);
		ltrim(tokens[1]);
		const std::string key(tokens[0]);
		const std::string value(tokens[1]);
		if (key.empty() || value.empty())
		{
			std::cout << "WARNING: missing key or value: '" << line << "'" << std::endl;
			continue;
		}
		if (0 == key.compare(SERVERADDRESS))
			address.assign(value);
		else if (0 == key.compare(PORT))
			porttmp.assign(value);
		else if (0 == key.compare(MODULES))
			modstmp.assign(value);
		else if (0 == key.compare(DSTARGAININ))
			dstar_in = getSigned(key, value);
		else if (0 == key.compare(DSTARGAINOUT))
			dstar_out = getSigned(key, value);
		else if (0 == key.compare(DMRGAININ))
			dmr_in = getSigned(key, value);
		else if (0 == key.compare(DMRGAINOUT))
			dmr_out = getSigned(key, value);
		else if (0 == key.compare(USRPTXGAIN))
			usrp_tx = getSigned(key, value);
		else if (0 == key.compare(USRPRXGAIN))
			usrp_rx = getSigned(key, value);
		else
			badParam(key);
	}
	cfgfile.close();

	for (auto c : modstmp)
	{
		if (isalpha(c))
		{
			if (islower(c))
				c = toupper(c);
			if (std::string::npos == tcmods.find(c))
				tcmods.append(1, c);
		}
	}
	if (tcmods.empty())
	{
		std::cerr << "ERROR: no identifable module letters in '" << modstmp << "'. Halt." << std::endl;
		return true;
	}

	if (! std::regex_match(address, IPv4RegEx) && ! std::regex_match(address, IPv6RegEx))
	{
		std::cerr << "ERROR: '" << address << "' is malformed, Halt." << std::endl;
		return true;
	}

	port = std::strtoul(porttmp.c_str(), nullptr, 10);
	if (port < 1025 || port > 49000)
	{
		std::cerr << "ERROR: Port '" << porttmp << "' must be between >1024 and <49000. Halt." << std::endl;
		return true;
	}

	std::cout << MODULES << " = " << tcmods << std::endl;
	std::cout << SERVERADDRESS << " = " << address << std::endl;
	std::cout << PORT << " = " << port << std::endl;
	std::cout << DSTARGAININ << " = " << dstar_in << std::endl;
	std::cout << DSTARGAINOUT << " = " << dstar_out << std::endl;
	std::cout << DMRGAININ << " = " << dmr_in << std::endl;
	std::cout << DMRGAINOUT << " = " << dmr_out << std::endl;
	std::cout << USRPTXGAIN << " = " << usrp_tx << std::endl;
	std::cout << USRPRXGAIN << " = " << usrp_rx << std::endl;

	return false;
}

int CConfigure::getSigned(const std::string &key, const std::string &value) const
{
	auto i = std::stoi(value.c_str());
	if (i < -24)
	{
		std::cout << "WARNING: " << key << " = " << value << " is too low. Limit to -24!" << std::endl;
		i = -24;
	}
	else if (i > 24)
	{
		std::cout << "WARNING: " << key << " = " << value << " is too high. Limit to 24!" << std::endl;
		i = 24;
	}
	return i;
}

void CConfigure::badParam(const std::string &key) const
{
	std::cout << "WARNING: Unexpected parameter: '" << key << "'" << std::endl;
}

int CConfigure::GetGain(EGainType gt) const
{
	switch (gt)
	{
		case EGainType::dmrin:    return dmr_in;
		case EGainType::dmrout:   return dmr_out;
		case EGainType::dstarin:  return dstar_in;
		case EGainType::dstarout: return dstar_out;
		case EGainType::usrptx:   return usrp_tx;
		case EGainType::usrprx:   return usrp_rx;
		default:                  return 0;
	}
}
