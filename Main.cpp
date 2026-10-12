// tcd - a hybrid transcoder using DVSI hardware and Codec2 software
// Copyright © 2021,2023 Thomas A. Early N7TAE
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

#include <unistd.h>
#include <iostream>

#include "Controller.h"
#include "Configure.h"

// the global objects
CConfigure  g_Conf;
CController g_Cont;

int main(int argc, char *argv[])
{
	if (2 != argc)
	{
		std::cerr << "ERROR: Usage: " << argv[0] << " PATHTOINIFILE" << std::endl;
		return EXIT_FAILURE;
	}

	if (g_Conf.ReadData(argv[1]))
		return EXIT_FAILURE;

	if (g_Cont.Start())
		return EXIT_FAILURE;

	std::cout << "Hybrid Transcoder version 0.1.0 successfully started" << std::endl;

	pause();

	g_Cont.Stop();

	return EXIT_SUCCESS;
}
