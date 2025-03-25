import esphome.codegen as cg
import esphome.config_validation as cv

# Define constants explicitly since import from esphome.const failed
CONF_ID = 'id'
CONF_SERVER = 'server'
CONF_USERNAME = 'username'
CONF_PASSWORD = 'password'
CONF_PORT = 'port'
CONF_MODE = 'mode'

MULTI_CONF = True
ftp_client_ns = cg.esphome_ns.namespace('ftp_client')

# Remove inheritance from StorageSource if it's no longer available
FTPClient = ftp_client_ns.class_('FTPClient', cg.Component)

# Enum for FTP modes
FTP_MODES = {
    'ACTIVE': ftp_client_ns.FTPMode.ACTIVE,
    'PASSIVE': ftp_client_ns.FTPMode.PASSIVE
}

def validate_ftp_config(config):
    """Additional validation for FTP configuration"""
    if len(config[CONF_USERNAME]) < 1:
        raise cv.Invalid("Username must not be empty")
    
    if len(config[CONF_PASSWORD]) < 1:
        raise cv.Invalid("Password must not be empty")
    
    return config

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_SERVER): cv.string,
    cv.Optional(CONF_PORT, default=21): cv.port,
    cv.Required(CONF_USERNAME): cv.string,
    cv.Required(CONF_PASSWORD): cv.string,
    cv.Required(CONF_ID): cv.declare_id(FTPClient),
    
    # Configuration options
    cv.Optional('mode', default='PASSIVE'): cv.enum(FTP_MODES, upper=True),
    cv.Optional('buffer_size', default=1024): cv.int_range(min=128, max=8192),
    cv.Optional('timeout', default=30000): cv.int_range(min=1000, max=60000),
}).extend({
    # Add validation directly in the schema extension
    cv.check_schema(validate_ftp_config)
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Set basic connection parameters
    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    
    # Set additional configuration
    cg.add(var.set_mode(config['mode']))
    cg.add(var.set_transfer_buffer_size(config['buffer_size']))
    cg.add(var.set_timeout_ms(config['timeout']))
    
    return var
