#include <iostream>
#include <string>
#include <zmq.hpp>
#include <vector>
#include <csignal>
#include <sstream>
#include <memory>
#include <unordered_map>
#include "sf.h"

struct Node {
    Node(int id, std::weak_ptr<Node> parent) : id(id), parent(parent) {};
    int id;
    std::weak_ptr<Node> parent;
    std::unordered_map<int, std::shared_ptr<Node>> children;
    std::unordered_map<std::string, int> dictionary;
};

class General_tree {
public:
    bool insert(int node_id, int parent_id) {
        if(root == nullptr) {
            root = std::make_shared<Node>(node_id, std::weak_ptr<Node>());
            return true;
        }
        std::vector<int> path = get_path(parent_id);
        if(path.empty()) {
            return false;
        }
        path.erase(path.begin());
        std::shared_ptr<Node> tmp = root;
        for(const auto& node : path) {
            tmp = tmp->children[node];
        }
        tmp->children[node_id] = std::make_shared<Node>(node_id, tmp);
        return true;
    }

    bool rmv(int node_id) {
        std::vector<int> path = get_path(node_id);
        if(path.empty()) {
            return false;
        }
        path.erase(path.begin());
        std::shared_ptr<Node> tmp = root;
        for(const auto& node : path) {
            tmp = tmp->children[node];
        }
        if(tmp->parent.lock()) {
            tmp = tmp->parent.lock();
            tmp->children.erase(node_id);
        } else {
            root = nullptr;
        }
        return true;
    }
    [[nodiscard]] std::vector<int> get_path(int id) const {
        std::vector<int> path;
        if(!get_node(root, id, path)) {
            return {};
        } else {
            return path;
        }
    }
    void add_dictionary(int id, std::string name, int value) {
        std::vector<int> path = get_path(id);
        path.erase(path.begin());
        std::shared_ptr<Node> tmp = root;
        for(const auto& node : path) {
            tmp = tmp->children[node];
        }
        tmp->dictionary[name] = value;
    }

    void find_dictionary(int id, std::string name) {
        std::vector<int> path = get_path(id);
        path.erase(path.begin());
        std::shared_ptr<Node> tmp = root;
        for(const auto& node : path) {
            tmp = tmp->children[node];
        }
        if (tmp->dictionary.find(name) == tmp->dictionary.end()) {
            std::cout << "'" << name << "' not found" << std::endl;
        } else {
            std::cout << tmp->dictionary[name] << std::endl;
        }
    }
private:
    bool get_node(const std::shared_ptr<Node>& current, int id, std::vector<int>& path) const {
        if(!current) {
            return false;
        }
        if(current->id == id) {
            path.push_back(current->id);
            return true;
        }
        path.push_back(current->id);
        for(const auto& node : current->children) {
            if(get_node(node.second, id, path)) {
                return true;
            }
        }
        path.pop_back();
        return false;
    }
    std::shared_ptr<Node> root = nullptr;
};

