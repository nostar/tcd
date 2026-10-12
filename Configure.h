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

#pragma once

#include <cstdint>
#include <string>
#include <regex>

enum class EGainType { dmrin, dmrout, dstarin, dstarout, usrptx, usrprx };

#define IS_TRUE(a) ((a)=='t' || (a)=='T' || (a)=='1')

class CConfigure
{
public:
	bool ReadData(const std::string &path);
	int GetGain(EGainType gt) const;
	std::string GetTCMods(void) const { return tcmods; }
	std::string GetAddress(void) const { return address; }
	unsigned GetPort(void) const { return port; }

private:
	// CFGDATA data;
	std::string tcmods, address;
	uint16_t port;
	int dstar_in, dstar_out, dmr_in, dmr_out, usrp_tx, usrp_rx;

	int getSigned(const std::string &key, const std::string &value) const;
	void badParam(const std::string &key) const;
};
