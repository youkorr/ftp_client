#include "ftp_client.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace esphome {
namespace ftp_client {

FTPClient::FTPClient() {
    // Initialize network-related structures
    memset(&server_addr_, 0, sizeof(server_addr_));
}

FTPClient::~FTPClient() {
    disconnect();
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
    ESP_LOGE(TAG, "FTP Error: %d", static_cast<int>(error));
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
        default:
            return "No error";
    }
}

bool FTPClient::resolve_hostname() {
    // Resolve hostname to IP address
    host_entry_ = gethostbyname(server_.c_str());
    if (!host_entry_) {
        set_error(FTPError::CONNECTION_FAILED);
        return false;
    }

    // Prepare server address structure
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    server_addr_.sin_addr.s_addr = *(in_addr_t*)host_entry_->h_addr;

    return true;
}

bool FTPClient::create_control_socket() {
    // Create socket
    control_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (control_socket_ < 0) {
        set_error(FTPError::NETWORK_ERROR);
        return false;
    }

    // Set socket options
    struct timeval timeout;
    timeout.tv_sec = timeout_ms_ / 1000;
    timeout.tv_usec = (timeout_ms_ % 1000) * 1000;
    setsockopt(control_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Use ::connect to explicitly call the system connect function
    if (::connect(control_socket_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
        set_error(FTPError::CONNECTION_FAILED);
        close(control_socket_);
        control_socket_ = -1;
        return false;
    }

    return true;
}

bool FTPClient::connect() {
    clear_error();

    // Resolve hostname
    if (!resolve_hostname()) {
        return false;
    }

    // Create control socket
    if (!create_control_socket()) {
        return false;
    }

    // Read welcome message
    char buffer[512];
    int bytes_received = recv(control_socket_, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        set_error(FTPError::CONNECTION_FAILED);
        disconnect();
        return false;
    }
    buffer[bytes_received] = '\0';
    ESP_LOGI(TAG, "Server welcome: %s", buffer);

    // Perform login
    return login();
}

bool FTPClient::login() {
    std::string response;
    
    // Send username
    std::string user_cmd = "USER " + username_ + "\r\n";
    send(control_socket_, user_cmd.c_str(), user_cmd.length(), 0);

    // Send password
    std::string pass_cmd = "PASS " + password_ + "\r\n";
    send(control_socket_, pass_cmd.c_str(), pass_cmd.length(), 0);

    // Read response
    char buffer[512];
    int bytes_received = recv(control_socket_, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        set_error(FTPError::LOGIN_FAILED);
        return false;
    }
    buffer[bytes_received] = '\0';

    // Check login response
    if (strstr(buffer, "230") == nullptr) {
        set_error(FTPError::LOGIN_FAILED);
        return false;
    }

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
    send(control_socket_, list_cmd.c_str(), list_cmd.length(), 0);

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
    if (mode_ == FTPMode::PASSIVE) {
        // PASV mode
        std::string pasv_cmd = "PASV\r\n";
        send(control_socket_, pasv_cmd.c_str(), pasv_cmd.length(), 0);

        // Read PASV response
        char buffer[512];
        int bytes_received = recv(control_socket_, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            set_error(FTPError::NETWORK_ERROR);
            return false;
        }
        buffer[bytes_received] = '\0';

        // Parse PASV response to get data port
        // Example response: 227 Entering Passive Mode (192,168,1,100,10,20)
        // Need to extract IP and port from the response
        // This is a simplified placeholder - you'll need to implement proper parsing
    } else {
        // Active mode - more complex setup
        // Need to bind to a local port and send PORT command
        // Not implemented in this example
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

    // Open local file
    FILE* local_file = fopen(local_path.c_str(), "wb");
    if (!local_file) {
        set_error(FTPError::TRANSFER_ERROR);
        return false;
    }

    // Send RETR command
    std::string retr_cmd = "RETR " + remote_path + "\r\n";
    send(control_socket_, retr_cmd.c_str(), retr_cmd.length(), 0);

    // Transfer file
    std::vector<char> buffer(transfer_buffer_size_);
    size_t total_downloaded = 0;

    while (true) {
        int bytes_received = recv(data_socket_, buffer.data(), buffer.size(), 0);
        if (bytes_received <= 0) break;

        size_t bytes_written = fwrite(buffer.data(), 1, bytes_received, local_file);
        if (bytes_written != bytes_received) {
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

    return true;
}

void FTPClient::disconnect() {
    // Send QUIT command
    if (control_socket_ != -1) {
        std::string quit_cmd = "QUIT\r\n";
        send(control_socket_, quit_cmd.c_str(), quit_cmd.length(), 0);
        
        // Close sockets
        close(control_socket_);
        control_socket_ = -1;
    }

    if (data_socket_ != -1) {
        close(data_socket_);
        data_socket_ = -1;
    }
}

} // namespace ftp_client
} // namespace esphome
