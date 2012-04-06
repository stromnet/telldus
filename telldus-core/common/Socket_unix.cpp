#include "Socket.h"

#include "Mutex.h"
#include "Strings.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#define BUFSIZE 512

using namespace TelldusCore;

int connectWrapper(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return connect(sockfd, addr, addrlen);
}

class Socket::PrivateData {
public:
	SOCKET_T socket;
	bool connected;
	fd_set infds;
	Mutex mutex;
};

Socket::Socket() {
	d = new PrivateData;
	d->socket = 0;
	d->connected = false;
	FD_ZERO(&d->infds);
}

Socket::Socket(SOCKET_T socket)
{
	d = new PrivateData;
	d->socket = socket;
	FD_ZERO(&d->infds);
	d->connected = true;
}

Socket::~Socket(void) {
	if(d->socket){
		close(d->socket);
	}
	delete d;
}

void Socket::connect(const std::wstring &server) {
	/* server is either a filesystem path, or a tcp socket.
	 * If a TCP socket, it should have format tcp://<ip>:<port>
	 */
	int isTcp = 0;
	std::string name = std::string(server.begin(), server.end());

	if(name.find("tcp://") == 0) {
		isTcp = 1;
		// Extract IP + port from name
		std::string sockspec(name, 6);
		size_t colon = sockspec.find(':');
		if(colon == std::string::npos) {
			//Log::error("Invalid socket '%s', missing :", name.c_str());
			return;
		}

		std::string hostspec(sockspec, 0, colon);
		std::string portspec(sockspec, colon + 1);
		int port = (int)strtol(portspec.c_str(), NULL, 10);
		if(port == 0) {
			//Log::error("Invalid socket '%s', cannot interpret port number", name.c_str());
			return;
		}
		if(hostspec.length() == 0) {
			//Log::error("Invalid socket '%s', missing bind IP", name.c_str());
			return;
		}

		struct sockaddr_in addr;
		d->socket = socket(AF_INET, SOCK_STREAM, 0);
		if (d->socket < 0) {
			//Log::error("Failed to create TCP socket, error %d (%s)", errno, strerror(errno));
			return;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(hostspec.c_str());

		if(connectWrapper(d->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			//Log::error("Failed to bind socket '%s', error %d: %s", name.c_str(), errno, strerror(errno));
			close(d->socket);
			return;
		}
	}else{
		// Use name as plain filename
		struct sockaddr_un remote;
		d->socket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (d->socket < 0) {
			return;
		}
		remote.sun_family = AF_UNIX;
		memset(remote.sun_path, '\0', sizeof(remote.sun_path));
		strncpy(remote.sun_path, name.c_str(), sizeof(remote.sun_path));

		int size = SUN_LEN(&remote);
		if (connectWrapper(d->socket, (struct sockaddr *)&remote, size) == -1) {
			return;
		}
	}

	TelldusCore::MutexLocker locker(&d->mutex);
	d->connected = true;
}

bool Socket::isConnected(){
	TelldusCore::MutexLocker locker(&d->mutex);
	return d->connected;
}

std::wstring Socket::read() {
	return this->read(0);
}

std::wstring Socket::read(int timeout) {
	struct timeval tv;
	char inbuf[BUFSIZE];

	FD_SET(d->socket, &d->infds);
	std::string msg;
	while(isConnected()) {
		tv.tv_sec = floor(timeout / 1000.0);
		tv.tv_usec = timeout % 1000;

		int response = select(d->socket+1, &d->infds, NULL, NULL, &tv);
		if (response == 0 && timeout > 0) {
			return L"";
		} else if (response <= 0) {
			FD_SET(d->socket, &d->infds);
			continue;
		}

		int received = BUFSIZE;
		while(received >= (BUFSIZE - 1)){
			memset(inbuf, '\0', sizeof(inbuf));
			received = recv(d->socket, inbuf, BUFSIZE - 1, 0);
			if(received > 0){
				msg.append(std::string(inbuf));
			}
		}
		if (received < 0) {
			TelldusCore::MutexLocker locker(&d->mutex);
			d->connected = false;
		}
		break;
	}

	return TelldusCore::charToWstring(msg.c_str());
}

void Socket::stopReadWait(){
	TelldusCore::MutexLocker locker(&d->mutex);
	d->connected = false;
	//TODO somehow signal the socket, if connected, here?
}

void Socket::write(const std::wstring &msg) {
	std::string newMsg(TelldusCore::wideToString(msg));
	int sent = send(d->socket, newMsg.c_str(), newMsg.length(), 0);
	if (sent < 0) {
		TelldusCore::MutexLocker locker(&d->mutex);
		d->connected = false;
	}
}
