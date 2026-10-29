// tcd - a hybid transcoder using DVSI hardware and Codec2 software
// Copyright © 2021 Thomas A. Early N7TAE
// Copyright © 2021 Doug McLain AD8DP

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

#include <unistd.h>
#include <sys/select.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <thread>
#include <queue>
#include "TranscoderPacket.h"
#include "Controller.h"
#include "Configure.h"

extern CConfigure g_Conf;

int32_t CController::calcNumerator(int32_t db) const
{
	float num = 256.0f * powf(10.0f, (float(db)/20.0f));

	return int32_t(roundf(num));
}

CController::CController() : keep_running(true) {}

bool CController::Start()
{
	usrp_rx_num = calcNumerator(g_Conf.GetGain(EGainType::usrprx));
	usrp_tx_num = calcNumerator(g_Conf.GetGain(EGainType::usrptx));

	if (InitVocoders() || tcClient.Open(g_Conf.GetAddress(), g_Conf.GetTCMods(), g_Conf.GetPort()))
	{
		keep_running = false;
		return true;
	}
	reflectorFuture = std::async(std::launch::async, &CController::ReadReflectorThread, this);
	c2Future        = std::async(std::launch::async, &CController::ProcessC2Thread,     this);
	imbeFuture      = std::async(std::launch::async, &CController::ProcessIMBEThread,   this);
	ambeFuture      = std::async(std::launch::async, &CController::ProcessAMBEThread,   this);
	usrpFuture      = std::async(std::launch::async, &CController::ProcessUSRPThread,   this);
	ambe2Future     = std::async(std::launch::async, &CController::ProcessAMBE2Thread,  this);
	return false;
}

void CController::Stop()
{
	keep_running = false;

	if (reflectorFuture.valid())
		reflectorFuture.get();
	if (c2Future.valid())
		c2Future.get();

	tcClient.Close();
}

bool CController::InitVocoders()
{
	// M17 "devices", one for each module
	const std::string modules(g_Conf.GetTCMods());
	for ( auto c : modules)
	{
		c2_16[c] = std::unique_ptr<CCodec2>(new CCodec2(false));
		c2_32[c] = std::unique_ptr<CCodec2>(new CCodec2(true));
	}
	
	ambe_in_num = calcNumerator(g_Conf.GetGain(EGainType::dmrin));
	ambe_out_num = calcNumerator(g_Conf.GetGain(EGainType::dmrout));
	dstar_in_num = calcNumerator(g_Conf.GetGain(EGainType::dstarin));
	dstar_out_num = calcNumerator(g_Conf.GetGain(EGainType::dstarout));
	return false;
}

// Encapsulate the incoming STCPacket into a CTranscoderPacket and push it into the appropriate queue
// based on packet's codec_in.
void CController::ReadReflectorThread()
{
	while (keep_running)
	{
		// preemptively check the connection(s)...
		tcClient.ReConnect();

		std::queue<std::unique_ptr<STCPacket>> queue;
		// wait up to 100 ms to read something on the unix port
		tcClient.Receive(queue, 100);
		while (! queue.empty())
		{
			// create a shared pointer to a new packet
			// there is only one CTranscoderPacket created for each new STCPacket received from the reflector
			auto packet = std::make_shared<CTranscoderPacket>(*queue.front());
			queue.pop();
			switch (packet->GetCodecIn())
			{
			case ECodecType::dstar:
				ambe_queue.push(packet);
				break;
			case ECodecType::dmr:
				ambe2_queue.push(packet);
				break;
			case ECodecType::p25:
				imbe_queue.push(packet);
				break;
			case ECodecType::usrp:
				usrp_queue.push(packet);
				break;
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
				codec2_queue.push(packet);
				break;
			default:
				Dump(packet, "ERROR: Received a reflector packet with unknown Codec:");
				break;
			}
		}
	}
}

