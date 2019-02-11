#include "HttpRequest.h"
#include <cassert>

using namespace swings;

HttpRequest::HttpRequest(int fd)
    : fd_(fd)
{
    assert(fd_ >= 0);
}

~HttpRequest()
{
}
