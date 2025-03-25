# ftp_client.h

#ifndef ESPHOME_FTP_CLIENT_H

#define ESPHOME_FTP_CLIENT_H



#include "esphome.h"

#include <WiFiClient.h>



namespace esphome {

namespace ftp_client {



class FTPClient : public Component {

 public:

  void setup() override;

  void loop() override;

  

  // Configuration du serveur FTP

  void set_server(const std::string& server) { server_ = server; }

  void set_port(uint16_t port) { port_ = port; }

  

  // Configuration des identifiants

  void set_username(const std::string& username) { username_ = username; }

  void set_password(const std::string& password) { password_ = password; }

  

  // Méthodes de gestion de la connexion

  bool connect();

  void disconnect();

  bool is_connected() const;

  

  // Méthode pour télécharger un fichier

  bool download_file(const std::string& remote_path, 

                     const std::string& local_path);



 private:

  WiFiClient client_;

  

  // Paramètres de connexion

  std::string server_;

  uint16_t port_ = 21;  // Port FTP par défaut

  std::string username_;

  std::string password_;

  

  // États internes

  bool connected_ = false;

  

  // Méthodes privées pour la gestion du protocole FTP

  bool send_command(const std::string& cmd);

  std::string read_response();

  bool login();

};



}  // namespace ftp_client

}  // namespace esphome