// This is only called when codec_in was dstar or dmr. Obviously, the incoming
// ambe packet was already decoded to audio.
// This might complete the packet. If so, send it back to the reflector
void CController::AudiotoCodec2(std::shared_ptr<CTranscoderPacket> packet)
{
	// the second half is silent in case this is frame is last.
	uint8_t m17data[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x01, 0x43, 0x09, 0xe4, 0x9c, 0x08, 0x21 };
	const auto m = packet->GetModule();
	if (packet->IsSecond())
	{
		// get the first half from the store
		memcpy(m17data, data_store[packet->GetModule()], 8);
		// and then calculate the second half
		c2_32[m]->codec2_encode(m17data+8, packet->GetAudioSamples());
		packet->SetM17Data(m17data);
	}
	else /* the packet is first */
	{
		// calculate the first half...
		c2_32[m]->codec2_encode(m17data, packet->GetAudioSamples());
		// and then copy the calculated data to the data_store
		memcpy(data_store[packet->GetModule()], m17data, 8);
		// set the m17_is_set flag if this is the last packet
		packet->SetM17Data(m17data);
	}

	// we might be all done...
	send_mux.lock();
	if (packet->AllCodecsAreSet() && packet->HasNotBeenSent()) SendToReflector(packet);
	send_mux.unlock();
}

// The original incoming coded was M17, so we will calculate the audio and then
// push the packet onto both the dstar and the dmr queue.
void CController::Codec2toAudio(std::shared_ptr<CTranscoderPacket> packet)
{
	uint8_t ambe[9];
	uint8_t imbe[11];

	if (packet->IsSecond())
	{
		if (packet->GetCodecIn() == ECodecType::c2_1600)
		{
			// we've already calculated the audio in the previous packet
			// copy the audio from local audio store
			packet->SetAudioSamples(audio_store[packet->GetModule()], false);
		}
		else /* codec_in is ECodecType::c2_3200 */
		{
			int16_t tmp[160];
			// decode the second 8 data bytes
			// and put it in the packet
			c2_32[packet->GetModule()]->codec2_decode(tmp, packet->GetM17Data()+8);
			packet->SetAudioSamples(tmp, false);
		}
	}
	else /* it's a "first packet" */
	{
		const auto m = packet->GetModule();
		if (packet->GetCodecIn() == ECodecType::c2_1600)
		{
			// c2_1600 encodes 40 ms of audio, 320 points, so...
			// we need some temporary audio storage for decoding c2_1600:
			int16_t tmp[320];
			// decode it into the temporary storage
			c2_16[m]->codec2_decode(tmp, packet->GetM17Data()); // 8 bytes input produces 320 audio points
			// move the first and second half
			// the first half is for the packet
			packet->SetAudioSamples(tmp, false);
			// and the second half goes into the audio store
			memcpy(audio_store[packet->GetModule()], &(tmp[160]), 320);
		}
		else /* codec_in is ECodecType::c2_3200 */
		{
			int16_t tmp[160];
			c2_32[m]->codec2_decode(tmp, packet->GetM17Data());
			packet->SetAudioSamples(tmp, false);
		}
	}
	// the only thing left is to encode the two ambe, so push the packet onto both AMBE queues
	ambe_queue.push(packet);
	ambe2_queue.push(packet);
	imbe_queue.push(packet);
	usrp_queue.push(packet);
}

void CController::ProcessC2Thread()
{
	while (keep_running)
	{
		auto packet = codec2_queue.pop();

		switch (packet->GetCodecIn())
		{
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
				// this is an original M17 packet, so decode it to audio
				// Codec2toAudio will send it on for AMBE processing
				Codec2toAudio(packet);
				break;

			case ECodecType::dstar:
			case ECodecType::dmr:
			case ECodecType::p25:
			case ECodecType::usrp:
				// codec_in was AMBE, so we need to calculate the the M17 data
				AudiotoCodec2(packet);
				break;
		}
	}
}

