/*
 *    @author  : Maxime Chretien (MixLeNain)
 *    @mail    : mchretien@linuxmail.org
 *    @project : jusst-recruitment-challenge
 *    @summary : Juust recruitement challenge for embedded dev
 *    @version : 0.3
 */

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <signal.h>
#include <thread>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace json = boost::json;            // from <boost/json.hpp>
namespace pt = boost::posix_time;	// from <boost/../posix_time.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

volatile sig_atomic_t stop;
std::mutex led_lock; // Mutex to protect the led struct

void int_handler(int signum) {
	// Force stop at the second SINGINT
	if (stop) {
		exit(1);
	}

	stop = 1;
}

struct led {
	std::string color = "off";
	int luminance = 0;
	bool fade = false; // If true, fade out led
	bool flash = false; // If true, flash the led
	bool state = false; // If flashing true means on and false means off | if fading true means fading started
	int time = 0; // Either fade out time or flash frequency (in ms)
	pt::ptime starting_time = pt::second_clock::local_time(); // Hold the starting time of an animation
};

struct state {
	std::string system;
	std::string playback;
	int volume;
	std::string bluetooth;
	bool struct_changed = false;
	bool volume_changed = false;
};

// Function parsing json message and updating the struct
void json_parser(std::string json_str, struct state* dev_state) {
	// Parse json
	json::value parsed_json = json::parse(json_str);
	json::object json_object = parsed_json.as_object();

	// Iterate the json object
	for (auto val_key_pair: json_object) {
		std::string key = val_key_pair.key();

		json::value val = val_key_pair.value();

		// Update the struct and acknowledge changes
		if (key == "system") {
			dev_state->system = val.as_string();
			dev_state->struct_changed = true;
		} else if (key == "playback") {
			dev_state->playback = val.as_string();
			dev_state->struct_changed = true;
		} else if (key == "volume") {
			dev_state->volume = val.as_int64();
			dev_state->struct_changed = true;
			dev_state->volume_changed = true;
		} else if (key == "bluetooth") {
			dev_state->bluetooth = val.as_string();
			dev_state->struct_changed = true;
		}
	}
}

// Function to update the led status depending on the device state
void update_led (struct state dev_state, struct led *sys_led) {

	// Lock the mutex before modifying the struct
	led_lock.lock();

	if (dev_state.system == "error") {
		sys_led->color = "red";
		sys_led->luminance = 100;

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}
	else if (dev_state.system == "updating") {
		sys_led->color = "yellow";
		sys_led->luminance = 100;
		sys_led->flash = true;
		sys_led->time = 1000; // 1Hz frequency = 1000ms period

		// Reset uneeded properties
		sys_led->fade = false;
	}
	else if (dev_state.system == "booting") {
		sys_led->color = "red";
		sys_led->luminance = 10;

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}
	else if (dev_state.volume_changed) {
		sys_led->color = "white";
		sys_led->luminance = dev_state.volume;
		sys_led->time = 3000;
		sys_led->fade = true;
		sys_led->state = false; // Reset fading

		// Reset uneeded properties
		sys_led->flash = false;
	}
	else if (dev_state.bluetooth == "pairing") {
		sys_led->color = "blue";
		sys_led->luminance = 100;
		sys_led->flash = true;
		sys_led->time = 500; // 2Hz frequency = 500ms period

		// Reset uneeded properties
		sys_led->fade = false;
	}
	else if (dev_state.playback == "inactive") {
		sys_led->color = "off";

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}
	else if (dev_state.playback == "playing" && dev_state.bluetooth == "connected") {
		sys_led->color = "blue";
		sys_led->luminance = 10;

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}
	else if (dev_state.playback == "playing") {
		sys_led->color = "white";
		sys_led->luminance = 10;

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}
	else if (dev_state.playback == "paused") {
		sys_led->color = "white";
		sys_led->luminance = 50;

		// Reset uneeded properties
		sys_led->flash = false;
		sys_led->fade = false;
	}

	// Unlock the mutex after modifying the struct
	led_lock.unlock();

}

