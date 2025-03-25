#include "ftp_client.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace esphome {
namespace ftp_client {

static const char* const TAG = "FTPClient";

FTPClient::FTPClient() : server_addr_{}, host_entry_{nullptr}, control_socket_{-1}, data_socket_{-1}, connected_{false} {
    // Initialize network-related structures
    //memset(&server_addr_, 0, sizeof(server_addr_));  // Initialisation via la liste d'initialisation
    //server_addr_ = {}; // C++11 et supérieur
}

FTPClient::~FTPClient() {
    disconnect();
}

void FTPClient::setup() {
    ESP_LOGCONFIG(TAG, "Setting up FTP Client...");
    dump_config();

    // Télécharger les fichiers configurés dans le YAML
    for (const auto& file_info : files_) {
        ESP_LOGI(TAG, "Downloading file: %s from %s", file_info.file_id.c_str(), file_info.source.c_str());
        // Extraire le nom du fichier local à partir de l'URL FTP
        size_t last_slash_pos = file_info.source.find_last_of('/');
        std::string local_file_name;
        if (last_slash_pos != std::string::npos) {
            local_file_name = file_info.source.substr(last_slash_pos + 1);
        } else {
            local_file_name = file_info.file_id; // Si pas de slash, utiliser l'ID
        }

        // Utiliser /config/ comme répertoire de destination
        std::string local_path = "/config/" + local_file_name;
        ESP_LOGI(TAG, "Local path: %s", local_path.c_str());

        // Télécharger le fichier
        if (connect()) {
            if (!download_file(file_info.source, local_path)) {
                ESP_LOGE(TAG, "Failed to download file: %s", file_info.source.c_str());
            }
            disconnect();
        } else {
            ESP_LOGE(TAG, "Failed to connect to FTP server to download file: %s", file_info.source.c_str());
        }
    }
}

void FTPClient::loop() {
    // Vous pouvez ajouter une logique de boucle ici si nécessaire
    // Par exemple, vérifier la connexion ou effectuer des tâches périodiques
}

void FTPClient::dump_config() {
    ESP_LOGCONFIG(TAG, "  FTP Server: %s", server_.c_str());
    ESP_LOGCONFIG(TAG, "  FTP Port: %u", port_);
    ESP_LOGCONFIG(TAG, "  FTP Username: %s", username_.c_str());
    ESP_LOGCONFIG(TAG, "  FTP Mode: %s", mode_ == FTPMode::PASSIVE ? "PASSIVE" : "ACTIVE");
    ESP_LOGCONFIG(TAG, "  Transfer Buffer Size: %zu", transfer_buffer_size_);
    ESP_LOGCONFIG(TAG, "  Timeout (ms): %u", timeout_ms_);

    ESP_LOGCONFIG(TAG, "  Files to download:");
    for (const auto& file_info : files_) {
        ESP_LOGCONFIG(TAG, "    - Source: %s, ID: %s", file_info.source.c_str(), file_info.file_id.c_str());
    }
}

void FTPClient::add_file(const std::string& source, const std::string& file_id) {
    // Vous pouvez stocker ces informations si nécessaire
    files_.push_back({source, file_id});
}
void FTPClient::set_server(const std::string& server) {
    server_ = server;
}

void FTPClient::set_port(uint16_t port) {
    port_ = port;
}

void FTPClient::set_username(const std::string& username) {
    username_ = username;
}

void FTPClient::set_password(const std::string& password) {
    password_ = password;
}

void FTPClient::set_mode(FTPMode mode) {
    mode_ = mode;
}

void FTPClient::set_transfer_buffer_size(size_t size) {
    transfer_buffer_size_ = size;
}

void FTPClient::set_timeout_ms(uint32_t timeout) {
    timeout_ms_ = timeout;
}

void FTPClient::set_error(FTPError error) {
    last_error_ = error;
    ESP_LOGE(TAG, "FTP Error: %d - %s", static_cast<int>(error), get_error_message().c_str());
}

void FTPClient::clear_error() {
    last_error_ = FTPError::NONE;
}

std::string FTPClient::get_error_message() const {
    switch (last_error_) {
        case FTPError::CONNECTION_FAILED:
            return "Failed to connect to FTP server";
        case FTPError::LOGIN_FAILED:
            return "Login failed";
        case FTPError::FILE_NOT_FOUND:
            return "File not found on server";
        case FTPError::TRANSFER_ERROR:
            return "File transfer error";
        case FTPError::TIMEOUT:
            return "Connection timeout";
        case FTPError::NETWORK_ERROR:
            return "Network error";
        case FTPError::SOCKET_ERROR:
            return "Socket error";
        case FTPError::INVALID_RESPONSE:
            return "Invalid response from server";
        default:
            return "No error";
    }
}

bool FTPClient::resolve_hostname() {
    // Resolve hostname to IP address
    if (server_.empty()) {
        ESP_LOGE(TAG, "Server address is empty!");
        set_error(FTPError::CONNECTION_FAILED);
        return false;
    }

    ip_addr_t ip;
    err_t err = dns_gethostbyname(server_.c_str(), &ip, nullptr, nullptr);
    if (err == ERR_OK) {
        ESP_LOGD(TAG, "Resolved IP address: %s", ipaddr_ntoa(&ip));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port_);
        server_addr_.sin_addr.s_addr = ip.addr;  // Use resolved IP address
        return true;
    } else {
        ESP_LOGE(TAG, "DNS resolution failed: %d", err);
        set_error(FTPError::CONNECTION_FAILED);
        return false;
    }
}

