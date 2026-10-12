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

#include <arpa/inet.h>
#include <iostream>

#include "TranscoderPacket.h"

CTranscoderPacket::CTranscoderPacket(const STCPacket &tcp) : dstar_set(false), dmr_set(false), p25_set(false), m17_set(false), usrp_set(false), not_sent(true)
{
	tcpacket.module = tcp.module;
	tcpacket.is_last = tcp.is_last;
	tcpacket.streamid = tcp.streamid;
	tcpacket.codec_in = tcp.codec_in;
	tcpacket.sequence = tcp.sequence;
	switch (tcp.codec_in)
	{
	case ECodecType::dstar:
		SetDStarData(tcp.dstar);
		break;
	case ECodecType::dmr:
		SetDMRData(tcp.dmr);
		break;
	case ECodecType::p25:
		SetP25Data(tcp.p25);
		break;
	case ECodecType::usrp:
		SetUSRPData(tcp.usrp);
		break;
	case ECodecType::c2_1600:
	case ECodecType::c2_3200:
		SetM17Data(tcp.m17);
		break;
	default:
		std::cerr << "Trying to allocate CTranscoderPacket with an unknown codec type!" << std::endl;
		break;
	}
}

char CTranscoderPacket::GetModule() const
{
	return tcpacket.module;
}

const uint8_t *CTranscoderPacket::GetDStarData() const
{
	return tcpacket.dstar;
}

const uint8_t *CTranscoderPacket::GetDMRData() const
{
	return tcpacket.dmr;
}

const uint8_t *CTranscoderPacket::GetP25Data() const
{
	return tcpacket.p25;
}

const int16_t *CTranscoderPacket::GetUSRPData() const
{
	return tcpacket.usrp;
}

const uint8_t *CTranscoderPacket::GetM17Data() const
{
	return tcpacket.m17;
}

const STCPacket *CTranscoderPacket::GetTCPacket() const
{
	return &tcpacket;
}

void CTranscoderPacket::SetM17Data(const uint8_t *data)
{
	memcpy(tcpacket.m17, data, 16);
	m17_set = true;
}

void CTranscoderPacket::SetDStarData(const uint8_t *dstar)
{
	memcpy(tcpacket.dstar, dstar, 9);
	dstar_set = true;
}

void CTranscoderPacket::SetDMRData(const uint8_t *dmr)
{
	memcpy(tcpacket.dmr, dmr, 9);
	dmr_set = true;
}

void CTranscoderPacket::SetP25Data(const uint8_t *p25)
{
	memcpy(tcpacket.p25, p25, 11);
	p25_set = true;
}

void CTranscoderPacket::SetUSRPData(const int16_t *usrp)
{
	for(int i = 0; i < 160; ++i){
		tcpacket.usrp[i] = usrp[i];
	}
	usrp_set = true;
}

void CTranscoderPacket::SetAudioSamples(const int16_t *sample, bool swap)
{
	for (unsigned int i=0; i<160; i++)
		audio[i] = swap ? ntohs(sample[i]) : sample[i];
}

const int16_t *CTranscoderPacket::GetAudioSamples() const
{
	return audio;
}

ECodecType CTranscoderPacket::GetCodecIn() const
{
	return tcpacket.codec_in;
}

uint16_t CTranscoderPacket::GetStreamId() const
{
	return tcpacket.streamid;
}

uint32_t CTranscoderPacket::GetSequence() const
{
	return tcpacket.sequence;
}

bool CTranscoderPacket::IsLast() const
{
	return tcpacket.is_last;
}

bool CTranscoderPacket::IsSecond() const
{
	return (1 == tcpacket.sequence % 2);
}

bool CTranscoderPacket::DStarIsSet() const
{
	return dstar_set;
}

bool CTranscoderPacket::DMRIsSet() const
{
	return dmr_set;
}

bool CTranscoderPacket::P25IsSet() const
{
	return p25_set;
}

bool CTranscoderPacket::USRPIsSet() const
{
	return usrp_set;
}

bool CTranscoderPacket::M17IsSet() const
{
	return m17_set;
}

bool CTranscoderPacket::AllCodecsAreSet()
{
#ifdef SW_MODES_ONLY
	dstar_set = true;
#ifndef USE_SW_AMBE2
	dmr_set = true;
#endif
#endif
	return (dstar_set && dmr_set && m17_set && p25_set && usrp_set);
}

void CTranscoderPacket::Sent()
{
	not_sent = false;
}

bool CTranscoderPacket::HasNotBeenSent() const
{
	return not_sent;
}
