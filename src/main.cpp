/*
 *    @author  : Maxime Chretien (MixLeNain)
 *    @mail    : mchretien@linuxmail.org
 *    @project : jusst-recruitment-challenge
 *    @summary : Juust recruitement challenge for embedded dev
 *    @version : 0.1
 */

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <signal.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

volatile sig_atomic_t stop;

void int_handler(int signum) {
    stop = 1;
}

// Sends a WebSocket message and prints the response
int main(int argc, char** argv)
{
	signal(SIGINT, int_handler);

	// This buffer will hold the incoming messages
	beast::flat_buffer buffer;

	// Set server settings
	std::string host = "localhost";
	std::string const  port = "8808";
	std::string const  target = "/ws";


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

			// Print the message
			std::cout << beast::make_printable(buffer.data()) << std::endl;

			// Clear the buffer
			buffer.clear();
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
