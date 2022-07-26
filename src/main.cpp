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
#include <cstdlib>
#include <iostream>
#include <string>
#include <signal.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace json = boost::json;            // from <boost/json.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

volatile sig_atomic_t stop;

void int_handler(int signum) {
    stop = 1;
}

struct led {
	std::string color = "off";
	int luminance = 0;
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

	if (dev_state.system == "error") {
		sys_led->color = "red";
		sys_led->luminance = 100;
	}
	else if (dev_state.system == "updating") {
		sys_led->color = "yellow";
		sys_led->luminance = 100;
		// TODO flashing with 1Hz
	}
	else if (dev_state.system == "booting") {
		sys_led->color = "red";
		sys_led->luminance = 10;
	}
	else if (dev_state.volume_changed) {
		sys_led->color = "white";
		sys_led->luminance = dev_state.volume;
		// TODO For 3s
		// TODO (bonus: fade out to off)
	}
	else if (dev_state.bluetooth == "pairing") {
		sys_led->color = "blue";
		sys_led->luminance = 100;
		// TODO (flashing with 2Hz)
	}
	else if (dev_state.playback == "inactive") {
		sys_led->color = "off";
	}
	else if (dev_state.playback == "playing" && dev_state.bluetooth == "connected") {
		sys_led->color = "blue";
		sys_led->luminance = 10;
	}
	else if (dev_state.playback == "playing") {
		sys_led->color = "white";
		sys_led->luminance = 10;
	}
	else if (dev_state.playback == "paused") {
		sys_led->color = "white";
		sys_led->luminance = 50;
	}

}

// Function to print current led status
void print_led_status (struct led sys_led) {
	std::string c = sys_led.color;
	std::cout << c;

	if (c != "off") {
		std::cout << "@" << sys_led.luminance;
	}

	std::cout << std::endl;
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

		while (!stop) {
			// Read a message into our buffer
			ws.read(buffer);

			// Parse json message
			json_parser(beast::buffers_to_string(buffer.data()), &device_state);

			// Clear the buffer
			buffer.clear();


			// if the device state changed, update the led status
			// and print it
			if (device_state.struct_changed) {

				update_led(device_state, &system_led);

				// Reset change flags
				device_state.struct_changed = false;
				device_state.volume_changed = false;

				print_led_status(system_led);
			}
		};

		// Close the WebSocket connection
		ws.close(websocket::close_code::normal);

		// If we get here then the connection is closed gracefully

	} catch(std::exception const& e) {
		//Print any error
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
