#pragma once

#include "esphome/core/component.h"  // Ajout pour ESPHome
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_tls.h"

#include <string>
#include <vector>
#include <functional>

namespace esphome {
namespace ftp_client {

enum class FTPMode {
    ACTIVE,
    PASSIVE
};

enum class FTPError {
    NONE,
    CONNECTION_FAILED,
    LOGIN_FAILED,
    FILE_NOT_FOUND,
    TRANSFER_ERROR,
    TIMEOUT,
    NETWORK_ERROR
};

class FTPClient : public Component {  // Ajout de Component
public:
    FTPClient();
    ~FTPClient();

    // Configuration methods
    void set_server(const std::string& server);
    void set_port(uint16_t port);
    void set_username(const std::string& username);
    void set_password(const std::string& password);
    void set_mode(FTPMode mode);
    void set_transfer_buffer_size(size_t size);
    void set_timeout_ms(uint32_t timeout);

    // Nouvelle méthode pour ajouter des fichiers
    void add_file(const std::string& source, const std::string& file_id);

    // File operations
    bool connect_to_server();  // Renommé pour éviter un conflit
    void disconnect();

    bool list_files(
        std::vector<std::string>& files, 
        std::function<void(size_t progress)> progress_callback = nullptr
    );

    bool download_file(
        const std::string& remote_path, 
        const std::string& local_path,
        std::function<void(size_t downloaded, size_t total)> progress_callback = nullptr
    );

    // Méthodes ESPHome
    void setup() override;  // Ajout de setup pour l'initialisation
    void loop() override;   // Ajout de loop pour l'exécution continue

    // Error handling
    FTPError get_last_error() const { return last_error_; }
    std::string get_error_message() const;

private:
    // Connection details
    std::string server_;
    uint16_t port_{21};
    std::string username_;
    std::string password_;
    FTPMode mode_{FTPMode::PASSIVE};

    // Network sockets
    int control_socket_{-1};
    int data_socket_{-1};

    std::vector<std::pair<std::string, std::string>> files_;

    // Error and configuration
    FTPError last_error_{FTPError::NONE};
    size_t transfer_buffer_size_{1024}; // 1 KB default buffer
    uint32_t timeout_ms_{30000}; // 30 seconds default timeout

    // Internal network methods
    bool create_control_socket();
    bool create_data_socket();
    bool resolve_hostname();

    // FTP protocol methods
    bool send_command(const std::string& cmd, std::string& response);
    bool login();
    bool prepare_data_connection();

    // Utility methods
    void set_error(FTPError error);
    void clear_error();

    // Network address storage
    struct sockaddr_in server_addr_;
    struct hostent* host_entry_{nullptr};

    // Logging tag
    static constexpr const char* TAG = "FTPClient";
};

} // namespace ftp_client
} // namespace esphome


