#include "http.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{   
    int port = 0;
    if (argc >= 2)
    {
        port = atoi(argv[1]);
    }
    http_run(port);
    return 0;
}