// Thread function to print current led status
void print_led_status (struct led *sys_led) {
	struct led prev_led, curr_led;

	// Remember fading starting point
	int base_luminance = 0;

	while (!stop) {
		// Lock the mutex before modifying the struct
		led_lock.lock();

		// If fading, reduce luminance
		if(sys_led->fade) {
			// Get current time
			pt::ptime now = pt::microsec_clock::local_time();

			// First run we do nothing
			if (!sys_led->state) {
				sys_led->state = true;
				base_luminance = sys_led->luminance;
				sys_led->starting_time = now;
			}
			else {
				// Compare current time with led starting time
				pt::time_duration diff = now - sys_led->starting_time;
				long diff_ms = diff.total_milliseconds();

				// Set luminance dépending on the current time position
				sys_led->luminance = base_luminance - ((diff_ms * base_luminance) / sys_led->time);

				// If we reached 0, disable fading and power off the led
				if (sys_led->luminance <= 0) {
					sys_led->fade = false;
					sys_led->color = "off";
				}
			}
		}

		if (sys_led->flash) {
			// Get current time and compare with led starting time
			pt::ptime now = pt::microsec_clock::local_time();
			pt::time_duration diff = now - sys_led->starting_time;
			long diff_ms = diff.total_milliseconds();

			// If time difference is higher, it's the first run and led is on
			if (diff_ms > sys_led->time) {
				sys_led->starting_time = now;
				sys_led->state = true;
			}
			else if (diff_ms < sys_led->time / 2) {
				// If we are under half the time, the led is powered on
				sys_led->state = true;
			}
			else { // Else the led is off
				sys_led->state = false;
			}
		}

		// Copy the current struct before releasing the mutex
		curr_led = *sys_led;

		// Unlock the mutex after modifying the struct
		led_lock.unlock();

		// If not during animation time and led is still the same, we don't update
		if((!curr_led.flash || prev_led.state == curr_led.state) &&
			prev_led.color == curr_led.color &&
			prev_led.luminance == curr_led.luminance)
		{
			// We sleep for 100 ms to avoid looping too fast
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			continue;
		}

		// If the led has a color and it's not during off state of flashing
		// we display the color and luminance
		// else we display off
		if (curr_led.color != "off" && !(curr_led.flash == true && curr_led.state == false)) {
			std::cout << curr_led.color << "@" << curr_led.luminance;
		} else {
			std::cout << "off";
		}

		std::cout << std::endl;

		// Save current led state
		prev_led = curr_led;
	}
}

// Main
int main(int argc, char** argv)
{
	signal(SIGINT, int_handler);

	// This buffer will hold the incoming messages
	beast::flat_buffer buffer;

	// Set server settings
	std::string host = "localhost";
	std::string const port = "8808";
	std::string const target = "/ws";

	// State stucture to hold values decoded from the websocket
	struct state device_state;

	// Led structure to hold system led values
	struct led system_led;

	try
	{
		// The io_context is required for all I/O
		net::io_context ioc;

		// These objects perform our I/O
		tcp::resolver resolver{ioc};
		websocket::stream<tcp::socket> ws{ioc};

		// Look up the domain name
		auto const results = resolver.resolve(host, port);

		// Make the connection on the IP address we get from a lookup
		auto ep = net::connect(ws.next_layer(), results);

		// Update the host_ string. This will provide the value of the
		// Host HTTP header during the WebSocket handshake.
		// See https://tools.ietf.org/html/rfc7230#section-5.4
		host += ':' + std::to_string(ep.port());

		// Set a decorator to change the User-Agent of the handshake
		ws.set_option(websocket::stream_base::decorator(
			[](websocket::request_type& req)
			{
				req.set(http::field::user_agent,
				std::string(BOOST_BEAST_VERSION_STRING) +
				" websocket-client-coro");
			}
		));

		// Perform the websocket handshake
		ws.handshake(host, target);

		// Spawn led status thread
		std::thread led_thread (print_led_status, &system_led);

		while (!stop) {
			// Read a message into our buffer
			ws.read(buffer);

			// Parse json message
			json_parser(beast::buffers_to_string(buffer.data()), &device_state);

			// Clear the buffer
			buffer.clear();

			// If the device state changed, update the led status
			if (device_state.struct_changed) {
				// Update the led status
				update_led(device_state, &system_led);

				// Reset change flags
				device_state.struct_changed = false;
				device_state.volume_changed = false;
			}

		};

		// Close the WebSocket connection
		ws.close(websocket::close_code::normal);

		// Join thread
		led_thread.join();

		// If we get here then the connection is closed gracefully

	} catch(std::exception const& e) {
		//Print any error
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
