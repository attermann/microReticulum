#pragma once

//#include <Arduino.h>

namespace RNS {

	class Interface {

	public:
		// Interface mode definitions
		enum modes {
		MODE_FULL           = 0x01,
		MODE_POINT_TO_POINT = 0x02,
		MODE_ACCESS_POINT   = 0x03,
		MODE_ROAMING        = 0x04,
		MODE_BOUNDARY       = 0x05,
		MODE_GATEWAY        = 0x06,
		};

		// Which interface modes a Transport Node
		// should actively discover paths for.
		//zDISCOVER_PATHS_FOR  = [MODE_ACCESS_POINT, MODE_GATEWAY]

		public:
		Interface();
		~Interface();

	};

}