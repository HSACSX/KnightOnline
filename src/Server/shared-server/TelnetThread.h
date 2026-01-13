#ifndef SERVER_SHAREDSERVER_TELNETTHREAD_H
#define SERVER_SHAREDSERVER_TELNETTHREAD_H

#pragma once

#include <unordered_set>
#include <shared/Thread.h>

class TelnetClientThread;
class CIni;
class TelnetThread : public Thread
{
	friend class TelnetClientThread;

public:
	TelnetThread(uint16_t port, std::unordered_set<std::string> clientAcceptList);
	~TelnetThread() override;

protected:
	/// \brief The main thread loop for the telnet server
	void thread_loop() override;

	/// \brief Shuts down resources managed by this thread
	void before_shutdown() override;

	/// \brief Telnet client thread pool
	std::shared_ptr<asio::thread_pool> _workerPool;

	/// \brief Client connection map.  Key value is the socket identifier
	std::unordered_map<uint32_t, std::shared_ptr<TelnetClientThread>> _telnetThreadMap;

private:
	/// \brief Attempts to start the _acceptor
	/// \return true if successful; false otherwise
	bool Listen();

	/// \brief Starts or continues an asynchronous acceptor chain
	void AsyncAccept();

	/// \brief Checks if a client's address is on the accept list
	/// \return true if the address is on the accept list; false otherwise
	bool isAcceptAddress(asio::ip::tcp::socket& clientSocket) const;

	/// \brief List of addresses to accept client connections from
	std::unordered_set<std::string> _clientAcceptList;

	/// \brief The Telnet listen port
	uint16_t _port;

	/// \brief Context object
	asio::io_context _io;

	/// \brief Number of threads in the thread pool
	uint32_t _workerThreadCount;

	/// \brief Acceptor socket
	std::unique_ptr<asio::ip::tcp::acceptor> _acceptor;

	/// \brief Used to track client socketIds
	uint32_t _nextSocketId;
};

#endif //SERVER_SHAREDSERVER_TELNETTHREAD_H
