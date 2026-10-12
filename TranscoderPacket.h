#pragma once


// tcd - a hybid transcoder using DVSI hardware and Codec2 software
// Copyright © 2021 Thomas A. Early N7TAE

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cstring>
#include <cstdint>
#include <atomic>

#include "TCPacketDef.h"

class CTranscoderPacket
{
public:
	// constructor
	CTranscoderPacket(const STCPacket &tcp);

	// this packet's refector module;
	char GetModule() const;

	// codec
	const uint8_t *GetDStarData() const;
	const uint8_t *GetDMRData() const;
	const uint8_t *GetP25Data() const;
	const uint8_t *GetM17Data() const;
	const int16_t *GetUSRPData() const;
	void SetDStarData(const uint8_t *dstar);
	void SetDMRData(const uint8_t *dmr);
	void SetP25Data(const uint8_t *p25);
	void SetM17Data(const uint8_t *m17);
	void SetUSRPData(const int16_t *usrp);
	void SetAudioSamples(const int16_t *samples, bool swap);

	// audio
	const int16_t *GetAudioSamples() const;

	// state of packet
	ECodecType GetCodecIn() const;
	uint16_t GetStreamId() const;
	uint32_t GetSequence() const;
	bool IsLast() const;
	bool IsSecond() const;
	bool DStarIsSet() const;
	bool DMRIsSet() const;
	bool P25IsSet() const;
	bool M17IsSet() const;
	bool USRPIsSet() const;
	bool AllCodecsAreSet();
	void Sent();
	bool HasNotBeenSent() const;

	// the all important packet
	const STCPacket *GetTCPacket() const;

private:
	STCPacket tcpacket;
	int16_t audio[160];
	std::atomic_bool dstar_set, dmr_set, p25_set, m17_set, usrp_set, not_sent;
};