void CController::AudiotoAMBE(std::shared_ptr<CTranscoderPacket> packet)
{
	uint8_t ambe[9];
	const int16_t *p = packet->GetAudioSamples();

	if (dstar_in_num != 256)
	{
		int16_t tmp[160];
		for(int i = 0; i < 160; ++i)
			tmp[i] = int16_t((p[i] * dstar_in_num) >> 8);
		ambevocoder.encode_2400x1200(tmp, ambe);
	}
	else
		ambevocoder.encode_2400x1200((int16_t *)p, ambe);

	packet->SetDStarData(ambe);

	// we might be all done...
	send_mux.lock();
	if (packet->AllCodecsAreSet() && packet->HasNotBeenSent()) SendToReflector(packet);
	send_mux.unlock();
}

void CController::AMBEtoAudio(std::shared_ptr<CTranscoderPacket> packet)
{
	int16_t tmp[160];
	uint8_t ambe[9];
	ambevocoder.decode_2400x1200(tmp, (uint8_t *)packet->GetDStarData());
	if (dstar_out_num != 256)
	{
		for (int i=0; i<160; i++)
			tmp[i] = (tmp[i] * dstar_out_num) >> 8;
	}
	packet->SetAudioSamples(tmp, false);
	codec2_queue.push(packet);
	ambe2_queue.push(packet);
	imbe_queue.push(packet);
	usrp_queue.push(packet);
}

void CController::ProcessAMBEThread()
{
	while (keep_running)
	{
		auto packet = ambe_queue.pop();

		switch (packet->GetCodecIn())
		{
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
			case ECodecType::dmr:
			case ECodecType::p25:
			case ECodecType::usrp:
				AudiotoAMBE(packet);
				break;

			case ECodecType::dstar:
				AMBEtoAudio(packet);
				break;
		}
	}
}

void CController::AudiotoAMBE2(std::shared_ptr<CTranscoderPacket> packet)
{
	uint8_t ambe[9];
	const int16_t *p = packet->GetAudioSamples();

	if (ambe_in_num != 256)
	{
		int16_t tmp[160];
		for(int i = 0; i < 160; ++i)
			tmp[i] = int16_t((p[i] * ambe_in_num) >> 8);
		ambevocoder.encode_2450x1150(tmp, ambe);
	}
	else
		ambevocoder.encode_2450x1150((int16_t *)p, ambe);

	packet->SetDMRData(ambe);

	// we might be all done...
	send_mux.lock();
	if (packet->AllCodecsAreSet() && packet->HasNotBeenSent()) SendToReflector(packet);
	send_mux.unlock();
}

void CController::AMBE2toAudio(std::shared_ptr<CTranscoderPacket> packet)
{
	int16_t tmp[160];
	uint8_t ambe[9];
	ambevocoder.decode_2450x1150(tmp, (uint8_t *)packet->GetDMRData());
	if (ambe_out_num != 256)
	{
		for (int i=0; i<160; i++)
			tmp[i] = (tmp[i] * ambe_out_num) >> 8;
	}
	packet->SetAudioSamples(tmp, false);
	codec2_queue.push(packet);
	imbe_queue.push(packet);
	ambe_queue.push(packet);
	usrp_queue.push(packet);
}

void CController::ProcessAMBE2Thread()
{
	while (keep_running)
	{
		auto packet = ambe2_queue.pop();

		switch (packet->GetCodecIn())
		{
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
			case ECodecType::dstar:
			case ECodecType::p25:
			case ECodecType::usrp:
				AudiotoAMBE2(packet);
				break;

			case ECodecType::dmr:
				AMBE2toAudio(packet);
				break;
		}
	}
}

void CController::AudiotoIMBE(std::shared_ptr<CTranscoderPacket> packet)
{
	uint8_t imbe[11];

	p25vocoder.encode_4400((int16_t *)packet->GetAudioSamples(), imbe);
	packet->SetP25Data(imbe);
	// we might be all done...
	send_mux.lock();
	if (packet->AllCodecsAreSet() && packet->HasNotBeenSent()) SendToReflector(packet);
	send_mux.unlock();
}