bool FTPClient::create_control_socket() {
    // Create socket
    control_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (control_socket_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        set_error(FTPError::SOCKET_ERROR);
        return false;
    }
    ESP_LOGD(TAG, "Control socket created: %d", control_socket_);

    // Set socket options
    struct timeval timeout;
    timeout.tv_sec = timeout_ms_ / 1000;
    timeout.tv_usec = (timeout_ms_ % 1000) * 1000;

    if (setsockopt(control_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "Failed to set socket timeout: %d", errno);
    }

    // Use ::connect to explicitly call the system connect function
    if (::connect(control_socket_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
        ESP_LOGE(TAG, "Connect failed: %d", errno);
        set_error(FTPError::CONNECTION_FAILED);
        close(control_socket_);
        control_socket_ = -1;
        return false;
    }
    ESP_LOGD(TAG, "Connected to server");

    return true;
}

bool FTPClient::read_response(int socket, std::string& response, uint32_t timeout_ms) {
    response.clear();
    char buffer[512];
    int bytes_received;
    
    // Configuration du timeout pour la réception
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        response = buffer;
        ESP_LOGD(TAG, "Response: %s", response.c_str());
        return true;
    } else {
        if (bytes_received == 0) {
            ESP_LOGW(TAG, "Connection closed by server");
            set_error(FTPError::CONNECTION_FAILED);
        } else {
            ESP_LOGE(TAG, "Receive failed: %d", errno);
            set_error(FTPError::NETWORK_ERROR);
        }
        return false;
    }
}

bool FTPClient::send_command(const std::string& command, std::string& response) {
    ESP_LOGD(TAG, "Sending command: %s", command.c_str());
    if (send(control_socket_, command.c_str(), command.length(), 0) < 0) {
        ESP_LOGE(TAG, "Send failed: %d", errno);
        set_error(FTPError::NETWORK_ERROR);
        return false;
    }

    return read_response(control_socket_, response);
}

bool FTPClient::connect() {
    clear_error();
    connected_ = false;

    // Resolve hostname
    if (!resolve_hostname()) {
        return false;
    }

    // Create control socket
    if (!create_control_socket()) {
        return false;
    }

    // Read welcome message
    std::string response;
    if (!read_response(control_socket_, response)) {
        disconnect();
        return false;
    }

    // Check welcome message
    if (response.empty() || strncmp(response.c_str(), "220", 3) != 0) {
        ESP_LOGE(TAG, "Invalid welcome message: %s", response.c_str());
        set_error(FTPError::CONNECTION_FAILED);
        disconnect();
        return false;
    }

    // Perform login
    if (!login()) {
        disconnect();
        return false;
    }

    connected_ = true;
    return true;
}

bool FTPClient::login() {
    std::string response;

    // Send username
    std::string user_cmd = "USER " + username_ + "\r\n";
    if (!send_command(user_cmd, response)) return false;
    if (strncmp(response.c_str(), "331", 3) != 0) {
        ESP_LOGE(TAG, "Username command failed: %s", response.c_str());
        set_error(FTPError::LOGIN_FAILED);
        return false;
    }

    // Send password
    std::string pass_cmd = "PASS " + password_ + "\r\n";
    if (!send_command(pass_cmd, response)) return false;
    if (strncmp(response.c_str(), "230", 3) != 0) {
        ESP_LOGE(TAG, "Password command failed: %s", response.c_str());
        set_error(FTPError::LOGIN_FAILED);
        return false;
    }

    ESP_LOGI(TAG, "Login successful");
    return true;
}

