#include "ConnectionListener.h"
#include "Socket.h"
#include "Log.h"

#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <stdio.h>
#include <unistd.h>
#include <errno.h>

class ConnectionListener::PrivateData {
public:
	TelldusCore::EventRef waitEvent;
	std::string name;
	bool running;
};

ConnectionListener::ConnectionListener(const std::wstring &name, TelldusCore::EventRef waitEvent)
{
	d = new PrivateData;
	d->waitEvent = waitEvent;

	d->name = std::string(name.begin(), name.end());
	d->running = true;

	this->start();
}

ConnectionListener::~ConnectionListener(void) {
	d->running = false;
	this->wait();
	unlink(d->name.c_str());
	delete d;
}

void ConnectionListener::run(){
	struct timeval tv = { 0, 0 };

	//Timeout for select
	SOCKET_T serverSocket;

	/* d->name is either a filesystem path, or a tcp socket.
	 * If a TCP socket, it should have format tcp://<ip>:<port>
	 */
	int isTcp = 0;

	if(d->name.find("tcp://") == 0) {
		isTcp = 1;
		// Extract IP + port from name
		std::string sockspec(d->name, 6);
		size_t colon = sockspec.find(':');
		if(colon == std::string::npos) {
			Log::error("Invalid socket '%s', missing :", d->name.c_str());
			return;
		}

		std::string hostspec(sockspec, 0, colon);
		std::string portspec(sockspec, colon + 1);
		int port = (int)strtol(portspec.c_str(), NULL, 10);
		if(port == 0) {
			Log::error("Invalid socket '%s', cannot interpret port number", d->name.c_str());
			return;
		}
		if(hostspec.length() == 0) {
			Log::error("Invalid socket '%s', missing bind IP", d->name.c_str());
			return;
		}

		struct sockaddr_in addr;
		serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocket < 0) {
			Log::error("Failed to create TCP socket, error %d (%s)", errno, strerror(errno));
			return;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(hostspec.c_str());

		if(bind(serverSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			Log::error("Failed to bind socket '%s', error %d: %s", d->name.c_str(), errno, strerror(errno));
			close(serverSocket);
			return;
		}
	}else{
		// Use name as plain filename
		struct sockaddr_un name;
		serverSocket = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (serverSocket < 0) {
			return;
		}
		name.sun_family = AF_LOCAL;
		memset(name.sun_path, '\0', sizeof(name.sun_path));
		strncpy(name.sun_path, d->name.c_str(), sizeof(name.sun_path));
		unlink(name.sun_path);
		int size = SUN_LEN(&name);
		bind(serverSocket, (struct sockaddr *)&name, size);
	}

	listen(serverSocket, 5);

	if(!isTcp) {
		//Change permissions to allow everyone
		chmod(d->name.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	}

	fd_set infds;
	FD_ZERO(&infds);
	FD_SET(serverSocket, &infds);

	while(d->running) {
		tv.tv_sec = 5;

		int response = select(serverSocket+1, &infds, NULL, NULL, &tv);
		if (response == 0) {
			FD_SET(serverSocket, &infds);
			continue;
		} else if (response < 0 ) {
			continue;
		}
		//Make sure it is a new connection
		if (!FD_ISSET(serverSocket, &infds)) {
			continue;
		}
		SOCKET_T clientSocket = accept(serverSocket, NULL, NULL);

		ConnectionListenerEventData *data = new ConnectionListenerEventData();
		data->socket = new TelldusCore::Socket(clientSocket);
		d->waitEvent->signal(data);

	}
	close(serverSocket);
}

