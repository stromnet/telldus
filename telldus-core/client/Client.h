#ifndef CLIENT_H
#define CLIENT_H

#include "Message.h"
#include "telldus-core.h"
#include "Thread.h"
#include "CallbackDispatcher.h"

namespace TelldusCore {
	class Client : public Thread
	{
	public:
		~Client(void);

		static Client *getInstance();
		static Client *getInstance(std::wstring client, std::wstring event);

		static void close();

		int registerEvent(CallbackStruct::CallbackType type, void *eventFunction, void *context );
		void stopThread(void);
		bool unregisterCallback( int callbackId );

		int getSensor(char *protocol, int protocolLen, char *model, int modelLen, int *id, int *dataTypes);
		int getController(int *controllerId, int *controllerType, char *name, int nameLen, int *available);

		bool getBoolFromService(const Message &msg);
		int getIntegerFromService(const Message &msg);
		std::wstring getWStringFromService(const Message &msg);

	protected:
			void run(void);

	private:
		Client(std::wstring client, std::wstring event);
		std::wstring sendToService(const Message &msg);

		class PrivateData;
		PrivateData *d;
		static Client *instance;
	};
}

#endif //CLIENT_H
