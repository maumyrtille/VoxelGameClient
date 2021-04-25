#include "WebSocketServerTransport.h"

WebSocketServerTransport::WebSocketServerTransport(uint16_t port): m_port(port) {
	m_server.clear_access_channels(websocketpp::log::alevel::all);
	m_server.init_asio();
}

void WebSocketServerTransport::start() {
	m_thread = std::thread(&WebSocketServerTransport::run, this);
}

void WebSocketServerTransport::handleOpen(websocketpp::connection_hdl connection) {
	printf("Client connected\n");
}

void WebSocketServerTransport::handleClose(websocketpp::connection_hdl connection) {
	printf("Client disconnected\n");
}

void WebSocketServerTransport::handleMessage(websocketpp::connection_hdl connection, server_t::message_ptr message) {
	printf("Received %zi bytes\n", message->get_payload().size());
}

void WebSocketServerTransport::run() {
	m_server.set_open_handler(std::bind(&WebSocketServerTransport::handleOpen, this, std::placeholders::_1));
	m_server.set_close_handler(std::bind(&WebSocketServerTransport::handleClose, this, std::placeholders::_1));
	m_server.set_message_handler(std::bind(
			&WebSocketServerTransport::handleMessage,
			this,
			std::placeholders::_1,
			std::placeholders::_2
	));
	
	m_server.set_reuse_addr(true);
	m_server.listen(m_port);
	m_server.start_accept();
	
	m_server.run();
}

void WebSocketServerTransport::shutdown() {
	m_server.stop_listening();
	m_thread.join();
}