void CController::IMBEtoAudio(std::shared_ptr<CTranscoderPacket> packet)
{
	int16_t tmp[160] = { 0 };
	p25vocoder.decode_4400(tmp, (uint8_t*)packet->GetP25Data());
	packet->SetAudioSamples(tmp, false);
	ambe_queue.push(packet);
	codec2_queue.push(packet);
	ambe2_queue.push(packet);
	usrp_queue.push(packet);
}

void CController::ProcessIMBEThread()
{
	while (keep_running)
	{
		auto packet = imbe_queue.pop();

		switch (packet->GetCodecIn())
		{
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
			case ECodecType::dstar:
			case ECodecType::dmr:
			case ECodecType::usrp:
				AudiotoIMBE(packet);
				break;

			case ECodecType::p25:
				IMBEtoAudio(packet);
				break;
		}
	}
}

void CController::AudiotoUSRP(std::shared_ptr<CTranscoderPacket> packet)
{
	const int16_t *p = packet->GetAudioSamples();

	if (usrp_tx_num != 256)
	{
		int16_t tmp[160];
		for(int i = 0; i < 160; ++i)
			tmp[i] = int16_t((p[i] * usrp_tx_num) >> 8);
		packet->SetUSRPData(tmp);
	}
	else
		packet->SetUSRPData(p);


	// we might be all done...
	send_mux.lock();
	if (packet->AllCodecsAreSet() && packet->HasNotBeenSent()) SendToReflector(packet);
	send_mux.unlock();
}

void CController::USRPtoAudio(std::shared_ptr<CTranscoderPacket> packet)
{
	const int16_t *p = packet->GetUSRPData();

	if (usrp_rx_num != 256)
	{
		int16_t tmp[160];
		for(int i = 0; i < 160; ++i)
			tmp[i] = int16_t((p[i] * usrp_rx_num) >> 8);
		packet->SetAudioSamples(tmp, false);
	}
	else
		packet->SetAudioSamples(p, false);
		
	codec2_queue.push(packet);
	ambe_queue.push(packet);
	ambe2_queue.push(packet);
	imbe_queue.push(packet);
}

void CController::ProcessUSRPThread()
{
	while (keep_running)
	{
		auto packet = usrp_queue.pop();

		switch (packet->GetCodecIn())
		{
			case ECodecType::c2_1600:
			case ECodecType::c2_3200:
			case ECodecType::dstar:
			case ECodecType::dmr:
			case ECodecType::p25:
				AudiotoUSRP(packet);
				break;

			case ECodecType::usrp:
				USRPtoAudio(packet);
				break;
		}
	}
}

void CController::SendToReflector(std::shared_ptr<CTranscoderPacket> packet)
{
	// send the packet over the socket
	while (tcClient.Send(packet->GetTCPacket()))
	{
		tcClient.ReConnect();
	}
	packet->Sent();
}

void CController::Dump(const std::shared_ptr<CTranscoderPacket> p, const std::string &title) const
{
	std::stringstream line;
	line << title << " Mod='" << p->GetModule() << "' SID=" << std::showbase << std::hex << ntohs(p->GetStreamId()) << std::noshowbase;

	ECodecType in = p->GetCodecIn();
	if (p->DStarIsSet())
		line << " DStar";
	if (ECodecType::dstar == in)
		line << '*';
	if (p->DMRIsSet())
		line << " DMR";
	if (ECodecType::dmr == in)
		line << '*';
	if (p->M17IsSet())
		line << " M17";
	if (ECodecType::c2_1600 == in)
		line << "**";
	else if (ECodecType::c2_3200 == in)
		line << '*';
	if (p->P25IsSet())
		line << " P25";
	if (ECodecType::p25 == in)
		line << "*";
	if (p->USRPIsSet())
		line << " USRP";
	if (ECodecType::usrp == in)
		line << "*";
	if (p->IsSecond())
		line << " IsSecond";
	if (p->IsLast())
		line << " IsLast";

	std::cout << line.str() << std::dec << std::endl;
}
