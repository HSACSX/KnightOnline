#include "pch.h"
#include "TelnetClientThread.h"
#include "AppThread.h"
#include "TelnetThread.h"

#include <db-library/Connection.h>
#include <db-library/Exceptions.h>
#include <db-library/RecordSetLoader_STLMap.h>
#include <db-library/utils.h>
#include <db-models/Full/binder/FullBinder.h>
#include <db-models/Full/model/FullModel.h>

#include <nanodbc/nanodbc.h>
#include <spdlog/spdlog.h>

#include <chrono>

using namespace std::chrono_literals;

TelnetClientThread::TelnetClientThread(TelnetThread* parent) : _clientSocket(*parent->_workerPool)
{
	asio::error_code ec;

	asio::ip::tcp::endpoint endpoint = _clientSocket.remote_endpoint(ec);
	if (ec)
	{
		spdlog::warn("TelnetClientThread::TelnetClientThread: failed remote IP lookup. socketId={}",
			_socketId);
	}
	else
	{
		_remoteIp = endpoint.address().to_string();
	}
}

TelnetClientThread::~TelnetClientThread()
{
	spdlog::debug("TelnetClientThread::~TelnetClientThread()");
}

void TelnetClientThread::thread_loop()
{
	WriteLine("User:");
	std::string username;
	do
	{
		username = ReadLine();
	}
	while (username.empty());

	WriteLine("Password:");
	std::string password;
	do
	{
		password = ReadLine();
	}
	while (password.empty());

	if (!Authenticate(username, password))
	{
		spdlog::warn("TelnetClientThread::thread_loop: failed authentication attempt for user {} "
					 "[remoteIp={}]",
			username, _remoteIp);
		_clientSocket.close();
		shutdown(false);
		return;
	}

	_username = std::move(username);

	spdlog::info(
		"TelnetClientThread::thread_loop: {} authenticated [remoteIp={}]", _username, _remoteIp);
	WriteLine("Authenticated.  Accepting commands. \"quit\" to close connection.");

	try
	{
		while (CanTick())
		{
			if (!_clientSocket.is_open())
			{
				spdlog::info(
					"TelnetClientThread::thread_loop: socket longer open [username={} remoteIp={}]",
					_username, _remoteIp);

				shutdown(false);
				continue;
			}

			std::string input = ReadLine();
			if (input.empty())
				continue;

			spdlog::info("TelnetClientThread::thread_loop: input: {} [username={} remoteIp={}]",
				input, _username, _remoteIp);

			if (input == "quit")
			{
				spdlog::info("TelnetClientThread::thread_loop: socket closed by request "
							 "[username={} remoteIp={}]",
					_username, _remoteIp);

				_clientSocket.close();
			}
			else if (input == "healthcheck")
			{
				HealthCheck();
			}
			else
			{
				AppThread* appThread = AppThread::instance();
				if (appThread == nullptr)
				{
					shutdown(false);
					continue;
				}

				if (appThread->HandleCommand(input))
					WriteLine("Input command accepted.");
				else
					WriteLine("Input command failed.");
			}
		}
	}
	catch (const std::exception& e)
	{
		spdlog::error("TelnetClientThread::thread_loop: {} [username={} remoteIp={}]", e.what(),
			_username, _remoteIp);
	}
}

void TelnetClientThread::WriteLine(const std::string& line)
{
	asio::write(_clientSocket, asio::buffer(line + "\r\n"));
}

std::string TelnetClientThread::ReadLine()
{
	constexpr auto Timeout   = 500ms;
	constexpr char Delimiter = '\n';

	std::string line;
	asio::io_context localContext;
	asio::streambuf buffer;
	asio::steady_timer timer(localContext, Timeout);
	bool isReading = false;
	bool isTimeout = false;
	std::error_code ec;

	asio::async_read_until(_clientSocket, buffer, Delimiter,
		[&](const std::error_code& err, std::size_t)
		{
			ec        = err;
			isReading = true;
		});

	timer.async_wait(
		[&](const std::error_code& err)
		{
			if (!err && !isReading)
			{
				isTimeout = true;
				_clientSocket.cancel();
			}
		});

	localContext.run();

	if (!isTimeout && !ec)
	{
		std::istream is(&buffer);
		std::getline(is, line, Delimiter);

		// strip any trailing \r
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
	}

	return line;
}

bool TelnetClientThread::Authenticate(const std::string& accountId, const std::string& password)
{
	db::SqlBuilder<full_model::TbUser> sql;
	sql.IsWherePK = true;

	try
	{
		db::ModelRecordSet<full_model::TbUser> recordSet;

		auto stmt = recordSet.prepare(sql);
		if (stmt == nullptr)
		{
			throw db::ApplicationError(
				"TelnetClientThread::Authenticate: statement could not be allocated");
		}

		stmt->bind(0, accountId.c_str());
		recordSet.execute();

		if (!recordSet.next())
			return false;

		full_model::TbUser user {};
		recordSet.get_ref(user);

		if (user.Password != password)
			return false;

		if (user.Authority == AUTHORITY_MANAGER)
			return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		db::utils::LogDatabaseError(dbErr, "TelnetClientThread::Authenticate()");
		return false;
	}

	return false;
}

void TelnetClientThread::HealthCheck()
{
	AppThread* appThread = AppThread::instance();
	if (appThread == nullptr)
	{
		WriteLine("status: STOPPED");
		return;
	}

	switch (appThread->GetAppStatus())
	{
		case AppStatus::INITIALIZING:
			WriteLine("status: INITIALIZING");
			break;

		case AppStatus::STARTING:
			WriteLine("status: STARTING");
			break;

		case AppStatus::READY:
			WriteLine("status: READY");
			break;

		case AppStatus::STOPPING:
			WriteLine("status: STOPPING");
			break;
	}
}
