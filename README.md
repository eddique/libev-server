# libev-server
An exercise in building an event driven TCP server with libev

## Quick Start

```sh
make && ./bin/libev-server 8000
```

```sh
make clean && make && ./bin/libev-server 8000
```

## Simple Benchmarking

### 100 Concurrent Requests

```sh
for i in {1..100};do
    curl http://localhost:8000 &;
    echo "";
done
```

### 10,000 Consecutive Requests

```sh
for i in {1..10000};do
    curl http://localhost:8000;
    echo "";
done
```