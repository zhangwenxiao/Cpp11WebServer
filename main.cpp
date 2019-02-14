#include "HttpServer.h"

int main()
{
	// TODO 读配置文件

	swings::HttpServer server(6666);
	server.run();

	return 0;
}
