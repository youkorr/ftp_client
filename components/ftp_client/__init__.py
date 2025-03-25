import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import storage
from esphome.const import CONF_ID

DEPENDENCIES = ['network']
AUTO_LOAD = ['ftp_client']

CONF_FTP_SERVER = 'server'
CONF_FTP_USERNAME = 'username'
CONF_FTP_PASSWORD = 'password'
CONF_FTP_PORT = 'port'
CONF_FTP_MODE = 'mode'
CONF_FTP_FILES = 'files'

ftp_storage_ns = cg.esphome_ns.namespace('ftp_storage')
FTPStorage = ftp_storage_ns.class_('FTPStorage', storage.Storage, cg.Component)

FTP_MODES = {
    'ACTIVE': ftp_storage_ns.FTPMode.ACTIVE,
    'PASSIVE': ftp_storage_ns.FTPMode.PASSIVE
}

def validate_ftp_file(value):
    if not isinstance(value, dict):
        raise cv.Invalid("File must be a dictionary")
    
    if 'source' not in value:
        raise cv.Invalid("File must have 'source' key")
    
    if not value['source'].startswith('ftp://'):
        raise cv.Invalid("Source must be an FTP URL (ftp://...)")
    
    if 'id' not in value:
        raise cv.Invalid("File must have 'id' key")
    
    return value

CONFIG_SCHEMA = storage.STORAGE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(FTPStorage),
    cv.Required(CONF_FTP_SERVER): cv.string,
    cv.Optional(CONF_FTP_PORT, default=21): cv.port,
    cv.Required(CONF_FTP_USERNAME): cv.string,
    cv.Required(CONF_FTP_PASSWORD): cv.string,
    cv.Optional(CONF_FTP_MODE, default='PASSIVE'): cv.enum(FTP_MODES, upper=True),
    cv.Required(CONF_FTP_FILES): cv.ensure_list(validate_ftp_file),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    cg.add(var.set_server(config[CONF_FTP_SERVER]))
    cg.add(var.set_port(config[CONF_FTP_PORT]))
    cg.add(var.set_username(config[CONF_FTP_USERNAME]))
    cg.add(var.set_password(config[CONF_FTP_PASSWORD]))
    cg.add(var.set_mode(config[CONF_FTP_MODE]))
    
    for file_config in config[CONF_FTP_FILES]:
        source = file_config['source']
        id = file_config['id']
        cg.add(var.add_file(source, id))
    
    await storage.register_storage(var, config)
