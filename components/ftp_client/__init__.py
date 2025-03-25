import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME

DEPENDENCIES = ['network']
CODEOWNERS = ['@your-github-username']

CONF_SERVER = 'server'
CONF_MODE = 'mode'
CONF_BUFFER_SIZE = 'buffer_size'
CONF_TIMEOUT = 'timeout'

ftp_client_ns = cg.esphome_ns.namespace('ftp_client')
FTPClient = ftp_client_ns.class_('FTPClient', cg.Component)

FTP_MODES = {
    'ACTIVE': ftp_client_ns.FTPMode.ACTIVE,
    'PASSIVE': ftp_client_ns.FTPMode.PASSIVE
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FTPClient),
    cv.Required(CONF_SERVER): cv.string,
    cv.Optional(CONF_PORT, default=21): cv.port,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Optional(CONF_MODE, default='PASSIVE'): cv.enum(FTP_MODES, upper=True),
    cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(min=512, max=8192),
    cv.Optional(CONF_TIMEOUT, default=30000): cv.int_range(min=1000, max=60000),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_transfer_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_timeout_ms(config[CONF_TIMEOUT]))
