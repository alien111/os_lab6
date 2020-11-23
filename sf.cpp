#include "sf.h"
bool send_msg(zmq::socket_t& socket, const std::string& message) {
    zmq::message_t m(message.size());
    memcpy(m.data(), message.c_str(), message.size());
    try {
        socket.send(m);
        return true;
    } catch(...) {
        return false;
    }
}

std::string get_msg(zmq::socket_t& socket) {
    zmq::message_t message;
    bool msg_got;
    try {
        msg_got = socket.recv(&message);
    } catch(...) {
        msg_got = false;
    }
    std::string received(static_cast<char*>(message.data()), message.size());
    if(!msg_got || received.empty()) {
        return "Error: Node is unavailable";
    } else {
        return received;
    }
}

int bind_socket(zmq::socket_t& socket) {
    int port = 30000;
    std::string port_tmp = "tcp://127.0.0.1:";
    while(true) {
        try {
            socket.bind(port_tmp + std::to_string(port));
            break;
        } catch(...) {
            port++;
        }
    }
    return port;
}

void crt_node(int id, int portNumber) {
    char* arg0 = strdup("./child_node");
    char* arg1 = strdup((std::to_string(id)).c_str());
    char* arg2 = strdup((std::to_string(portNumber)).c_str());
    char* args[] = {arg0, arg1, arg2, nullptr};
    execv("./child_node", args);
}