int main() {
    General_tree tree;
    std::string command;
    int child_pid = 0;
    int child_id = 0;
    zmq::context_t ctx(1);
    zmq::socket_t rule_socket(ctx, ZMQ_REQ);
    rule_socket.setsockopt(ZMQ_SNDTIMEO, 5000);
    rule_socket.setsockopt(ZMQ_LINGER, 5000);
    rule_socket.setsockopt(ZMQ_RCVTIMEO, 5000);
    rule_socket.setsockopt(ZMQ_REQ_CORRELATE, 1);
    rule_socket.setsockopt(ZMQ_REQ_RELAXED, 1);
    int port_n = bind_socket(rule_socket);
    while(std::cin >> command) {
        if(command == "create") {
            int node_id, parent_id;
            std::string result;
            std::cin >> node_id >> parent_id;
            if(!child_pid) {
                child_pid = fork();
                if(child_pid == -1) {
                    std::cout << "Unable to create process" << std::endl;
                    exit(-1);
                } else if(child_pid == 0) {
                    crt_node(node_id, port_n);
                } else {
                    parent_id = 0;
                    child_id = node_id;
                    send_msg(rule_socket, "pid");
                    result = get_msg(rule_socket);
                }
            } else {
                if(!tree.get_path(node_id).empty()) {
                    std::cout << "Error: Already exists" << std::endl;
                    continue;
                }
                std::vector<int> path = tree.get_path(parent_id);
                if(path.empty()) {
                    std::cout << "Error: Parent not found" << std::endl;
                    continue;
                }
                path.erase(path.begin());
                std::stringstream s;
                s << "create " << path.size();
                for(int id : path) {
                    s << " " << id;
                }
                s << " " << node_id;
                send_msg(rule_socket, s.str());
                result = get_msg(rule_socket);
            }

            if(result.substr(0, 2) == "Ok") {
                tree.insert(node_id, parent_id);
            }
            std::cout << result << std::endl;
        } else if(command == "remove") {
            if(child_pid == 0) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            int node_id;
            std::cin >> node_id;
            if(node_id == child_id) {
                send_msg(rule_socket, "kill");
                get_msg(rule_socket);
                kill(child_pid, SIGTERM);
                kill(child_pid, SIGKILL);
                child_id = 0;
                child_pid = 0;
                std::cout << "Ok" << std::endl;
                tree.rmv(node_id);
                continue;
            }
            std::vector<int> path = tree.get_path(node_id);
            if(path.empty()) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            path.erase(path.begin());
            std::stringstream s;
            s << "remove " << path.size() - 1;
            for(int i : path) {
                s << " " << i;
            }
            send_msg(rule_socket, s.str());
            std::string recieved = get_msg(rule_socket);
            if(recieved.substr(0, 2) == "Ok") {
                tree.rmv(node_id);
            }
            std::cout << recieved << std::endl;
        } else if(command == "exec") {
            if(child_pid == 0) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            int node_id;
            std::cin >> node_id;
            std::string name_value;
            std::getline(std::cin, name_value);
            std::vector<int> path = tree.get_path(node_id);
            if(path.empty()) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            path.erase(path.begin());
            std::stringstream s;
            s << "exec " << path.size();
            for(int i : path) {
                s << " " << i;
            }
            std::string received;
            if(!send_msg(rule_socket, s.str())) {
                received = "Node is unavailable";
            } else {
                received = get_msg(rule_socket);
                if (received == "Node is available") {
                    std::string name;
                    int value;
                    int size_arguments = name_value.size();
                    std::stringstream ss(name_value);
                    bool searchNeeded = true;
                    for (int i = 1; i < size_arguments; ++i) {
                        if (name_value[i] == ' ') {
                            ss >> name;
                            ss >> value;
                            tree.add_dictionary(node_id, name, value);
                            std::cout << "Ok:" << node_id << std::endl;
                            searchNeeded = false;
                            break;
                        }
                    }
                    if (searchNeeded) {
                        ss >> name;
                        std::cout << "Ok:" << node_id << ": ";
                        tree.find_dictionary(node_id, name);
                    }
                } else {
                    std::cout << received << std::endl;
                }
            }
        } else if(command == "ping") {
            if(child_pid == 0) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            int node_id;
            std::cin >> node_id;
            std::vector<int> path = tree.get_path(node_id);
            if(path.empty()) {
                std::cout << "Error: Not found" << std::endl;
                continue;
            }
            path.erase(path.begin());
            std::stringstream s;
            s << "ping " << path.size();
            for(int i : path) {
                s << " " << i;
            }
            std::string received;
            if(!send_msg(rule_socket, s.str())) {
                received = "Node is unavailable";
            } else {
                received = get_msg(rule_socket);
            }
            std::cout << received << std::endl;
        } else if(command == "exit") {
            send_msg(rule_socket, "kill");
            get_msg(rule_socket);
            kill(child_pid, SIGTERM);
            kill(child_pid, SIGKILL);
            break;
        } else {
            std::cout << "Unknown command" << std::endl;
        }
        command.clear();
    }
    return 0;
}