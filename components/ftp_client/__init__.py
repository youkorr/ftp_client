import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import switch
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_SERVER, CONF_USERNAME

CODEOWNERS = ["@your_github_username"]

ftp_ns = cg.esphome_ns.namespace('ftp')
FTPClient = ftp_ns.class_('FTPClient', cg.Component)
FTPDownloadSwitch = ftp_ns.class_('FTPDownloadSwitch', switch.Switch, cg.Component)

CONF_FILES = 'files'
CONF_SOURCE = 'source'
CONF_FILE_ID = 'id'

FTP_FILE_SCHEMA = cv.Schema({
    cv.Required(CONF_SOURCE): cv.string,
    cv.Required(CONF_FILE_ID): cv.string,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FTPClient),
    cv.Required(CONF_SERVER): cv.string,
    cv.Optional(CONF_PORT, default=21): cv.port,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Optional(CONF_FILES): cv.ensure_list(FTP_FILE_SCHEMA),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    
    if CONF_FILES in config:
        for file_config in config[CONF_FILES]:
            source = file_config[CONF_SOURCE]
            file_id = file_config[CONF_FILE_ID]
            cg.add(var.add_file(source, file_id))