bool FTPClient::list_files(
    std::vector<std::string>& files, 
    std::function<void(size_t progress)> progress_callback
) {
    if (!prepare_data_connection()) {
        return false;
    }

    // Send LIST command
    std::string list_cmd = "LIST\r\n";
    std::string response;
    if (!send_command(list_cmd, response)) return false;

    // Read file list
    std::vector<char> buffer(transfer_buffer_size_);
    size_t file_count = 0;

    while (true) {
        int bytes_received = recv(data_socket_, buffer.data(), buffer.size() - 1, 0);
        if (bytes_received <= 0) break;

        buffer[bytes_received] = '\0';
        std::string file_list(buffer.data());

        // Parse and add files
        size_t pos = 0;
        while ((pos = file_list.find('\n')) != std::string::npos) {
            std::string filename = file_list.substr(0, pos);
            files.push_back(filename);
            file_list.erase(0, pos + 1);
            
            file_count++;
            if (progress_callback) {
                progress_callback(file_count);
            }
        }
    }

    // Close data connection
    close(data_socket_);
    data_socket_ = -1;

    return !files.empty();
}

bool FTPClient::prepare_data_connection() {
    std::string response;
    if (mode_ == FTPMode::PASSIVE) {
        // PASV mode
        std::string pasv_cmd = "PASV\r\n";
        if (!send_command(pasv_cmd, response)) return false;

        // Parse PASV response to get data port
        // Example response: 227 Entering Passive Mode (192,168,1,100,10,20)
        size_t start = response.find('(');
        size_t end = response.find(')');
        if (start == std::string::npos || end == std::string::npos) {
            ESP_LOGE(TAG, "Failed to parse PASV response: %s", response.c_str());
            set_error(FTPError::INVALID_RESPONSE);
            return false;
        }

        std::string data = response.substr(start + 1, end - start - 1);
        std::vector<int> values;
        std::stringstream ss(data);
        std::string value;

        while (std::getline(ss, value, ',')) {
            try {
                values.push_back(std::stoi(value));
            } catch (const std::invalid_argument& e) {
                ESP_LOGE(TAG, "Invalid PASV value: %s", value.c_str());
                set_error(FTPError::INVALID_RESPONSE);
                return false;
            }
        }

        if (values.size() != 6) {
            ESP_LOGE(TAG, "Invalid PASV format: %s", response.c_str());
            set_error(FTPError::INVALID_RESPONSE);
            return false;
        }

        // Reconstruct IP and port
        std::string ip_address = std::to_string(values[0]) + "." + std::to_string(values[1]) + "." +
                                 std::to_string(values[2]) + "." + std::to_string(values[3]);
        int port = values[4] * 256 + values[5];

        ESP_LOGD(TAG, "Data connection details: IP=%s, Port=%d", ip_address.c_str(), port);

        // Create data socket
        data_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (data_socket_ < 0) {
            ESP_LOGE(TAG, "Failed to create data socket: %d", errno);
            set_error(FTPError::SOCKET_ERROR);
            return false;
        }

        struct sockaddr_in data_addr;
        memset(&data_addr, 0, sizeof(data_addr));
        data_addr.sin_family = AF_INET;
        data_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip_address.c_str(), &data_addr.sin_addr) <= 0) {
            ESP_LOGE(TAG, "Invalid address: %s", ip_address.c_str());
            close(data_socket_);
            data_socket_ = -1;
            return false;
        }

        // Connect to data socket
        if (::connect(data_socket_, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
            ESP_LOGE(TAG, "Data connect failed: %d", errno);
            close(data_socket_);
            data_socket_ = -1;
            set_error(FTPError::CONNECTION_FAILED);
            return false;
        }

    } else {
        // Active mode - more complex setup
        ESP_LOGE(TAG, "Active mode not implemented");
        set_error(FTPError::NETWORK_ERROR);
        return false;
    }

    return true;
}

