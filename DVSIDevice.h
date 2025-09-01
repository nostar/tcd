#pragma once

// tcd - a hybid transcoder using DVSI hardware and Codec2 software
// Copyright © 2022 Thomas A. Early N7TAE

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

#include <netinet/in.h>
#include <string>
#include <sstream>
#include <future>
#include <atomic>
#include <ftd2xx.h>

#include "PacketQueue.h"
#include "DVSIPacket.h"

class CDVDevice
{
public:
	CDVDevice(Encoding t);
	virtual ~CDVDevice();

	bool OpenDevice(const std::string &serialno, const std::string &desc, Edvtype dvtype, int8_t in_gain, int8_t out_gain);
	void Start();
	void CloseDevice();
	void AddPacket(const std::shared_ptr<CTranscoderPacket> packet);
	std::string GetProductID() { return productid; }

protected:
	const Encoding type;
	FT_HANDLE ftHandle;
	std::atomic<unsigned int> buffer_depth;
	std::atomic<bool> keep_running;
	CPacketQueue input_queue;
	std::future<void> feedFuture, readFuture;
	std::string description, productid;

	bool DiscoverFtdiDevices();
	bool ConfigureVocoder(uint8_t pkt_ch, Encoding type, int8_t in_gain, int8_t out_gain);
	bool checkResponse(SDV_Packet &responsePacket, uint8_t response) const;
	bool GetResponse(SDV_Packet &packet);
	bool InitDevice();
	void FeedDevice();
	void ReadDevice();
	void FTDI_Error(const char *where, FT_STATUS status) const;
	void dump(const char *title, const void *data, int length) const;

	// pure virtual methods unique to the device type
	virtual void PushWaitingPacket(unsigned int channel, std::shared_ptr<CTranscoderPacket> packet) = 0;
	virtual std::shared_ptr<CTranscoderPacket> PopWaitingPacket(unsigned int channel) = 0;
	virtual void ProcessPacket(const SDV_Packet &p) = 0;
	virtual bool SendAudio(const uint8_t channel, const int16_t *audio) const = 0;
	virtual bool SendData(const uint8_t channel, const uint8_t *data) const = 0;
};
