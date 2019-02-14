#include "HttpServer.h"

int main(int argc, char** argv)
{
	// TODO 读配置文件
	int port = 6666;
	if(argc == 2) {
		port = atoi(argv[1]);
	}

	swings::HttpServer server(port);
	server.run();

	return 0;
}
