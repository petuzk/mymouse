from doit import task_params
from doit.tools import Interactive

APP_NRF_DIRECTORY = 'app-nrf'
PROGRAMMER_HOST = 'raspberrypi.local'
PROGRAMMER_SSH_HOST = 'pi'


@task_params([{'name': 'build_dir', 'default': 'build', 'short': 'b'}])
def task_flash(build_dir):
    FILES_TO_COPY = [
        'zephyr.hex'
    ]
    zephyr_dir = f'{APP_NRF_DIRECTORY}/{build_dir}/zephyr'
    return {
        'file_dep': [f'{zephyr_dir}/{file}' for file in FILES_TO_COPY],
        'actions': [
            *(f'scp {zephyr_dir}/{file} {PROGRAMMER_SSH_HOST}:{file}' for file in FILES_TO_COPY),
            f'ssh {PROGRAMMER_SSH_HOST} ./flash.sh'
        ],
        'verbosity': 2,
    }


def task_shell():
    return {
        'actions': [Interactive(f'telnet {PROGRAMMER_HOST} 9090')]
    }


def task_rewire():
    return {
        'actions': [f'ssh {PROGRAMMER_SSH_HOST} sudo systemctl restart openocd'],
    }
