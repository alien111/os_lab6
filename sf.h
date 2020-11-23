#pragma once
#include <iostream>
#include <string>
#include "zmq.hpp"
#include "unistd.h"

bool send_msg(zmq::socket_t& socket, const std::string& message);

std::string get_msg(zmq::socket_t& socket);

int bind_socket(zmq::socket_t& socket);

void crt_node(int id, int portNumber);