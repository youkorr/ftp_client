import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME

DEPENDENCIES = ['network']
CODEOWNERS = ['@your-github-username']

CONF_SERVER = 'server'
CONF_MODE = 'mode'
CONF_BUFFER_SIZE = 'buffer_size'
CONF_TIMEOUT = 'timeout'
CONF_FILES = 'files'
CONF_REMOTE_PATH = 'remote_path'
CONF_LOCAL_PATH = 'local_path'
CONF_FILE_ID = 'id'

ftp_client_ns = cg.esphome_ns.namespace('ftp_client')
FTPClientComponent = ftp_client_ns.class_('FTPClientComponent', cg.Component)

ftp_storage_ns = cg.esphome_ns.namespace('ftp_storage')
FTPStorageComponent = ftp_storage_ns.class_('FTPStorageComponent', cg.Component)

FTP_MODES = {
    'ACTIVE': ftp_client_ns.FTPMode.ACTIVE,
    'PASSIVE': ftp_client_ns.FTPMode.PASSIVE
}

def validate_ftp_file(value):
    if not isinstance(value, dict):
        raise cv.Invalid("File config must be a dictionary")
    if CONF_REMOTE_PATH not in value:
        raise cv.Invalid(f"File config must contain '{CONF_REMOTE_PATH}'")
    if CONF_LOCAL_PATH not in value:
        raise cv.Invalid(f"File config must contain '{CONF_LOCAL_PATH}'")
    if CONF_FILE_ID not in value:
        raise cv.Invalid(f"File config must contain '{CONF_FILE_ID}'")
    return value

FTP_CLIENT_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FTPClientComponent),
    cv.Required(CONF_SERVER): cv.string,
    cv.Optional(CONF_PORT, default=21): cv.port,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Optional(CONF_MODE, default='PASSIVE'): cv.enum(FTP_MODES, upper=True),
    cv.Optional(CONF_BUFFER_SIZE, default=2048): cv.int_range(min=512, max=8192),
    cv.Optional(CONF_TIMEOUT, default=30000): cv.int_range(min=1000, max=60000),
})

FTP_STORAGE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FTPStorageComponent),
    cv.Required(CONF_SERVER): cv.string,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Required(CONF_FILES): cv.ensure_list(validate_ftp_file),
})

CONFIG_SCHEMA = cv.Schema({
    cv.Optional('ftp_client'): FTP_CLIENT_SCHEMA,
    cv.Optional('ftp_storage'): FTP_STORAGE_SCHEMA,
})

async def to_code(config):
    if 'ftp_client' in config:
        ftp_config = config['ftp_client']
        var = cg.new_Pvariable(ftp_config[CONF_ID])
        await cg.register_component(var, ftp_config)
        
        cg.add(var.set_server(ftp_config[CONF_SERVER]))
        cg.add(var.set_port(ftp_config[CONF_PORT]))
        cg.add(var.set_username(ftp_config[CONF_USERNAME]))
        cg.add(var.set_password(ftp_config[CONF_PASSWORD]))
        cg.add(var.set_mode(ftp_config[CONF_MODE]))
        cg.add(var.set_transfer_buffer_size(ftp_config[CONF_BUFFER_SIZE]))
        cg.add(var.set_timeout_ms(ftp_config[CONF_TIMEOUT]))

    if 'ftp_storage' in config:
        storage_config = config['ftp_storage']
        storage = cg.new_Pvariable(storage_config[CONF_ID])
        await cg.register_component(storage, storage_config)
        
        cg.add(storage.set_server(storage_config[CONF_SERVER]))
        cg.add(storage.set_username(storage_config[CONF_USERNAME]))
        cg.add(storage.set_password(storage_config[CONF_PASSWORD]))
        
        for file in storage_config[CONF_FILES]:
            cg.add(storage.add_file(
                file[CONF_REMOTE_PATH],
                file[CONF_LOCAL_PATH],
                file[CONF_FILE_ID]
            ))
