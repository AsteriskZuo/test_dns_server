.PHONY: all clean build run query query_server query_method test help

# 默认参数
DEFAULT_DOMAIN = ap4-tls.agora.io
DEFAULT_SERVER = https://cloudflare-dns.com/dns-query
DEFAULT_METHOD = json

all: help build

build:
	cmake -B build -S .
	cmake --build build

clean:
	rm -rf build

help:
	@echo "DoH Client Makefile Help"
	@echo "========================"
	@echo "Available targets:"
	@echo "  make build        - Build the project"
	@echo "  make test         - Build and run tests"
	@echo "  make run          - Run with default domain ($(DEFAULT_DOMAIN))"
	@echo "  make query domain=example.com - Query specific domain"
	@echo "  make query_server domain=example.com server=https://dns.google/dns-query - Query with custom server"
	@echo "  make query_method domain=example.com method=get - Query using specific method (get, post, json)"
	@echo "  make query_full domain=example.com server=https://dns.google/dns-query method=post - Full custom query"
	@echo "  make clean        - Remove build directory"
	@echo ""
	@echo "Default values:"
	@echo "  DEFAULT_DOMAIN = $(DEFAULT_DOMAIN)"
	@echo "  DEFAULT_SERVER = $(DEFAULT_SERVER)"
	@echo "  DEFAULT_METHOD = $(DEFAULT_METHOD) (options: get, post, json)"
	@echo ""
	@echo "New features:"
	@echo "  - Structured logging with spdlog"
	@echo "  - Configuration file support (config/doh_config.json)"
	@echo "  - Enhanced error handling with custom exceptions"
	@echo "  - Unit tests with Google Test"

run: build
	./build/test_dns_server

# Run with a specific domain
# Usage: make query domain=example.com
query: build
	./build/test_dns_server --domain $(domain)

# Run with a specific domain and DNS server
# Usage: make query_server domain=example.com server=https://dns.google/dns-query
query_server: build
	./build/test_dns_server --domain $(domain) --server $(server)

# Run with a specific domain and method
# Usage: make query_method domain=example.com method=get
query_method: build
	./build/test_dns_server --domain $(domain) --server $(DEFAULT_SERVER) --method $(method)

# Run with full custom parameters
# Usage: make query_full domain=example.com server=https://dns.google/dns-query method=post
query_full: build
	./build/test_dns_server --domain $(domain) --server $(server) --method $(method)

# Build and run tests
test: build
	@echo "Running unit tests..."
	cd build && ctest --output-on-failure