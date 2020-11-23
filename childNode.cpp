#include <string>
#include <sstream>
#include <zmq.hpp>
#include <csignal>
#include <iostream>
#include <unordered_map>
#include "sf.h"
int main(int argc, char* argv[]) {
    if(argc != 3) {
        std::cerr << "Not enough parameters" << std::endl;
        exit(-1);
    }
    int id = std::stoi(argv[1]);
    int parent_port = std::stoi(argv[2]);
    zmq::context_t ctx;
    zmq::socket_t parent_socket(ctx, ZMQ_REP);
    std::string port_tmp = "tcp://127.0.0.1:";
    parent_socket.connect(port_tmp + std::to_string(parent_port));
    std::unordered_map<int, int> pids;
    std::unordered_map<int, int> ports;
    std::unordered_map<int, zmq::socket_t> sockets;
    while(true) {
        std::string action = get_msg(parent_socket);
        std::stringstream s(action);
        std::string command;
        s >> command;
        if(command == "pid") {
            std::string reply = "Ok: " + std::to_string(getpid());
            send_msg(parent_socket, reply);
        } else if(command == "create") {
            int size, node_id;
            s >> size;
            std::vector<int> path(size);
            for(int i = 0; i < size; ++i) {
                s >> path[i];
            }
            s >> node_id;
            if(size == 0) {
                auto socket = zmq::socket_t(ctx, ZMQ_REQ);
                socket.setsockopt(ZMQ_SNDTIMEO, 5000);
                socket.setsockopt(ZMQ_LINGER, 5000);
                socket.setsockopt(ZMQ_RCVTIMEO, 5000);
                socket.setsockopt(ZMQ_REQ_CORRELATE, 1);
                socket.setsockopt(ZMQ_REQ_RELAXED, 1);
                sockets.emplace(node_id, std::move(socket));
                int port = bind_socket(sockets.at(node_id));
                int pid = fork();
                if(pid == -1) {
                    send_msg(parent_socket, "Unable to fork");
                } else if(pid == 0) {
                    crt_node(node_id, port);
                } else {
                    ports[node_id] = port;
                    pids[node_id] = pid;
                    send_msg(sockets.at(node_id), "pid");
                    send_msg(parent_socket, get_msg(sockets.at(node_id)));
                }
            } else {
                int next_smb = path.front();
                path.erase(path.begin());
                std::stringstream msg;
                msg << "create " << path.size();
                for(int i : path) {
                    msg << " " << i;
                }
                msg << " " << node_id;
                send_msg(sockets.at(next_smb), msg.str());
                send_msg(parent_socket, get_msg(sockets.at(next_smb)));
            }
        } else if(command == "remove") {
            int size, node_id;
            s >> size;
            std::vector<int> path(size);
            for(int i = 0; i < size; ++i) {
                s >> path[i];
            }
            s >> node_id;
            if(path.empty()) {
                send_msg(sockets.at(node_id), "kill");
                get_msg(sockets.at(node_id));
                kill(pids[node_id], SIGTERM);
                kill(pids[node_id], SIGKILL);
                pids.erase(node_id);
                sockets.at(node_id).disconnect(port_tmp + std::to_string(ports[node_id]));
                ports.erase(node_id);
                sockets.erase(node_id);
                send_msg(parent_socket, "Ok");
            } else {
                int next_smb = path.front();
                path.erase(path.begin());
                std::stringstream msg;
                msg << "remove " << path.size();
                for(int i : path) {
                    msg << " " << i;
                }
                msg << " " << node_id;
                send_msg(sockets.at(next_smb), msg.str());
                send_msg(parent_socket, get_msg(sockets.at(next_smb)));
            }
        } else if(command == "exec") {
            int size;
            s >> size;
            std::vector<int> path(size);
            for(int i = 0; i < size; ++i) {
                s >> path[i];
            }
            if(path.empty()) {
                send_msg(parent_socket, "Node is available");
            } else {
                int next_smb = path.front();
                path.erase(path.begin());
                std::stringstream msg;
                msg << "exec " << path.size();
                for(int i : path) {
                    msg << " " << i;
                }
                std::string received;
                if(!send_msg(sockets.at(next_smb), msg.str())) {
                    received = "Node is unavailable";
                } else {
                    received = get_msg(sockets.at(next_smb));
                }
                send_msg(parent_socket, received);
            }
        } else if(command == "ping") {
            int size;
            s >> size;
            std::vector<int> path(size);
            for(int i = 0; i < size; ++i) {
                s >> path[i];
            }
            if(path.empty()) {
                send_msg(parent_socket, "Ok: 1");
            } else {
                int next_smb = path.front();
                path.erase(path.begin());
                std::stringstream msg;
                msg << "ping " << path.size();
                for(int i : path) {
                    msg << " " << i;
                }
                std::string received;
                if(!send_msg(sockets.at(next_smb), msg.str())) {
                    received = "Node is unavailable";
                } else {
                    received = get_msg(sockets.at(next_smb));
                }
                send_msg(parent_socket, received);
            }
        } else if(command == "kill") {
            for(auto& item : sockets) {
                send_msg(item.second, "kill");
                get_msg(item.second);
                kill(pids[item.first], SIGTERM);
                kill(pids[item.first], SIGKILL);
            }
            send_msg(parent_socket, "Ok");
        }
        if(parent_port == 0) {
            break;
        }
    }
}