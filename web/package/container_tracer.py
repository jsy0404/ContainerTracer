from abc import ABCMeta, abstractmethod
from flask_socketio import SocketIO
from threading import Thread
import ctypes
import json
import os
import copy
from .__init__ import Config


##
# @brief Runner class for container-tracer run in multithread.
# Set container-tracer options and run container-tracer.
# Print result by a interval async.
# Usage:
#   Pass the socketio for communicate with frontend at creation.
#   Set config with set_config(config).
#   Run container-tracer with run_all_container_tracer().
class ContainerTracer(metaclass=ABCMeta):
    _libc_path = "librunner.so"

    ##
    # @brief Initialize with socketio before run container-tracer.
    #
    # @param[in] socketio Want to communicate frontend socketio.
    # @param[in] config Config options from frontend.
    def __init__(self, socketio: SocketIO, config: dict) -> None:
        self.socketio = socketio
        self.libc = ctypes.CDLL(self._libc_path)
        self._set_config(config)
        self.nr_tasks = int(config["setting"]["nr_tasks"])
        self.global_config = None
        ret = self.libc.runner_init(self.config_json.encode())
        if ret != 0:
            raise OSError(ret, os.strerror(ret))

    ##
    # @brief Set proper options before running run_all_container_tracer().
    #
    # @param[in] config Config options from frontend.
    @abstractmethod
    def _set_config(self, config: dict) -> None:
        pass

    ##
    # @brief Set global configuration object by this function.
    #
    # @param Config global configuration object
    #
    # @note This method is called by `set_options()` in `web/package/main/events.py`
    def set_global_config(self, config: Config) -> None:
        self.global_config = config

    ##
    # @brief Free memory after running container-trace.
    # Must call container_tracer_free after running run_all_container_tracer().
    # If don't, Error must be occur at next phase.
    def container_tracer_free(self) -> None:
            self.libc.runner_free()

    ##
    # @brief Call runner module with config options at set_config().
    def _container_tracer_run(self) -> None:
        ret = self.libc.runner_run()
        if ret != 0:
            raise OSError(ret, os.strerror(ret))

    ##
    # @brief Receive container-tracer interval result.
    # Run async with container-tracer.
    # If there are no result, pending.
    #
    # @param[in] key Certain group's key that want to receive.
    @abstractmethod
    def _get_interval_result(self, key: str) -> None:
        pass

    ##
    # @brief Check the validation of the filename.
    # And if it is not valid, then attach number 1, 2, 3...
    #
    # @param str Filename which I want to output.
    #
    # @return Valid filename string.
    @staticmethod
    def get_valid_filename(filename: str) -> str:
        if not os.path.exists(filename):
            return filename

        number = 1
        base = copy.copy(filename) # may occur overhead ?
        filename = f"{base}.{number}"

        while os.path.exists(filename):
            number += 1
            filename = f"{base}.{number}"

        return filename

    ##
    # @brief Get total result from runner library.
    def _get_total_result(self) -> None:
        key_set = set(["cgroup-" + str(i + 1) for i in range(self.nr_tasks)])
        for key in key_set:
            self.libc.runner_get_total_result.restype = ctypes.POINTER(ctypes.c_char)
            ptr = self.libc.runner_get_total_result(key.encode())
            if ptr == 0:
                raise Exception("Memory Allocation Fail!")
            ret = ctypes.cast(ptr, ctypes.c_char_p).value
            self.libc.runner_put_result_string(ptr)

            filename = self.get_valid_filename(f"{key}-total-result.json")
            with open(filename, "w") as f:
                result_string = json.loads(ret.decode())
                result_json = json.dumps(result_string, indent=4, sort_keys=True)
                f.write(result_json)

    ##
    # @brief Refresh frontend chart by a interval with container-tracer async.
    # Send result via chart module.
    @abstractmethod
    def _refresh(self) -> None:
        pass

    ##
    # @brief Send container-tracer interval result to frontend chart.
    #
    # @param[in] interval_result Container-traacer interval result
    # want to send to frontend chart.
    def _update_interval_results(self, interval_results: dict) -> None:
        if interval_results:
            self.socketio.emit("chart_data_result", interval_results)
        else:
            self.socketio.emit("chart_end")

    ##
    # @brief Run container-tracer and refresh frontend chart
    # in multithread with selected driver.
    def _container_tracer_driver(self) -> None:
        container_tracer_proc = Thread(target=self._container_tracer_run)
        refresh_proc = Thread(target=self._refresh)

        container_tracer_proc.start()
        container_tracer_proc.join()

        refresh_proc.start()
        refresh_proc.join()

        if self.global_config == None:
            raise Exception("`global_config` must be specified");

        global_config = self.global_config
        if global_config.container_tracer:
            global_config.container_tracer.container_tracer_free()
            global_config.container_tracer = None

    ##
    # @brief Run container-tracer.
    # No waiting with seperating thread.
    # Must be called with sudo, call set_config() before running this.
    def run_all_container_tracer(self) -> None:
        if os.getuid() != 0:
            raise Exception("Execute by superuser!!!")
        self.driver = Thread(target=self._container_tracer_driver)
        self.driver.start()
