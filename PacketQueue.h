#pragma once

//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.

// tcd - a hybrid transcoder using DVSI hardware and Codec2 software
// Copyright © 2021 Thomas A. Early N7TAE
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "TranscoderPacket.h"

// for holding CTranscoder packets while the vocoders are working their magic
// thread safe
class CPacketQueue
{
public:
	CPacketQueue() : keep_running(true) {}

	std::shared_ptr<CTranscoderPacket> pop()
	{
		std::shared_ptr<CTranscoderPacket> rval;	// the return value

		std::unique_lock<std::mutex> lock(mx);

		while (keep_running && q.empty())
			cv.wait(lock);

		if (keep_running)
		{
			rval = q.front();
			q.pop();
		}
		else
		{
			while (! q.empty())
				q.pop();
		}

		return rval;
	}

	std::size_t push(std::shared_ptr<CTranscoderPacket> item)
	{
		std::unique_lock<std::mutex> lock(mx);
		bool was_empty = q.empty();
		q.push(item);

		if (was_empty)
			cv.notify_one();

		return q.size();
	}

	bool IsEmpty()
	{
		std::lock_guard<std::mutex> lock(mx);
		return q.empty();
	}

	void Shutdown()
	{
		std::lock_guard<std::mutex> lock(mx);
		keep_running = false;
		cv.notify_all();
	}

private:
	std::mutex mx;
	std::condition_variable cv;
	std::queue<std::shared_ptr<CTranscoderPacket>> q;
	std::atomic<bool> keep_running;
};
