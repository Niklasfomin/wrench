/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * @brief Simple macros for doing color
 */

#include <string>
#include <simgrid/s4u/Actor.hpp>
#include <iostream>
#include "ColorLogging.h"

namespace wrench {

		std::map<simgrid::s4u::ActorPtr , std::string> ColorLogging::colormap;


		void ColorLogging::beginThisProcessColor() {
					// Comment this line out to do no colors
					std::cerr << ColorLogging::getThisProcessLoggingColor();
		}

		void ColorLogging::endThisProcessColor() {
			std::cerr << "\033[0m";
		}

		static void endThisProcessColor();

		void ColorLogging::setThisProcessLoggingColor(std::string color) {
			ColorLogging::colormap[simgrid::s4u::Actor::self()] = color;
		}

		std::string ColorLogging::getThisProcessLoggingColor() {
			if (ColorLogging::colormap.find(simgrid::s4u::Actor::self()) != ColorLogging::colormap.end()) {
				return ColorLogging::colormap[simgrid::s4u::Actor::self()];
			} else {
				return "";
			}
		}

};