bool FTPClient::download_file(
    const std::string& remote_path, 
    const std::string& local_path,
    std::function<void(size_t downloaded, size_t total)> progress_callback
) {
    if (!prepare_data_connection()) {
        return false;
    }

    // Send RETR command
    std::string retr_cmd = "RETR " + remote_path + "\r\n";
    std::string response;
    if (!send_command(retr_cmd, response)) return false;
    if (strncmp(response.c_str(), "150", 3) != 0) {
        ESP_LOGE(TAG, "RETR command failed: %s", response.c_str());
        set_error(FTPError::TRANSFER_ERROR);
        return false;
    }

    // Open local file
    FILE* local_file = fopen(local_path.c_str(), "wb");
    if (!local_file) {
        ESP_LOGE(TAG, "Failed to open local file: %s", local_path.c_str());
        set_error(FTPError::TRANSFER_ERROR);
        return false;
    }

    // Transfer file
    std::vector<char> buffer(transfer_buffer_size_);
    size_t total_downloaded = 0;

    while (true) {
        int bytes_received = recv(data_socket_, buffer.data(), buffer.size(), 0);
        if (bytes_received <= 0) break;

        size_t bytes_written = fwrite(buffer.data(), 1, bytes_received, local_file);
        if (bytes_written != bytes_received) {
            ESP_LOGE(TAG, "Write to file failed: %d", errno);
            set_error(FTPError::TRANSFER_ERROR);
            fclose(local_file);
            return false;
        }

        total_downloaded += bytes_received;

        // Progress callback
        if (progress_callback) {
            progress_callback(total_downloaded, total_downloaded);
        }
    }

    fclose(local_file);
    close(data_socket_);
    data_socket_ = -1;

    // Read final response
    if (!read_response(control_socket_, response)) return false;
    if (strncmp(response.c_str(), "226", 3) != 0 && strncmp(response.c_str(), "250", 3) != 0) {
        ESP_LOGW(TAG, "Transfer not complete: %s", response.c_str());
    }

    ESP_LOGI(TAG, "File downloaded successfully: %s", local_path.c_str());
    return true;
}

void FTPClient::disconnect() {
    // Send QUIT command
    if (control_socket_ != -1) {
        std::string quit_cmd = "QUIT\r\n";
         std::string response;
        send_command(quit_cmd,response);
        
        // Close sockets
        close(control_socket_);
        control_socket_ = -1;
        ESP_LOGD(TAG, "Control socket closed");
    }

    if (data_socket_ != -1) {
        close(data_socket_);
        data_socket_ = -1;
        ESP_LOGD(TAG, "Data socket closed");
    }
    connected_ = false;
    ESP_LOGI(TAG, "Disconnected from FTP server");
}

void FTPClient::setup() {
    ESP_LOGCONFIG(TAG, "Setting up FTP Client...");
    dump_config();

    // Télécharger les fichiers configurés dans le YAML
    for (const auto& file_info : files_) {
        ESP_LOGI(TAG, "Downloading file: %s from %s", file_info.file_id.c_str(), file_info.source.c_str());
        // Extraire le nom du fichier local à partir de l'URL FTP
        size_t last_slash_pos = file_info.source.find_last_of('/');
        std::string local_file_name;
        if (last_slash_pos != std::string::npos) {
            local_file_name = file_info.source.substr(last_slash_pos + 1);
        } else {
            local_file_name = file_info.file_id; // Si pas de slash, utiliser l'ID
        }

        // Utiliser /config/ comme répertoire de destination
        std::string local_path = "/config/" + local_file_name;
        ESP_LOGI(TAG, "Local path: %s", local_path.c_str());

        // Télécharger le fichier
        if (connect()) {
            if (!download_file(file_info.source, local_path)) {
                ESP_LOGE(TAG, "Failed to download file: %s", file_info.source.c_str());
            }
            disconnect();
        } else {
            ESP_LOGE(TAG, "Failed to connect to FTP server to download file: %s", file_info.source.c_str());
        }
    }
}

void FTPClient::loop() {
    // Add any periodic tasks here
}

void FTPClient::dump_config() {
    ESP_LOGCONFIG(TAG, "  Server: %s", server_.c_str());
    ESP_LOGCONFIG(TAG, "  Port: %u", port_);
    ESP_LOGCONFIG(TAG, "  Username: %s", username_.c_str());
    ESP_LOGCONFIG(TAG, "  Mode: %s", mode_ == FTPMode::PASSIVE ? "PASSIVE" : "ACTIVE");
    ESP_LOGCONFIG(TAG, "  Transfer Buffer Size: %zu", transfer_buffer_size_);
    ESP_LOGCONFIG(TAG, "  Timeout: %u ms", timeout_ms_);

    ESP_LOGCONFIG(TAG, "  Files to download:");
    for (const auto& file_info : files_) {
        ESP_LOGCONFIG(TAG, "    - Source: %s, ID: %s", file_info.source.c_str(), file_info.file_id.c_str());
    }
}


} // namespace ftp_client
} // namespace esphome


